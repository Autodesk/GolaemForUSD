#pragma once

#include "hydraGlobals.h"

#include <glmArray.h>
#include <glmFurCache.h>
#include <glmVector3.h>

#include <pxr/imaging/hd/dataSource.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

#include <vector>

namespace glmhydra {

/*
 * Class which provides data sources for the curves defined by a Golaem
 * FurCache.
 */
class FurAdapter
{
public:
    FurAdapter(
        glm::crowdio::FurCache::SP furCachePtr, size_t meshInFurIndex,
        float scale, const PXR_NS::SdfPath& material,
        const PrimvarDSMapRef& customPrimvars,
        float renderPercent = 100.0f, int refineLevel = 0);

    void SetGeometry(const glm::Array<glm::Vector3>& deformedVertices);

    void SetGeometry(
        const glm::Array<PXR_NS::HdSampledDataSource::Time>& shutterOffsets,
        const DeformedVectors& deformedVertices, size_t furIndex);

    PXR_NS::HdContainerDataSourceHandle GetDataSource() const;

private:
    void CopyVertices(size_t shutterIndex, const glm::Array<glm::Vector3>& src);

    PXR_NS::HdContainerDataSourceHandle GetCurveDataSource() const;
    PXR_NS::HdContainerDataSourceHandle GetPrimvarsDataSource() const;
    PXR_NS::HdContainerDataSourceHandle GetDisplayStyleDataSource() const;

    glm::crowdio::FurCache::SP _furCachePtr;
    size_t _meshInFurIndex;
    int _curveIncr;
    PXR_NS::SdfPath _material;
    const PrimvarDSMapRef _customPrimvars;
    PrimvarDSMap _perCurvePrimvars;
    int _refineLevel;
    PXR_NS::VtIntArray _vertexCounts;
    PXR_NS::VtIntArray _vertexIndices;
    std::vector<PXR_NS::VtVec3fArray> _vertices;
    PXR_NS::VtFloatArray _widths;
    PXR_NS::VtVec2fArray _uvs;
    PXR_NS::TfToken _curveDegree;
    std::vector<PXR_NS::HdSampledDataSource::Time> _shutterOffsets;
};

}  // namespace glmhydra
