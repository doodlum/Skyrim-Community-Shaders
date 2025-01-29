#pragma once

namespace Hooks
{
	struct BSShader_BeginTechnique
	{
		static bool thunk(RE::BSShader* shader, uint32_t vertexDescriptor, uint32_t pixelDescriptor, bool skipPixelShader);
		static inline REL::Relocation<decltype(thunk)> func;
	};
	struct BSGraphics_SetDirtyStates
	{
		static void thunk(bool isCompute);
		static inline REL::Relocation<decltype(thunk)> func;
	};
	struct IDXGISwapChain_Present
	{
		static HRESULT WINAPI thunk(IDXGISwapChain* This, UINT SyncInterval, UINT Flags);
		static inline REL::Relocation<decltype(thunk)> func;
	};
	void Install();
	void InstallD3DHooks();
}
