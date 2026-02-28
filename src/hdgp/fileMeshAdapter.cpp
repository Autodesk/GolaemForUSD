#include "fileMeshAdapter.h"

#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <cassert>

PXR_NAMESPACE_USING_DIRECTIVE

using Time = HdSampledDataSource::Time;

using IntArrayDS = HdRetainedTypedSampledDataSource<VtIntArray>;
using Vec3fArrayDS = HdRetainedTypedMultisampledDataSource<VtVec3fArray>;
using Vec2fArrayDS = HdRetainedTypedSampledDataSource<VtVec2fArray>;

TF_DEFINE_PRIVATE_TOKENS(
    fileMeshAdapterTokens,
    (st)
);

namespace glmhydra {

/*
 * The FileMeshAdapter constructor makes copies of all the data it needs, so all
 * the arguments can be deleted afterwards. It does little else, leaving as much
 * work as possible for the methods which return the data sources, because they
 * may be called from multiple threads.
 *
 * Call SetGeometry() afterwards to set the deformed vertices and normals. (For
 * a rigid mesh, this is unnecessary.)
 */
FileMeshAdapter::FileMeshAdapter(
    const glm::crowdio::GlmFileMesh& fileMesh)
    : _vertexCounts(fileMesh._polygonCount),
      _vertexIndices(fileMesh._polygonsTotalVertexCount),
      _totalVertexCount(fileMesh._vertexCount),
      _totalNormalCount(fileMesh._normalCount),
      _normalMode(glm::crowdio::GlmNormalMode(fileMesh._normalMode)),
      _uvMode(glm::crowdio::GlmUVMode(fileMesh._uvMode)),
      _isRigid(fileMesh._skinningType == glm::crowdio::GLM_SKIN_RIGID)
{
    for (int i = 0; i < _vertexCounts.size(); ++i) {
        _vertexCounts[i] = fileMesh._polygonsVertexCount[i];
    }

    for (int i = 0; i < _vertexIndices.size(); ++i) {
        _vertexIndices[i] = fileMesh._polygonsVertexIndices[i];
    }

    if (fileMesh._normalCount > 0 &&
        _normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX_INDEXED) {
        _normalIndices.resize(fileMesh._polygonsTotalVertexCount);
        for (int i = 0; i < _normalIndices.size(); ++i) {
            _normalIndices[i] = fileMesh._polygonsNormalIndices[i];
        }
    }

    // note that if there are multiple UV sets, we only take the first; the
    // others are ignored

    if (fileMesh._uvSetCount > 0 && fileMesh._uvCoordCount[0] > 0) {
        _uvs.resize(fileMesh._uvCoordCount[0]);
        if (_uvMode == glm::crowdio::GLM_UV_PER_POLYGON_VERTEX_INDEXED) {
            _uvIndices.resize(fileMesh._polygonsTotalVertexCount);
            for (int i = 0; i < _uvIndices.size(); ++i) {
                _uvIndices[i] = fileMesh._polygonsUVIndices[i];
            }
        }
        for (int i = 0; i < _uvs.size(); ++i) {
            _uvs[i].Set(fileMesh._us[0][i], fileMesh._vs[0][i]);
        }
    }

    // for a rigid body, copy the initial vertices and normals once and for all

    if (fileMesh._skinningType == glm::crowdio::GLM_SKIN_RIGID) {
        VtVec3fArray v(_totalVertexCount);
        for (size_t i = 0; i < _totalVertexCount; ++i) {
            const float *src = fileMesh._vertices[i]._position;
            v[i].Set(src[0], src[1], src[2]);
        }
        _vertices.assign(1, std::move(v));

        VtVec3fArray n(_totalNormalCount);
        for (size_t i = 0; i < _totalNormalCount; ++i) {
            const float *src = fileMesh._normals[i];
            n[i].Set(src[0], src[1], src[2]);
        }
        _normals.assign(1, std::move(n));

        _shutterOffsets.assign(1, 0);
    }
}

/*
 * Sets the deformed vertices and normals for the current frame.
 */
void FileMeshAdapter::SetGeometry(
    const glm::Array<glm::Vector3>& deformedVertices,
    const glm::Array<glm::Vector3>& deformedNormals)
{
    assert(deformedVertices.size() == _totalVertexCount);
    assert(deformedNormals.size() == _totalNormalCount);
    assert(!_isRigid);

    _shutterOffsets.assign(1, 0);

    _vertices.resize(1);
    _normals.resize(1);

    CopyGlmVecArrayToVt(_vertices[0], deformedVertices);
    CopyGlmVecArrayToVt(_normals[0], deformedNormals);
}

/*
 * Variation on SetGeometry() for motion blur. Specify any number of shutter
 * offsets and the deformed vertices and normals for each of those offsets.
 *
 * It is assumed that the shutter offsets are given in order! That is,
 * HdRetainedTypedMultisampledDataSource makes that assumption.
 *
 * The DeformedVectors type corresponds to the vector arrays found in
 * glm::crowdio::OutputEntityGeoData. The arrays have three dimensions --
 * corresponding to the frame index, the mesh index and the vector index -- so
 * we need the mesh index to access the vectors.
 */
void FileMeshAdapter::SetGeometry(
    const glm::Array<Time>& shutterOffsets,
    const DeformedVectors& deformedVertices,
    const DeformedVectors& deformedNormals,
    size_t meshIndex)
{
    const size_t sampleCount = shutterOffsets.size();

    assert(deformedVertices.size() == sampleCount);
    assert(deformedNormals.size() == sampleCount);
    assert(!_isRigid);

    _shutterOffsets.assign(shutterOffsets.begin(), shutterOffsets.end());

    _vertices.resize(sampleCount);
    _normals.resize(sampleCount);

    for (size_t i = 0; i < sampleCount; ++i) {
        assert(deformedVertices[i][meshIndex].size() == _totalVertexCount);
        CopyGlmVecArrayToVt(_vertices[i], deformedVertices[i][meshIndex]);
        assert(deformedNormals[i][meshIndex].size() == _totalNormalCount);
        CopyGlmVecArrayToVt(_normals[i], deformedNormals[i][meshIndex]);
    }
}

HdContainerDataSourceHandle FileMeshAdapter::GetMeshDataSource() const
{
    return HdMeshSchema::Builder()
        .SetTopology(
            HdMeshTopologySchema::Builder()
            .SetFaceVertexCounts(IntArrayDS::New(_vertexCounts))
            .SetFaceVertexIndices(IntArrayDS::New(_vertexIndices))
            .Build())
        .SetSubdivisionScheme(
            HdRetainedTypedSampledDataSource<TfToken>::New(UsdGeomTokens->none))
        .Build();
}

HdContainerDataSourceHandle FileMeshAdapter::GetPrimvarsDataSource() const
{
    VtTokenArray dataNames;
    VtArray<HdDataSourceBaseHandle> dataSources;
    size_t capacity = 3;  // points, normals and UVs

    dataNames.reserve(capacity);
    dataSources.reserve(capacity);

    // vertex data source

    HdContainerDataSourceHandle vertexDataSource =
        HdPrimvarSchema::Builder()
        .SetPrimvarValue(
            Vec3fArrayDS::New(
                _shutterOffsets.size(),
                // HdRetainedTypedMultisampledDataSource does not modify the
                // data, so this is safe
                const_cast<Time*>(_shutterOffsets.data()),
                const_cast<VtVec3fArray*>(_vertices.data())))
        .SetInterpolation(GetVertexInterpDataSource())
        .SetRole(
            HdPrimvarSchema::BuildRoleDataSource(
                HdPrimvarSchemaTokens->point))
        .Build();

    dataNames.push_back(HdPrimvarsSchemaTokens->points);
    dataSources.push_back(vertexDataSource);

    // normal data source, if the mesh contains normals

    HdContainerDataSourceHandle normalDataSource;

    if (_normals.size() > 0) {

        // normals may or may not be indexed

        HdPrimvarSchema::Builder normalBuilder;
        if (_normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX_INDEXED) {
            normalBuilder.SetIndexedPrimvarValue(
                Vec3fArrayDS::New(
                    _shutterOffsets.size(),
                    const_cast<Time*>(_shutterOffsets.data()),
                    const_cast<VtVec3fArray*>(_normals.data())));
            normalBuilder.SetIndices(IntArrayDS::New(_normalIndices));
        } else {
            normalBuilder.SetPrimvarValue(
                Vec3fArrayDS::New(
                    _shutterOffsets.size(),
                    const_cast<Time*>(_shutterOffsets.data()),
                    const_cast<VtVec3fArray*>(_normals.data())));
        }

        // normals may or may not be shared by polygons using the same vertices

        if (_normalMode ==
            glm::crowdio::GLM_NORMAL_PER_CONTROL_POINT) {
            normalBuilder.SetInterpolation(GetVertexInterpDataSource());
        } else {
            normalBuilder.SetInterpolation(
                GetFaceVaryingInterpDataSource());
        }

        normalBuilder.SetRole(
            HdPrimvarSchema::BuildRoleDataSource(
                HdPrimvarSchemaTokens->normal));

        dataNames.push_back(HdPrimvarsSchemaTokens->normals);
        dataSources.push_back(normalBuilder.Build());
    }

    // UV data source, if the mesh contains UVs

    HdContainerDataSourceHandle uvDataSource;

    if (_uvs.size() > 0) {
        HdPrimvarSchema::Builder uvBuilder;

        // UVs may or may not be indexed

        if (_uvMode ==
            glm::crowdio::GLM_UV_PER_POLYGON_VERTEX_INDEXED) {
            uvBuilder.SetIndexedPrimvarValue(Vec2fArrayDS::New(_uvs));
            uvBuilder.SetIndices(IntArrayDS::New(_uvIndices));
        } else {
            uvBuilder.SetPrimvarValue(Vec2fArrayDS::New(_uvs));
        }

        // uvs may or may not be shared by polygons using the same vertices

        if (_uvMode ==
            glm::crowdio::GLM_UV_PER_CONTROL_POINT) {
            uvBuilder.SetInterpolation(GetVertexInterpDataSource());
        } else {
            uvBuilder.SetInterpolation(GetFaceVaryingInterpDataSource());
        }

        uvBuilder.SetRole(
            HdPrimvarSchema::BuildRoleDataSource(
                HdPrimvarSchemaTokens->textureCoordinate));

        dataNames.push_back(fileMeshAdapterTokens->st);
        dataSources.push_back(uvBuilder.Build());
    }

    // the final primvars data source contains the vertices, normals and UVs

    return HdRetainedContainerDataSource::New(
        dataNames.size(), dataNames.cdata(), dataSources.cdata());
}

}  // namespace glmhydra
