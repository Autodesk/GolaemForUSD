#include "furAdapter.h"

#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_USING_DIRECTIVE

using Time = HdSampledDataSource::Time;

using glm::crowdio::FurCache;
using glm::crowdio::FurCurveGroup;

TF_DEFINE_PRIVATE_TOKENS(
    furAdapterTokens,
    (st)
);

namespace glmhydra {

/*
 * Note that FurAdapter keeps a (smart) pointer to the fur cache, preventing it
 * from being deleted before the FurAdapter.
 *
 * Note also that the fur cache may contain more than one curve group, but it is
 * assumed here that all the information given for them in FurCurveGroup is in
 * fact shared by all of them: whether curves are cubic or linear, whether or
 * not they have widths and/or UVs, and additional float and vector properties.
 *
 * Call SetGeometry() afterwards with the fur vertex positions.
 */
FurAdapter::FurAdapter(
    FurCache::SP furCachePtr, size_t meshInFurIndex, float scale,
    const SdfPath& material, const PrimvarDSMapRef& customPrimvars,
    float renderPercent, int refineLevel)
    : _furCachePtr(furCachePtr),
      _meshInFurIndex(meshInFurIndex),
      _curveIncr(std::max(1l, std::lround(100.0f / renderPercent))),
      _material(material),
      _customPrimvars(customPrimvars),
      _refineLevel(refineLevel),
      _curveDegree(UsdGeomTokens->cubic)
{
    const FurCache& furCache = *_furCachePtr;

    // start by counting the number of visible curves and vertices

    size_t totalCurveCount = 0;
    size_t totalVertexCount = 0;
    size_t groupCount = furCache._curveGroups.size();

    for (size_t igroup = 0; igroup < groupCount; ++igroup) {
        const FurCurveGroup& group = furCache._curveGroups[igroup];
        if (group._supportMeshId == _meshInFurIndex) {
            size_t ncurve = group._numVertices.size();
            for (size_t icurve = 0; icurve < ncurve; icurve += _curveIncr) {
                totalCurveCount += 1;
                totalVertexCount += group._numVertices[icurve];
            }
        }
    }

    if (totalCurveCount == 0) {
        return;
    }

    _vertexCounts.reserve(totalCurveCount);
    _vertexIndices.reserve(totalVertexCount);

    // some information is determined by the first curve group and is assumed to
    // be shared by all groups in the cache

    const FurCurveGroup& firstGroup = furCache._curveGroups[0];
    if (firstGroup._curveDegrees == 1) {
        _curveDegree = UsdGeomTokens->linear;
    }

    bool hasWidths = !firstGroup._widths.empty();
    if (hasWidths) {
        _widths.reserve(totalVertexCount);
    }

    bool hasUVs = !firstGroup._uvs.empty();
    if (hasUVs) {
        _uvs.reserve(totalVertexCount);
    }

    size_t floatPropCount = firstGroup._floatProperties.size();
    std::vector<VtFloatArray> floatPropValues(floatPropCount);
    for (size_t i = 0; i < floatPropCount; ++i) {
        floatPropValues[i].reserve(totalCurveCount);
    }

    size_t vector3PropCount = firstGroup._vector3Properties.size();
    std::vector<VtVec3fArray> vector3PropValues(vector3PropCount);
    for (size_t i = 0; i < vector3PropCount; ++i) {
        vector3PropValues[i].reserve(totalCurveCount);
    }

    // fill in vertex counts, indices, widths, UVs and properties

    size_t outputIndex = 0;

    for (const FurCurveGroup& group: furCache._curveGroups) {
        if (group._supportMeshId != _meshInFurIndex) {
            continue;
        }

        size_t inputIndex = 0;
        size_t ncurve = group._numVertices.size();

        for (size_t icurve = 0; icurve < ncurve; ++icurve) {
            size_t nvert = group._numVertices[icurve];

            if (icurve % _curveIncr != 0) {
                inputIndex += nvert;
                continue;
            }

            _vertexCounts.push_back(static_cast<int>(nvert));

            for (size_t ivert = 0; ivert < nvert; ++ivert) {
                _vertexIndices.push_back(static_cast<int>(outputIndex));

                if (hasWidths) {
                    if (group._widths.empty()) {
                        _widths.push_back(0.0f);
                    } else {
                        _widths.push_back(scale * group._widths[inputIndex]);
                    }
                }

                if (hasUVs) {
                    if (group._uvs.empty()) {
                        _uvs.emplace_back(0.0f, 0.0f);
                    } else {
                        _uvs.emplace_back(
                            group._uvs[inputIndex][0],
                            group._uvs[inputIndex][1]);
                    }
                }

                ++inputIndex;
                ++outputIndex;
            }
        }

        if (group._floatProperties.size() == floatPropCount) {
            for (size_t i = 0; i < floatPropCount; ++i) {
                VtFloatArray& dst = floatPropValues[i];
                for (float value: group._floatProperties[i]) {
                    dst.push_back(value);
                }
            }
        }

        if (group._vector3Properties.size() == vector3PropCount) {
            for (size_t i = 0; i < vector3PropCount; ++i) {
                VtVec3fArray& dst = vector3PropValues[i];
                for (const glm::Vector3& vec3: group._vector3Properties[i]) {
                    dst.emplace_back(vec3.getFloatValues());
                }
            }
        }
    }

    // create Hydra data sources for per-curve properties

    for (size_t i = 0; i < floatPropCount; ++i) {
        const glm::GlmString& glmName = firstGroup._floatPropertiesNames[i];
        _perCurvePrimvars[TfToken(glmName.c_str())] =
            HdRetainedTypedSampledDataSource<VtFloatArray>::New(
                floatPropValues[i]);
    }

    for (size_t i = 0; i < vector3PropCount; ++i) {
        const glm::GlmString& glmName = firstGroup._vector3PropertiesNames[i];
        _perCurvePrimvars[TfToken(glmName.c_str())] =
            HdRetainedTypedSampledDataSource<VtVec3fArray>::New(
                vector3PropValues[i]);
    }
}

void FurAdapter::CopyVertices(
    size_t shutterIndex, const glm::Array<glm::Vector3>& src)
{
    const FurCache& furCache = *_furCachePtr;
    size_t inputIndex = 0;

    VtVec3fArray& dst = _vertices[shutterIndex];
    dst.clear();
    dst.reserve(_vertexIndices.size());

    for (const FurCurveGroup& group: furCache._curveGroups) {
        size_t ncurve = group._numVertices.size();
        for (size_t icurve = 0; icurve < ncurve; ++icurve) {
            size_t nvert = group._numVertices[icurve];
            if (icurve % _curveIncr == 0
                && group._supportMeshId == _meshInFurIndex) {
                for (size_t ivert = 0; ivert < nvert; ++ivert) {
                    if (inputIndex + ivert >= src.size()) {
                        break;
                    }
                    dst.emplace_back(src[inputIndex + ivert].getFloatValues());
                }
            }
            inputIndex += nvert;
        }
    }

    if (dst.size() != _vertexIndices.size()) {
        TF_WARN("[GolaemHydra] FurAdapter: expected %d vertices, got %d\n",
                _vertexIndices.size(), dst.size());
    }
}

/*
 * Sets the deformed fur vertices for the current frame.
 */
void FurAdapter::SetGeometry(const glm::Array<glm::Vector3>& deformedVertices)
{
    _shutterOffsets.assign(1, 0);
    _vertices.resize(1);
    CopyVertices(0, deformedVertices);
}

/*
 * Variation on SetGeometry() for motion blur. Specify any number of shutter
 * offsets and the deformed vertices for each of those offsets.
 *
 * It is assumed that the shutter offsets are given in order! That is,
 * HdRetainedTypedMultisampledDataSource makes that assumption.
 *
 * The DeformedVectors type corresponds to the vector arrays found in
 * glm::crowdio::OutputEntityGeoData. The arrays have three dimensions --
 * corresponding to the frame index, the fur index and the vector index -- so we
 * need the fur index to access the vectors.
 */
void FurAdapter::SetGeometry(
    const glm::Array<HdSampledDataSource::Time>& shutterOffsets,
    const DeformedVectors& deformedVertices, size_t furIndex)
{
    const size_t sampleCount = shutterOffsets.size();
    _shutterOffsets.assign(shutterOffsets.begin(), shutterOffsets.end());
    _vertices.resize(sampleCount);

    for (size_t i = 0; i < sampleCount; ++i) {
        CopyVertices(i, deformedVertices[i][furIndex]);
    }
}

HdContainerDataSourceHandle FurAdapter::GetCurveDataSource() const
{
    return HdBasisCurvesSchema::Builder()
        .SetTopology(
            HdBasisCurvesTopologySchema::Builder()
            .SetCurveVertexCounts(
                HdRetainedTypedSampledDataSource<VtIntArray>::New(
                    _vertexCounts))
            .SetCurveIndices(
                HdRetainedTypedSampledDataSource<VtIntArray>::New(
                    _vertexIndices))
            .SetBasis(
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    UsdGeomTokens->catmullRom))
            .SetType(
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    _curveDegree))
            .SetWrap(
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    UsdGeomTokens->nonperiodic))
            .Build())
        .Build();
}

HdContainerDataSourceHandle FurAdapter::GetPrimvarsDataSource() const
{
    VtTokenArray dataNames;
    VtArray<HdDataSourceBaseHandle> dataSources;
    size_t capacity = 3 + _perCurvePrimvars.size();

    if (_customPrimvars) {
        capacity += _customPrimvars->size();
    }

    dataNames.reserve(capacity);
    dataSources.reserve(capacity);

    // vertices

    HdContainerDataSourceHandle vertexDataSource =
        HdPrimvarSchema::Builder()
        .SetPrimvarValue(
            HdRetainedTypedMultisampledDataSource<VtVec3fArray>::New(
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

    // width per vertex

    if (!_widths.empty()) {
        HdContainerDataSourceHandle widthDataSource =
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<VtFloatArray>::New(_widths))
            .SetInterpolation(GetVertexInterpDataSource())
            .Build();

        dataNames.push_back(HdPrimvarsSchemaTokens->widths);
        dataSources.push_back(widthDataSource);
    }

    // per-vertex UVs, if the fur contains UVs

    if (_uvs.size() > 0) {
        HdContainerDataSourceHandle uvDataSource =
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                HdRetainedTypedSampledDataSource<VtVec2fArray>::New(_uvs))
            .SetInterpolation(GetVertexInterpDataSource())
            .SetRole(
                HdPrimvarSchema::BuildRoleDataSource(
                    HdPrimvarSchemaTokens->textureCoordinate))
            .Build();

        dataNames.push_back(furAdapterTokens->st);
        dataSources.push_back(uvDataSource);
    }

    // per-entity (constant) attributes

    if (_customPrimvars) {
        for (const auto& entry: *_customPrimvars) {
            HdContainerDataSourceHandle dataSource =
                HdPrimvarSchema::Builder()
                .SetPrimvarValue(entry.second)
                .SetInterpolation(GetConstantInterpDataSource())
                .Build();

            dataNames.push_back(entry.first);
            dataSources.push_back(dataSource);
        }
    }

    // per-curve (uniform) properties

    for (const auto& entry: _perCurvePrimvars) {
        HdContainerDataSourceHandle dataSource =
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(entry.second)
            .SetInterpolation(GetUniformInterpDataSource())
            .Build();

        dataNames.push_back(entry.first);
        dataSources.push_back(dataSource);
    }

    return HdRetainedContainerDataSource::New(
        dataNames.size(), dataNames.cdata(), dataSources.cdata());
}

HdContainerDataSourceHandle FurAdapter::GetDisplayStyleDataSource() const
{
    return HdLegacyDisplayStyleSchema::Builder()
        .SetRefineLevel(HdRetainedTypedSampledDataSource<int>::New(_refineLevel))
        .Build();
}

HdContainerDataSourceHandle FurAdapter::GetDataSource() const
{
    VtTokenArray dataNames;
    VtArray<HdDataSourceBaseHandle> dataSources;

    dataNames.reserve(6);
    dataSources.reserve(6);

    dataNames.push_back(HdXformSchemaTokens->xform);
    dataSources.push_back(GetIdentityXformDataSource());

    dataNames.push_back(HdBasisCurvesSchemaTokens->basisCurves);
    dataSources.push_back(GetCurveDataSource());

    dataNames.push_back(HdPrimvarsSchemaTokens->primvars);
    dataSources.push_back(GetPrimvarsDataSource());

    if (!_material.IsEmpty()) {
        dataNames.push_back(HdMaterialBindingsSchemaTokens->materialBindings);
        dataSources.push_back(GetMaterialDataSource(_material));
    }

    if (!_widths.empty()) {
        dataNames.push_back(HdLegacyDisplayStyleSchemaTokens->displayStyle);
        dataSources.push_back(GetDisplayStyleDataSource());
    }

    return HdRetainedContainerDataSource::New(
        dataNames.size(), dataNames.cdata(), dataSources.cdata());
}

}  // namespace glmhydra
