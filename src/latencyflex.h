// Copyright 2021 Tatsuyuki Ishi
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LATENCYFLEX_H
#define LATENCYFLEX_H

#ifdef LATENCYFLEX_HAVE_PERFETTO
#include <perfetto.h>
PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("latencyflex").SetDescription("LatencyFleX latency and throughput metrics"));
#else
#define TRACE_COUNTER(...)
#define TRACE_EVENT_BEGIN(...)
#define TRACE_EVENT_END(...)
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace lfx {
namespace internal {
// An exponentially weighted moving average estimator.
class EwmaEstimator {
public:
  // `alpha`: Smoothing factor. Larger values means less smoothing, resulting in
  //          a bumpy but quick response.
  // `full_weight`: Set to true to disable weight correction for initial
  //                samples. The estimator will start with a value of 0 weighted
  //                at 100% instead.
  EwmaEstimator(double alpha, bool full_weight = false)
      : alpha_(alpha), current_weight_(full_weight ? 1.0 : 0.0) {}

  // Update the estimate with `value`. `value` must not be negative. If a
  // negative exponent is used, then `value` must not be too small or the
  // internal accumulator will overflow.
  void update(double value) {
    current_ = (1 - alpha_) * current_ + alpha_ * value;
    current_weight_ = (1 - alpha_) * current_weight_ + alpha_;
  }

  double get() const {
    if (current_weight_ == 0) {
      return 0;
    }
    return current_ / current_weight_;
  }

private:
  double alpha_;
  double current_ = 0;
  double current_weight_;
};
} // namespace internal

enum Phases { kUp = 0, kDown, kNumPhases };

// Tracks and computes frame time, latency and the desired sleep time before
// next tick. All time is in nanoseconds. The clock domain doesn't matter as
// long as it's a single consistent clock.
//
// Access must be externally synchronized.
class LatencyFleX {
public:
  LatencyFleX() : latency_(0.3), inv_throughtput_(0.3), proj_correction_(0.5, true) {
    std::fill(std::begin(frame_begin_ids_), std::end(frame_begin_ids_), UINT64_MAX);
  }

  // Get the desired wake-up time. Sleep until this time, then call `BeginFrame()`. This function
  // must be called *exactly once* before each call to `BeginFrame()`. Calling this the second time
  // with the same `frame_id` will corrupt the internal time tracking.
  //
  // If a wait target cannot be determined due to lack of data, then `0` is
  // returned.
  uint64_t GetWaitTarget(uint64_t frame_id) {
    if (prev_frame_end_id_ != UINT64_MAX) {
      size_t phase = frame_id % kNumPhases;
      double invtpt = inv_throughtput_.get();
      int64_t comp_to_apply = 0;
      if (frame_end_projection_base_ == UINT64_MAX) {
        frame_end_projection_base_ = prev_frame_end_ts_;
      } else {
        // The prediction error is equal to (actual latency) - (expected latency).
        // As we adapt our latency estimator to the actual latency values, this
        // will eventually converge as long as we are not constantly overpacing,
        // building a queue at a faster pace than the estimator can adapt.

        // In the section below, we attempt to apply additional compensation in
        // the case of delay increase, to prevent extra queuing as much as possible.
        int64_t prediction_error =
            (int64_t)prev_frame_end_ts_ -
            (int64_t)(frame_end_projection_base_ +
                      frame_end_projected_ts_[prev_frame_end_id_ % kMaxInflightFrames]);
        TRACE_COUNTER("latencyflex", "Prediction error", prediction_error);
        int64_t prev_comp_applied = comp_applied_[prev_frame_end_id_ % kMaxInflightFrames];
        // We need to limit the compensation to delay increase, or otherwise we would cancel out the
        // regular delay decrease from our pacing. To achieve this, we treat any early prediction as
        // having prediction error of zero.
        //
        // We also want to cancel out the counter-reaction from our previous compensation, so what
        // we essentially want here is `prediction_error_ - prev_prediction_error_ +
        // prev_comp_applied`. But since we clamp prediction_error_ and prev_prediction_error_,
        // the naive approach of adding prev_comp_applied directly would have a bias toward
        // overcompensation. Consider the example below where we're pacing at the correct (100%)
        // rate but things arrives late due to reason that are *not* queuing (noise):
        // 5ms late, 5ms late, ... (a period longer than our latency) ... , 0ms
        // We would compensate -5ms on the first frame, bringing the prediction error to 0. But when
        // the 0ms frame arrives, the prediction error becomes -5ms due to our overcompensation.
        // Due to its negativity, we don't recompensate for this decrease: this is the bias.
        //
        // The solution here is to include prev_comp_applied as a part of clamping equation, which
        // allows it to also undercompensate when it makes sense. It seems to do a great job on
        // preventing prediction error from getting stuck in a state that is drift away.
        proj_correction_.update(
            std::max(INT64_C(0), prediction_error) -
            std::max(INT64_C(0), prev_prediction_error_ - prev_comp_applied));
        prev_prediction_error_ = prediction_error;
        // Try to cancel out any unintended delay happened to previous frame start. This is
        // primarily meant for cases where a frame time spike happens and we get backpressured
        // on the main thread. prev_forced_correction_ will stay high until our prediction catches
        // up, canceling out any excessive correction we might end up doing.
        comp_to_apply = std::round(proj_correction_.get());
        comp_applied_[frame_id % kMaxInflightFrames] = comp_to_apply;
        TRACE_COUNTER("latencyflex", "Delay Compensation", comp_to_apply);
      }

      // The target wakeup time.
      uint64_t target =
          (int64_t)frame_end_projection_base_ +
          (int64_t)frame_end_projected_ts_[prev_frame_begin_id_ % kMaxInflightFrames] +
          comp_to_apply +
          (int64_t)std::round((((int64_t)frame_id - (int64_t)prev_frame_begin_id_) +
                               1 / (phase == kUp ? up_factor_ : 1) - 1) *
                                  invtpt / down_factor_ -
                              latency_.get());
      // The projection is something close to the predicted frame end time, but it is always paced
      // at down_factor * throughput, which prevents delay compensation from kicking in until it's
      // actually necessary (i.e. we're overpacing).
      uint64_t new_projection =
          (int64_t)frame_end_projected_ts_[prev_frame_begin_id_ % kMaxInflightFrames] +
          comp_to_apply +
          (int64_t)std::round(((int64_t)frame_id - (int64_t)prev_frame_begin_id_) * invtpt /
                              down_factor_);
      frame_end_projected_ts_[frame_id % kMaxInflightFrames] = new_projection;
      TRACE_EVENT_BEGIN(
          "latencyflex", "projection",
          perfetto::Track(track_base_ + frame_id % kMaxInflightFrames + kMaxInflightFrames),
          target);
      TRACE_EVENT_END(
          "latencyflex",
          perfetto::Track(track_base_ + frame_id % kMaxInflightFrames + kMaxInflightFrames),
          frame_end_projection_base_ + new_projection);
      return target;
    } else {
      return 0;
    }
  }

  // Begin the frame. Called on the main/simulation thread.
  //
  // This call must be preceded with a call to `GetWaitTarget()`.
  //
  // `target` should be the timestamp returned by `GetWaitTarget()`.
  // `timestamp` should be calculated as follows:
  // - If a sleep is not performed (because the wait target has already been
  //   passed), then pass the current time.
  // - If a sleep is performed (wait target was not in the past), then pass the
  //   wait target as-is. This allows compensating for any latency incurred by
  //   the OS for waking up the process.
  void BeginFrame(uint64_t frame_id, uint64_t target, uint64_t timestamp) {
    TRACE_EVENT_BEGIN("latencyflex", "frame",
                      perfetto::Track(track_base_ + frame_id % kMaxInflightFrames), timestamp);
    frame_begin_ids_[frame_id % kMaxInflightFrames] = frame_id;
    frame_begin_ts_[frame_id % kMaxInflightFrames] = timestamp;
    prev_frame_begin_id_ = frame_id;
    if (target != 0) {
      int64_t forced_correction = timestamp - target;
      frame_end_projected_ts_[frame_id % kMaxInflightFrames] += forced_correction;
      comp_applied_[frame_id % kMaxInflightFrames] += forced_correction;
      prev_prediction_error_ += forced_correction;
    }
  }

  // End the frame. Called from a rendering-related thread.
  //
  // The timestamp should be obtained in one of the following ways:
  // - Run a thread dedicated to wait for command buffer completion fences.
  //   Capture the timestamp on CPU when the fence is signaled.
  // - Capture a GPU timestamp when frame ends, then convert it into a clock
  //   domain on CPU (known as "timestamp calibration").
  //
  // If `latency` and `frame_time` are not null, then the latency and the frame
  // time are returned respectively, or UINT64_MAX is returned if measurement is
  // unavailable.
  void EndFrame(uint64_t frame_id, uint64_t timestamp, uint64_t *latency, uint64_t *frame_time) {
    size_t phase = frame_id % kNumPhases;
    int64_t latency_val = -1;
    int64_t frame_time_val = -1;
    if (frame_begin_ids_[frame_id % kMaxInflightFrames] == frame_id) {
      frame_begin_ids_[frame_id % kMaxInflightFrames] = UINT64_MAX;

      if (frame_time && prev_frame_end_id_ != UINT64_MAX)
        *frame_time = timestamp - prev_frame_real_end_ts_;
      prev_frame_real_end_ts_ = timestamp;
      timestamp = std::max(timestamp, prev_frame_end_ts_ + target_frame_time);
      auto frame_start = frame_begin_ts_[frame_id % kMaxInflightFrames];
      latency_val = (int64_t)timestamp - (int64_t)frame_start;
      if (phase == kDown) {
        latency_.update(latency_val);
      }
      if (latency)
        *latency = latency_val;
      TRACE_COUNTER("latencyflex", "Latency", latency_val);
      TRACE_COUNTER("latencyflex", "Latency (Estimate)", latency_.get());
      if (prev_frame_end_id_ != UINT64_MAX) {
        if (frame_id > prev_frame_end_id_) {
          auto frames_elapsed = frame_id - prev_frame_end_id_;
          frame_time_val =
              ((int64_t)timestamp - (int64_t)prev_frame_end_ts_) / (int64_t)frames_elapsed;
          frame_time_val = std::clamp(frame_time_val, INT64_C(1000000), INT64_C(50000000));
          if (phase == kUp) {
            inv_throughtput_.update(frame_time_val);
          }
          TRACE_COUNTER("latencyflex", "Frame Time", frame_time_val);
          TRACE_COUNTER("latencyflex", "Frame Time (Estimate)", inv_throughtput_.get());
        }
      }
      prev_frame_end_id_ = frame_id;
      prev_frame_end_ts_ = timestamp;
    }
    if (frame_time)
      *frame_time = frame_time_val;
    TRACE_EVENT_END("latencyflex", perfetto::Track(track_base_ + frame_id % kMaxInflightFrames),
                    timestamp);
  }

  void Reset() {
    auto new_instance = LatencyFleX();
#ifdef LATENCYFLEX_HAVE_PERFETTO
    new_instance.track_base_ = track_base_ + 2 * kMaxInflightFrames;
#endif
    new_instance.target_frame_time = target_frame_time;
    *this = new_instance;
  }

  uint64_t target_frame_time = 0;

private:
  static const std::size_t kMaxInflightFrames = 16;

  uint64_t frame_begin_ts_[kMaxInflightFrames] = {};
  uint64_t frame_begin_ids_[kMaxInflightFrames];
  uint64_t frame_end_projected_ts_[kMaxInflightFrames] = {};
  uint64_t frame_end_projection_base_ = UINT64_MAX;
  int64_t comp_applied_[kMaxInflightFrames] = {};
  uint64_t prev_frame_begin_id_ = UINT64_MAX;
  double up_factor_ = 1.10;
  double down_factor_ = 0.985;
  int64_t prev_prediction_error_ = 0;
  uint64_t prev_frame_end_id_ = UINT64_MAX;
  uint64_t prev_frame_end_ts_ = 0;
  uint64_t prev_frame_real_end_ts_ = 0;
  internal::EwmaEstimator latency_;
  internal::EwmaEstimator inv_throughtput_;
  internal::EwmaEstimator proj_correction_;

#ifdef LATENCYFLEX_HAVE_PERFETTO
  uint64_t track_base_ = 0;
#endif
};
} // namespace lfx

#endif // LATENCYFLEX_H
