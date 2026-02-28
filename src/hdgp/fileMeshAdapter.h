#pragma once

#include "hydraGlobals.h"

#include <pxr/imaging/hd/retainedDataSource.h>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/hash.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>

#include <glmArray.h>
#include <glmGeometryFile.h>
#include <glmVector3.h>

#include <vector>

namespace glmhydra {

/*
 * Class which provides Hydra data sources wrapping the topology and UVs found
 * in a GlmFileMesh, as well as the deformed vertices and normals at any given
 * frame. Use FileMeshInstance to add an xform, a material and any custom
 * primvars.
 */
class FileMeshAdapter
{
public:
    FileMeshAdapter(const glm::crowdio::GlmFileMesh& fileMesh);

    void SetGeometry(
        const glm::Array<glm::Vector3>& deformedVertices,
        const glm::Array<glm::Vector3>& deformedNormals);

    void SetGeometry(
        const glm::Array<PXR_NS::HdSampledDataSource::Time>& shutterOffsets,
        const DeformedVectors& deformedVertices,
        const DeformedVectors& deformedNormals,
        size_t meshIndex);

    PXR_NS::HdContainerDataSourceHandle GetMeshDataSource() const;
    PXR_NS::HdContainerDataSourceHandle GetPrimvarsDataSource() const;

    bool IsRigid() const {
        return _isRigid;
    }

private:
    PXR_NS::VtIntArray _vertexCounts;
    PXR_NS::VtIntArray _vertexIndices;
    size_t _totalVertexCount;
    std::vector<PXR_NS::VtVec3fArray> _vertices;
    PXR_NS::VtIntArray _normalIndices;
    glm::crowdio::GlmNormalMode _normalMode;
    size_t _totalNormalCount;
    std::vector<PXR_NS::VtVec3fArray> _normals;
    PXR_NS::VtIntArray _uvIndices;
    glm::crowdio::GlmUVMode _uvMode;
    PXR_NS::VtVec2fArray _uvs;
    std::vector<PXR_NS::HdSampledDataSource::Time> _shutterOffsets;
    bool _isRigid;
};

}  // namespace glmhydra
