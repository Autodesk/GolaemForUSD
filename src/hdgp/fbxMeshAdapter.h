#pragma once

#include "hydraGlobals.h"
#include "meshDataSourceBase.h"

#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

#include <glmArray.h>

#include <fbxsdk/fbxsdk_def.h>

#include <vector>

namespace glm {
    namespace crowdio {
        class CrowdFBXCharacter;
    }
}

#include <fbxsdk/fbxsdk_nsbegin.h>
class FbxTime;
#include <fbxsdk/fbxsdk_nsend.h>

namespace glmhydra {

/*
 * Class which provides Hydra data sources wrapping an FBX mesh: topology,
 * geometry, UVs, transformation matrix, etc.
 *
 * This class is very much like the FileMeshAdapter/FileMeshInstance classes for
 * Golaem file meshes, and the FurMeshAdapter class for fur, but with two
 * differences:
 *
 * 1. For FBX meshes, all the work is done in a single class. For Golaem file
 * meshes, the implementation is separated into two classes for rigid body
 * support: theoretically, you can have several instances of the same mesh with
 * different materials and transforms. There is no such support for rigid FBX
 * meshes.
 *
 * 2. Deformed vertices and normals are passed directly to the constructor; you
 * cannot modify them later as you can for Golaem file meshes and fur. Those
 * classes were designed so that we could, in theory, keep a cache of them
 * around and modify just the geometry at each frame. But we don't do that, in
 * the end, because tests showed it was not faster. And doing the same thing
 * here would entail keeping around the data structure needed to map deformed
 * vertices and normals to their positions in our tables (because we only use
 * the subset associated with a given material and ignore the rest).
 */
class FbxMeshAdapter: public MeshDataSourceBase
{
public:
    FbxMeshAdapter(
        glm::crowdio::CrowdFBXCharacter& fbxCharacter, size_t meshIndex,
        const glm::Array<FBXSDK_NAMESPACE::FbxTime>& fbxTimes,
        const glm::Array<PXR_NS::HdSampledDataSource::Time>& shutterOffsets,
        const DeformedVectors& deformedVertices,
        const DeformedVectors& deformedNormals,
        int meshMaterialIndex, const PXR_NS::SdfPath& material,
        const PrimvarDSMapRef& customPrimvars);

    PXR_NS::HdContainerDataSourceHandle GetDataSource() const override;
    PXR_NS::HdDataSourceLocatorSet GetVariableDataSources() const override;

private:
    PXR_NS::HdContainerDataSourceHandle GetXformDataSource() const;
    PXR_NS::HdContainerDataSourceHandle GetMeshDataSource() const;
    PXR_NS::HdContainerDataSourceHandle GetPrimvarsDataSource() const;

    PXR_NS::VtIntArray _vertexCounts;
    PXR_NS::VtIntArray _vertexIndices;
    std::vector<PXR_NS::VtVec3fArray> _vertices;
    std::vector<PXR_NS::VtVec3fArray> _normals;
    std::vector<PXR_NS::GfMatrix4d> _xforms;
    PXR_NS::VtVec2fArray _uvs;
    PXR_NS::VtIntArray _uvIndices;
    bool _areUvsPerVertex;
    bool _areUvsIndexed;
    std::vector<PXR_NS::HdSampledDataSource::Time> _shutterOffsets;
    PXR_NS::SdfPath _material;
    const PrimvarDSMapRef _customPrimvars;
};

}  // namespace glmhydra
