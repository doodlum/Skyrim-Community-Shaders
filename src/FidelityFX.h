#pragma once

//#include <FidelityFX/host/backends/dx11/ffx_dx11.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>

#include <FidelityFX/host/ffx_brixelizer.h>

#include <FidelityFX/host/ffx_fsr3.h>
#include <FidelityFX/host/ffx_interface.h>

#include "Buffer.h"
#include "State.h"

class FidelityFX
{
public:
	static FidelityFX* GetSingleton()
	{
		static FidelityFX singleton;
		return &singleton;
	}

	FfxFsr3Context fsrContext;

	void CreateFSRResources();
	void DestroyFSRResources();
	void Upscale(Texture2D* a_color, Texture2D* a_alphaMask, float2 a_jitter, bool a_reset, float a_sharpness);
};
