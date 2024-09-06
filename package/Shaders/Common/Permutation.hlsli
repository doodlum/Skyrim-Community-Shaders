

#define _Glowmap 2
#define _Hair 6

#define _VC (1 << 0)
#define _Skinned (1 << 1)
#define _ModelSpaceNormals (1 << 2)
// flags 3 to 8 are unused
#define _Specular (1 << 9)
#define _SoftLighting (1 << 10)
#define _RimLighting (1 << 11)
#define _BackLighting (1 << 12)
#define _ShadowDir (1 << 13)
#define _DefShadow (1 << 14)
#define _ProjectedUV (1 << 15)
// (HAIR technique only)
#define _DepthWriteDecals (1 << 15)
#define _AnisoLighting (1 << 16)
#define _AmbientSpecular (1 << 17)
#define _WorldMap (1 << 18)
#define _BaseObjectIsSnow (1 << 19)
#define _DoAlphaTest (1 << 20)
#define _Snow (1 << 21)
#define _CharacterLight (1 << 22)
#define _AdditionalAlphaMask (1 << 23)

#define _NormalTexCoord (1 << 1)
#define _Reflections (1 << 2)
#define _Refractions (1 << 3)
#define _Depth (1 << 4)
#define _Interior (1 << 5)
#define _Wading (1 << 6)
#define _VertexAlphaDepth (1 << 7)
#define _Cubemap (1 << 8)
#define _Flowmap (1 << 9)
#define _BlendNormals (1 << 10)

#define _GrayscaleToColor (1 << 19)
#define _GrayscaleToAlpha (1 << 20)
#define _IgnoreTexAlpha (1 << 21)

#define _InWorld (1 << 0)
#define _IsBeastRace (1 << 1)

cbuffer PerShader : register(b4)
{
	uint VertexShaderDescriptor;
	uint PixelShaderDescriptor;
	uint ExtraShaderDescriptor;
};