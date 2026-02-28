#pragma once

#include "fileMeshAdapter.h"
#include "hydraGlobals.h"
#include "meshDataSourceBase.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/base/tf/denseHashMap.h>

#include <memory>

namespace glmhydra {

/*
 * Adds xform, material and custom primvar data sources to the data sources for
 * a mesh's topology and geometry (provided by FileMeshAdapter). This class is
 * separated from FileMeshAdapter so that multiple instances can share the same
 * mesh but with different transformations and materials.
 */
class FileMeshInstance: public MeshDataSourceBase
{
public:
    FileMeshInstance(
        const std::shared_ptr<FileMeshAdapter>& adapter,
        const PXR_NS::SdfPath& material,
        const PrimvarDSMapRef& customPrimvars);

    void SetTransform(const float pos[3], const float rot[4], float scale);

    // TODO: variant of SetTransform() with shutter offsets

    PXR_NS::HdContainerDataSourceHandle GetDataSource() const override;
    PXR_NS::HdDataSourceLocatorSet GetVariableDataSources() const override;

private:
    PXR_NS::HdContainerDataSourceHandle GetPrimvarsDataSource() const;

    std::shared_ptr<FileMeshAdapter> _adapter;
    PXR_NS::SdfPath _material;
    const PrimvarDSMapRef _customPrimvars;
    PXR_NS::HdContainerDataSourceHandle _xform;
};

}  // namespace glmhydra
