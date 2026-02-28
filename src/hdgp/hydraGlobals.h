#pragma once

#include <glmArray.h>
#include <glmVector3.h>

#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/base/tf/denseHashMap.h>
#include <pxr/base/vt/array.h>

#include <memory>

namespace glmhydra {

/*
 * If true, rigid mesh entities are treated differently: a single instance of
 * FileMeshAdapter is created for a given rigid mesh, and FileMeshInstance is
 * used to add different materials, transformation matrices and custom primvars
 * for each instance.
 *
 * For now, though, this is disabled, because I don't know how to calculate the
 * transformation matrix for a mesh.
 */
static const bool kEnableRigidEntities = false;

/*
 * Type of the vector arrays found in glm::crowdio::OutputEntityGeoData. The
 * three dimensions correspond to the index of the frame being computed, the
 * index of the mesh or fur instance, and the index of the vector itself.
 */
using DeformedVectors = glm::Array<glm::Array<glm::Array<glm::Vector3>>>;

/*
 * A map of custom primvar names to their corresponding data sources, generated
 * from a crowd entity's shader and PP attributes.
 */
using PrimvarDSMap = PXR_NS::TfDenseHashMap<
    PXR_NS::TfToken, PXR_NS::HdSampledDataSourceHandle, PXR_NS::TfHash>;
using PrimvarDSMapRef = std::shared_ptr<PrimvarDSMap>;

PXR_NS::HdContainerDataSourceHandle GetIdentityXformDataSource();
PXR_NS::HdTokenDataSourceHandle GetConstantInterpDataSource();
PXR_NS::HdTokenDataSourceHandle GetUniformInterpDataSource();
PXR_NS::HdTokenDataSourceHandle GetFaceVaryingInterpDataSource();
PXR_NS::HdTokenDataSourceHandle GetVertexInterpDataSource();
PXR_NS::HdContainerDataSourceHandle GetMaterialDataSource(
    const PXR_NS::SdfPath& material);

void CopyGlmVecArrayToVt(
    PXR_NS::VtVec3fArray& dst, const glm::Array<glm::Vector3>& src);

}  // namespace glmhydra
