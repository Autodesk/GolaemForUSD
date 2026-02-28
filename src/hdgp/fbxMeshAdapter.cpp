#include "fbxMeshAdapter.h"

#include <glmCrowdFBXBaker.h>
#include <glmCrowdFBXCharacter.h>

#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <fbxsdk.h>

PXR_NAMESPACE_USING_DIRECTIVE

using Time = HdSampledDataSource::Time;

using IntArrayDS = HdRetainedTypedSampledDataSource<VtIntArray>;
using Vec3fArrayDS = HdRetainedTypedMultisampledDataSource<VtVec3fArray>;
using Vec2fArrayDS = HdRetainedTypedSampledDataSource<VtVec2fArray>;

TF_DEFINE_PRIVATE_TOKENS(
    fbxMeshAdapterTokens,
    (st)
);

namespace glmhydra {

FbxMeshAdapter::FbxMeshAdapter(
    glm::crowdio::CrowdFBXCharacter& fbxCharacter, size_t meshIndex,
    const glm::Array<FbxTime>& fbxTimes, const glm::Array<Time>& shutterOffsets,
    const DeformedVectors& deformedVertices,
    const DeformedVectors& deformedNormals,
    int meshMaterialIndex, const SdfPath& material,
    const PrimvarDSMapRef& customPrimvars)
    : _xforms(shutterOffsets.size()),
      _shutterOffsets(shutterOffsets.begin(), shutterOffsets.end()),
      _areUvsPerVertex(false),
      _areUvsIndexed(false),
      _material(material),
      _customPrimvars(customPrimvars)
{
    size_t sampleCount = shutterOffsets.size();

    // fetch the transformation matrix for this mesh at each time sample

    FbxNode *fbxNode = fbxCharacter.getCharacterFBXMeshes()[meshIndex];
    FbxAMatrix geomTransform;
    glm::crowdio::CrowdFBXBaker::getGeomTransform(geomTransform, fbxNode);

    for (int isample = 0; isample < sampleCount; ++isample) {
        FbxAMatrix xform;
        fbxCharacter.getMeshGlobalTransform(xform, fbxNode, fbxTimes[isample]);
        xform *= geomTransform;
        _xforms[isample].Set(
            xform[0][0], xform[0][1], xform[0][2], xform[0][3],
            xform[1][0], xform[1][1], xform[1][2], xform[1][3],
            xform[2][0], xform[2][1], xform[2][2], xform[2][3],
            xform[3][0], xform[3][1], xform[3][2], xform[3][3]);
    }

    // polygons bound to a different material are ignored, meaning some number
    // of vertices may be superfluous, so we construct a map of original vertex
    // indices to used vertex indices

    FbxMesh *fbxMesh = fbxCharacter.getCharacterFBXMesh(meshIndex);
    const FbxLayer *fbxLayer0 = fbxMesh->GetLayer(0);
    FbxLayerElementArrayTemplate<int> const *mtlArray = nullptr;

    if (fbxLayer0) {
        const FbxLayerElementMaterial *materials = fbxLayer0->GetMaterials();
        if (materials) {
            mtlArray = &materials->GetIndexArray();
        }
    }

    auto IsPolyIgnored = [mtlArray, meshMaterialIndex](int polyIndex) {
        return mtlArray && (*mtlArray)[polyIndex] != meshMaterialIndex;
    };

    int allVertexCount = fbxMesh->GetControlPointsCount();
    int allPolyCount = fbxMesh->GetPolygonCount();
    std::vector<int> vertexMap(allVertexCount, -1);
    int usedVertexCount = 0;
    int usedPolyCount = 0;
    int usedPolyVertexCount = 0;

    for (int ipoly = 0; ipoly < allPolyCount; ++ipoly) {
        if (IsPolyIgnored(ipoly)) {
            continue;
        }
        int nvert = fbxMesh->GetPolygonSize(ipoly);
        for (int ivert = 0; ivert < nvert; ++ivert) {
            int vertIndex = fbxMesh->GetPolygonVertex(ipoly, ivert);
            if (vertexMap[vertIndex] < 0) {
                vertexMap[vertIndex] = usedVertexCount++;
            }
        }
        ++usedPolyCount;
        usedPolyVertexCount += nvert;
    }

    // copy the vertex counts and indices of the visible polygons only

    _vertexCounts.reserve(usedPolyCount);
    _vertexIndices.reserve(usedPolyVertexCount);

    for (int ipoly = 0; ipoly < allPolyCount; ++ipoly) {
        if (IsPolyIgnored(ipoly)) {
            continue;
        }
        int nvert = fbxMesh->GetPolygonSize(ipoly);
        for (int ivert = 0; ivert < nvert; ++ivert) {
            int vertIndex = fbxMesh->GetPolygonVertex(ipoly, ivert);
            _vertexIndices.push_back(vertexMap[vertIndex]);
        }
        _vertexCounts.push_back(nvert);
    }

    // for each time sample, copy the deformed vertices we need

    _vertices.resize(sampleCount);

    for (size_t i = 0; i < sampleCount; ++i) {
        const glm::Array<glm::Vector3>& src = deformedVertices[i][meshIndex];
        VtVec3fArray& dst = _vertices[i];
        dst.resize(usedVertexCount);

        for (int ivert = 0; ivert < allVertexCount; ++ivert) {
            int myIndex = vertexMap[ivert];
            if (myIndex >= 0) {
                dst[myIndex].Set(src[ivert].getFloatValues());
            }
        }
    }

    // for each time sample, copy the deformed normals we need (normals are
    // always per polygon vertex)

    if (fbxLayer0 && fbxLayer0->GetNormals()) {
        _normals.resize(sampleCount);

        for (size_t i = 0; i < sampleCount; ++i) {
            const glm::Array<glm::Vector3>& src = deformedNormals[i][meshIndex];
            VtVec3fArray& dst = _normals[i];
            dst.reserve(usedPolyVertexCount);

            int inputIndex = 0;
            for (int ipoly = 0; ipoly < allPolyCount; ++ipoly) {
                int nvert = fbxMesh->GetPolygonSize(ipoly);
                if (IsPolyIgnored(ipoly)) {
                    inputIndex += nvert;
                } else {
                    for (int ivert = 0; ivert < nvert; ++ivert) {
                        dst.emplace_back(src[inputIndex++].getFloatValues());
                    }
                }
            }
        }
    }

    // create UV and UV index tables, if the mesh has UVs (note that if there
    // are multiple UV sets, we only take the first)

    if (fbxMesh->GetLayerCount(FbxLayerElement::eUV) > 0) {
        const FbxLayer *layer = fbxMesh->GetLayer(
            fbxMesh->GetLayerTypedIndex(0, FbxLayerElement::eUV));
        const FbxLayerElementUV *uvElement = layer->GetUVs();

        _areUvsPerVertex =
            uvElement->GetMappingMode() == FbxLayerElement::eByControlPoint;
        _areUvsIndexed =
            uvElement->GetReferenceMode() != FbxLayerElement::eDirect;

        if (_areUvsPerVertex) {

            // indexed, per vertex UVs: figure out which UVs are actually used
            // (referenced by a vertex that is referenced by a visible polygon),
            // copy them to our (potentially) smaller UV table, and save indices
            // into that table for each visible vertex

            if (_areUvsIndexed) {
                int allUvCount = uvElement->GetDirectArray().GetCount();
                std::vector<int> uvMap(allUvCount, -1);
                int usedUvCount = 0;

                _uvIndices.reserve(usedVertexCount);

                for (int ivert = 0; ivert < allVertexCount; ++ivert) {
                    if (vertexMap[ivert] >= 0) {
                        int uvIndex = uvElement->GetIndexArray()[ivert];
                        if (uvMap[uvIndex] < 0) {
                            uvMap[uvIndex] = usedUvCount++;
                        }
                        _uvIndices.push_back(uvMap[uvIndex]);
                    }
                }

                _uvs.resize(usedUvCount);

                for (int uvIndex = 0; uvIndex < allUvCount; ++uvIndex) {
                    int myIndex = uvMap[uvIndex];
                    if (myIndex >= 0) {
                        const FbxVector2& uv =
                            uvElement->GetDirectArray()[uvIndex];
                        _uvs[myIndex].Set(
                            static_cast<float>(uv[0]),
                            static_cast<float>(uv[1]));
                    }
                }
            }

            // unindexed, per vertex UVs: copy the UVs for the vertices
            // referenced by visible polygons to our (potentially) smaller UV
            // table

            else {
                _uvs.reserve(usedVertexCount);

                for (int ivert = 0; ivert < allVertexCount; ++ivert) {
                    if (vertexMap[ivert] >= 0) {
                        const FbxVector2& uv =
                            uvElement->GetDirectArray()[ivert];
                        _uvs.emplace_back(
                            static_cast<float>(uv[0]),
                            static_cast<float>(uv[1]));
                    }
                }
            }
        } else {

            // indexed, per polygon vertex UVs: figure out which UVs are
            // actually used (referenced by a visible polygon), copy them to our
            // (potentially) smaller UV table, and save indices into that table
            // for each visible polygon vertex

            if (_areUvsIndexed) {
                int allUvCount = uvElement->GetDirectArray().GetCount();
                std::vector<int> uvMap(allUvCount, -1);
                int usedUvCount = 0;

                _uvIndices.reserve(usedPolyVertexCount);

                for (int ipoly = 0, pvIndex = 0; ipoly < allPolyCount; ++ipoly) {
                    int nvert = fbxMesh->GetPolygonSize(ipoly);
                    if (IsPolyIgnored(ipoly)) {
                        pvIndex += nvert;
                    } else {
                        for (int ivert = 0; ivert < nvert; ++ivert) {
                            int uvIndex = uvElement->GetIndexArray()[pvIndex++];
                            if (uvMap[uvIndex] < 0) {
                                uvMap[uvIndex] = usedUvCount++;
                            }
                            _uvIndices.push_back(uvMap[uvIndex]);
                        }
                    }
                }

                _uvs.resize(usedUvCount);

                for (int uvIndex = 0; uvIndex < allUvCount; ++uvIndex) {
                    int myIndex = uvMap[uvIndex];
                    if (myIndex >= 0) {
                        const FbxVector2& uv =
                            uvElement->GetDirectArray()[uvIndex];
                        _uvs[myIndex].Set(
                            static_cast<float>(uv[0]),
                            static_cast<float>(uv[1]));
                    }
                }
            }

            // unindexed, per polygon vertex UVs: copy the UVs for the vertices
            // in visible polygons to our (potentially) smaller UV table

            else {
                _uvs.reserve(usedPolyVertexCount);

                for (int ipoly = 0, pvIndex = 0; ipoly < allPolyCount; ++ipoly) {
                    int nvert = fbxMesh->GetPolygonSize(ipoly);
                    if (IsPolyIgnored(ipoly)) {
                        pvIndex += nvert;
                    } else {
                        for (int ivert = 0; ivert < nvert; ++ivert) {
                            const FbxVector2& uv =
                                uvElement->GetDirectArray()[pvIndex++];
                            _uvs.emplace_back(
                                static_cast<float>(uv[0]),
                                static_cast<float>(uv[1]));
                        }
                    }
                }
            }
        }
    }
}

HdContainerDataSourceHandle FbxMeshAdapter::GetXformDataSource() const
{
    return HdXformSchema::Builder()
        .SetMatrix(
            HdRetainedTypedMultisampledDataSource<GfMatrix4d>::New(
                _shutterOffsets.size(),
                const_cast<Time*>(_shutterOffsets.data()),
                const_cast<GfMatrix4d*>(_xforms.data())))
        .Build();
}

HdContainerDataSourceHandle FbxMeshAdapter::GetMeshDataSource() const
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

HdContainerDataSourceHandle FbxMeshAdapter::GetPrimvarsDataSource() const
{
    VtTokenArray dataNames;
    VtArray<HdDataSourceBaseHandle> dataSources;
    size_t capacity = 3;  // points, normals and UVs

    if (_customPrimvars) {
        capacity += _customPrimvars->size();
    }

    dataNames.reserve(capacity);
    dataSources.reserve(capacity);

    // vertex data source

    HdContainerDataSourceHandle vertexDataSource =
        HdPrimvarSchema::Builder()
        .SetPrimvarValue(
            Vec3fArrayDS::New(
                _shutterOffsets.size(),
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

    if (_normals.size() > 0) {
        HdContainerDataSourceHandle normalDataSource =
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                Vec3fArrayDS::New(
                    _shutterOffsets.size(),
                    const_cast<Time*>(_shutterOffsets.data()),
                    const_cast<VtVec3fArray*>(_normals.data())))
            .SetInterpolation(GetFaceVaryingInterpDataSource())
            .SetRole(
                HdPrimvarSchema::BuildRoleDataSource(
                    HdPrimvarSchemaTokens->normal))
            .Build();

        dataNames.push_back(HdPrimvarsSchemaTokens->normals);
        dataSources.push_back(normalDataSource);
    }

    // UV data source, if the mesh contains UVs

    HdContainerDataSourceHandle uvDataSource;

    if (_uvs.size() > 0) {
        HdPrimvarSchema::Builder uvBuilder;

        // UVs may or may not be indexed

        if (_areUvsIndexed) {
            uvBuilder.SetIndexedPrimvarValue(Vec2fArrayDS::New(_uvs));
            uvBuilder.SetIndices(IntArrayDS::New(_uvIndices));
        } else {
            uvBuilder.SetPrimvarValue(Vec2fArrayDS::New(_uvs));
        }

        // uvs may or may not be shared by polygons using the same vertices

        if (_areUvsPerVertex) {
            uvBuilder.SetInterpolation(GetVertexInterpDataSource());
        } else {
            uvBuilder.SetInterpolation(GetFaceVaryingInterpDataSource());
        }

        uvBuilder.SetRole(
            HdPrimvarSchema::BuildRoleDataSource(
                HdPrimvarSchemaTokens->textureCoordinate));

        dataNames.push_back(fbxMeshAdapterTokens->st);
        dataSources.push_back(uvBuilder.Build());
    }

    // custom primvars

    if (_customPrimvars) {
        for (auto it: *_customPrimvars) {
            dataNames.push_back(it.first);
            dataSources.push_back(
                HdPrimvarSchema::Builder()
                .SetPrimvarValue(it.second)
                .SetInterpolation(GetConstantInterpDataSource())
                .Build());
        }
    }

    // the final primvars data source contains the vertices, normals, UVs and
    // custom primvars

    return HdRetainedContainerDataSource::New(
        dataNames.size(), dataNames.cdata(), dataSources.cdata());
}

HdContainerDataSourceHandle FbxMeshAdapter::GetDataSource() const
{
    VtTokenArray dataNames;
    VtArray<HdDataSourceBaseHandle> dataSources;

    dataNames.reserve(4);
    dataSources.reserve(4);

    dataNames.push_back(HdXformSchemaTokens->xform);
    dataSources.push_back(GetXformDataSource());

    dataNames.push_back(HdMeshSchemaTokens->mesh);
    dataSources.push_back(GetMeshDataSource());

    dataNames.push_back(HdPrimvarsSchemaTokens->primvars);
    dataSources.push_back(GetPrimvarsDataSource());

    if (!_material.IsEmpty()) {
        dataNames.push_back(HdMaterialBindingsSchemaTokens->materialBindings);
        dataSources.push_back(GetMaterialDataSource(_material));
    }

    return HdRetainedContainerDataSource::New(
        dataNames.size(), dataNames.cdata(), dataSources.cdata());
}

HdDataSourceLocatorSet FbxMeshAdapter::GetVariableDataSources() const
{
    // actually, all primvars except for UV coordinates can vary from frame to
    // frame, but giving Hydra a list of all those locators seems to slow it
    // down more than just telling it that all primvars may vary

    HdDataSourceLocatorSet locators(HdPrimvarsSchema::GetDefaultLocator());
    locators.append(HdXformSchema::GetDefaultLocator());
    return locators;
}

}  // namespace glmhydra
