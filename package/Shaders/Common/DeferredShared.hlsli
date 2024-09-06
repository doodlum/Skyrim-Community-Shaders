cbuffer PerFrameDeferredShared : register(b11)
{
	float4 BufferDim;
	float4 CameraData;
	row_major float3x4 DirectionalAmbient;
	uint FrameCount;
	uint FrameCountAlwaysActive;
};

float GetScreenDepth(float depth)
{
	return (CameraData.w / (-depth * CameraData.z + CameraData.x));
}