#include "fileMeshInstance.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/base/gf/quatd.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace glmhydra {

FileMeshInstance::FileMeshInstance(
    const std::shared_ptr<FileMeshAdapter>& adapter,
    const SdfPath& material, const PrimvarDSMapRef& customPrimvars)
    : _adapter(adapter),
      _material(material),
      _customPrimvars(customPrimvars),
      _xform(GetIdentityXformDataSource())
{
}

void FileMeshInstance::SetTransform(
    const float pos[3], const float rot[4], float scale)
{
    GfMatrix4d mtx, mtx2;
    mtx.SetScale(scale);
    mtx2.SetRotate(GfQuatd(rot[3], rot[0], rot[1], rot[2]));
    mtx *= mtx2;
    mtx.SetTranslateOnly(GfVec3d(pos[0], pos[1], pos[2]));

    _xform = HdXformSchema::Builder()
        .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(mtx))
        .Build();
}

HdContainerDataSourceHandle
FileMeshInstance::GetPrimvarsDataSource() const
{
    HdContainerDataSourceHandle meshDataSource =
        _adapter->GetPrimvarsDataSource();

    if (!_customPrimvars || _customPrimvars->empty()) {
        return meshDataSource;
    }

    HdContainerDataSourceEditor editor(meshDataSource);

    for (auto it: *_customPrimvars) {
        editor.Set(
            HdDataSourceLocator(it.first),
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(it.second)
            .SetInterpolation(GetConstantInterpDataSource())
            .Build());
    }

    return editor.Finish();
}

HdContainerDataSourceHandle FileMeshInstance::GetDataSource() const
{
    VtTokenArray dataNames;
    VtArray<HdDataSourceBaseHandle> dataSources;

    dataNames.reserve(4);
    dataSources.reserve(4);

    dataNames.push_back(HdXformSchemaTokens->xform);
    dataSources.push_back(_xform);

    dataNames.push_back(HdMeshSchemaTokens->mesh);
    dataSources.push_back(_adapter->GetMeshDataSource());

    dataNames.push_back(HdPrimvarsSchemaTokens->primvars);
    dataSources.push_back(GetPrimvarsDataSource());

    if (!_material.IsEmpty()) {
        dataNames.push_back(HdMaterialBindingsSchemaTokens->materialBindings);
        dataSources.push_back(GetMaterialDataSource(_material));
    }

    return HdRetainedContainerDataSource::New(
        dataNames.size(), dataNames.cdata(), dataSources.cdata());
}

HdDataSourceLocatorSet FileMeshInstance::GetVariableDataSources() const
{
    // actually, all primvars except for UV coordinates can vary from frame to
    // frame, but giving Hydra a list of all those locators seems to slow it
    // down more than just telling it that all primvars may vary

    HdDataSourceLocatorSet locators(HdPrimvarsSchema::GetDefaultLocator());

    bool isRigid = kEnableRigidEntities && _adapter->IsRigid();
    if (isRigid) {
        locators.append(HdXformSchema::GetDefaultLocator());
    }

    return locators;
}

}  // namespace glmhydra
