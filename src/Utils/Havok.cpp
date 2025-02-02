#include "Havok.h"

namespace Util
{
	void HkMatrixToNiMatrix(const RE::hkMatrix3& a_hkMat, RE::NiMatrix3& a_niMat)
	{
		a_niMat.entry[0][0] = a_hkMat.col0.quad.m128_f32[0];
		a_niMat.entry[1][0] = a_hkMat.col0.quad.m128_f32[1];
		a_niMat.entry[2][0] = a_hkMat.col0.quad.m128_f32[2];
		a_niMat.entry[0][1] = a_hkMat.col1.quad.m128_f32[0];
		a_niMat.entry[1][1] = a_hkMat.col1.quad.m128_f32[1];
		a_niMat.entry[2][1] = a_hkMat.col1.quad.m128_f32[2];
		a_niMat.entry[0][2] = a_hkMat.col2.quad.m128_f32[0];
		a_niMat.entry[1][2] = a_hkMat.col2.quad.m128_f32[1];
		a_niMat.entry[2][2] = a_hkMat.col2.quad.m128_f32[2];
	}

	RE::NiTransform HkTransformToNiTransform(const RE::hkTransform& a_hkTransform)
	{
		RE::NiTransform result;
		HkMatrixToNiMatrix(a_hkTransform.rotation, result.rotate);
		result.translate = HkVectorToNiPoint(a_hkTransform.translation * RE::bhkWorld::GetWorldScaleInverse());
		result.scale = 1.f;
		return result;
	}
}  // namespace Util
