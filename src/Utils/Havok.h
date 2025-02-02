// functions that are related to Havok

#pragma once

namespace Util
{
	[[nodiscard]] inline RE::NiPoint3 HkVectorToNiPoint(const RE::hkVector4& vec) { return { vec.quad.m128_f32[0], vec.quad.m128_f32[1], vec.quad.m128_f32[2] }; }

	RE::NiTransform HkTransformToNiTransform(const RE::hkTransform& a_hkTransform);

}  // namespace Util
