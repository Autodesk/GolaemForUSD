#include "hydraGlobals.h"

#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/base/gf/matrix4d.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace glmhydra {

HdContainerDataSourceHandle GetIdentityXformDataSource()
{
    static const HdContainerDataSourceHandle identityXform =
        HdXformSchema::Builder()
        .SetMatrix(
            HdRetainedTypedSampledDataSource<GfMatrix4d>::New(GfMatrix4d(1.0)))
        .Build();

    return identityXform;
}

HdTokenDataSourceHandle GetConstantInterpDataSource()
{
    static const HdTokenDataSourceHandle constantInterp =
        HdPrimvarSchema::BuildInterpolationDataSource(
            HdPrimvarSchemaTokens->constant);

    return constantInterp;
}

HdTokenDataSourceHandle GetUniformInterpDataSource()
{
    static const HdTokenDataSourceHandle uniformInterp =
        HdPrimvarSchema::BuildInterpolationDataSource(
            HdPrimvarSchemaTokens->uniform);

    return uniformInterp;
}

HdTokenDataSourceHandle GetFaceVaryingInterpDataSource()
{
    static const HdTokenDataSourceHandle faceVaryingInterp =
        HdPrimvarSchema::BuildInterpolationDataSource(
            HdPrimvarSchemaTokens->faceVarying);

    return faceVaryingInterp;
}

HdTokenDataSourceHandle GetVertexInterpDataSource()
{
    static const HdTokenDataSourceHandle vertexInterp =
        HdPrimvarSchema::BuildInterpolationDataSource(
            HdPrimvarSchemaTokens->vertex);

    return vertexInterp;
}

HdContainerDataSourceHandle GetMaterialDataSource(const SdfPath& material)
{
    return HdRetainedContainerDataSource::New(
        HdMaterialBindingsSchemaTokens->allPurpose,
        HdMaterialBindingSchema::Builder()
        .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(material))
        .Build());
}

/*
 * Copies a glm::Array of 3D vectors to a VtArray, resizing it as needed.
 */
void CopyGlmVecArrayToVt(
    VtVec3fArray& dst, const glm::Array<glm::Vector3>& src)
{
    size_t sz = src.size();
    dst.resize(sz);
    for (size_t i = 0; i < sz; ++i) {
        dst[i].Set(src[i].getFloatValues());
    }
}

}  // namespace glmhydra
