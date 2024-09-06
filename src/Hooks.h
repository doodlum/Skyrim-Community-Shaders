#pragma once

namespace Hooks
{
	bool hk_BSShader_BeginTechnique(RE::BSShader* shader, uint32_t vertexDescriptor, uint32_t pixelDescriptor, bool skipPixelShader);
	void Install();
}
