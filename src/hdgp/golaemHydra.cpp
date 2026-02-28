#include <pxr/imaging/hdGp/generativeProceduralPlugin.h>
#include <pxr/imaging/hdGp/generativeProceduralPluginRegistry.h>

#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/renderSettingsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneGlobalsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <pxr/base/tf/debug.h>
#include <pxr/base/tf/staticTokens.h>

#include <glmCrowdGcgCharacter.h>
#include <glmCrowdFBXBaker.h>
#include <glmCrowdFBXCharacter.h>
#include <glmCrowdFBXStorage.h>
#include <glmCrowdIOUtils.h>
#include <glmCrowdTerrainMesh.h>
#include <glmFurCache.h>
#include <glmGolaemCharacter.h>
#include <glmIdsFilter.h>
#include <glmSimulationCacheFactory.h>
#include <glmSimulationCacheFactorySimulation.h>

#include "glmUSD.h"
#include "fbxMeshAdapter.h"
#include "fileMeshAdapter.h"
#include "fileMeshInstance.h"
#include "furAdapter.h"
#include "hydraGlobals.h"

#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

using Time = HdSampledDataSource::Time;

using glm::GlmString;
using glm::GolaemCharacter;
using glm::PODArray;
using glm::ShaderAssetDataContainer;
using glm::ShaderAttribute;
using glm::ShaderAttributeType;
using glm::crowdio::CachedSimulation;
using glm::crowdio::CrowdGcgCharacter;
using glm::crowdio::GlmFileMesh;
using glm::crowdio::GlmFileMeshTransform;
using glm::crowdio::GlmFrameData;
using glm::crowdio::GlmGeometryFile;
using glm::crowdio::GlmNormalMode;
using glm::crowdio::GlmSimulationData;
using glm::crowdio::GlmUVMode;
using glm::crowdio::SimulationCacheFactory;

using glmhydra::FbxMeshAdapter;
using glmhydra::FileMeshAdapter;
using glmhydra::FileMeshInstance;
using glmhydra::FurAdapter;
using glmhydra::MeshDataSourceBase;

using PrimvarDSMap = glmhydra::PrimvarDSMap;
using PrimvarDSMapRef = glmhydra::PrimvarDSMapRef;

TF_DEBUG_CODES(
    GLMHYDRA_TRACE,
    GLMHYDRA_DEPENDENCIES,
    GLMHYDRA_MOTION_BLUR
);

namespace
{
TF_DEFINE_PRIVATE_TOKENS(
    golaemTokens,
    (crowdFields)
    (cacheName)
    (cacheDir)
    (characterFiles)
    (entityIds)
    (enableLayout)
    (layoutFiles)
    (terrainFile)
    (renderPercent)
    (displayMode)
    (geometryTag)
    (dirmap)
    (materialPath)
    (materialAssignMode)
    (enableMotionBlur)
    (defaultShutterOpen)
    (defaultShutterClose)
    (enableLod)
    (enableFur)
    (furRenderPercent)
    (furRefineLevel)
    (bbox)
    (mesh)
    (bySurfaceShader)
    (byShadingGroup)
    (none)
);

/*
 * We use a hash map to store an entity's custom primvars (name and data source
 * for each), which are generated from shader attributes and PP attributes.
 */
using PrimvarDataSourceMap =
    TfDenseHashMap<TfToken, HdSampledDataSourceHandle, TfHash>;
using PrimvarDataSourceMapRef = std::shared_ptr<PrimvarDataSourceMap>;

/*
 * Arguments (primvars) provided by the USD prim.
 */
struct Args
{
    Args()
        : entityIds("*"),
          enableLayout(true),
          renderPercent(100),
          displayMode(golaemTokens->mesh),
          geometryTag(0),
          materialAssignMode(golaemTokens->byShadingGroup),
          enableMotionBlur(false),
          defaultShutterOpen(0),
          defaultShutterClose(1),
          enableLod(false),
          enableFur(false),
          furRenderPercent(100),
          furRefineLevel(0)
        {}

    enum ArgChanges {
        // no arguments have changed
        kNoChanges,
        // only unimportant arguments have changed
        kUnimportantChanges,
        // fur topology or refine level may change, but no kReload changes
        kFurChanges,
        // InitCrowd() will have to be called to reload cache, characters, etc.
        kReload
    };

    /*
     * Compares the arguments in other to these arguments and returns an
     * indicator of what has changed.
     */
    ArgChanges compare(const Args& other) const
    {
        ArgChanges ret = kNoChanges;

#define CMP(var, importance)                                    \
        if (var != other.var) {                                 \
            TF_DEBUG_MSG(                                       \
                GLMHYDRA_TRACE,                                 \
                "[GolaemHydra] attribute changed: " #var "\n"); \
            ret = std::max(ret, importance);                    \
        }

        CMP(crowdFields, kReload);
        CMP(cacheName, kReload);
        CMP(cacheDir, kReload);
        CMP(characterFiles, kReload);
        CMP(entityIds, kUnimportantChanges);
        CMP(enableLayout, kReload);
        CMP(layoutFiles, kReload);
        CMP(terrainFile, kReload);
        CMP(renderPercent, kUnimportantChanges);
        CMP(displayMode, kUnimportantChanges);
        CMP(geometryTag, kUnimportantChanges);
        CMP(dirmap, kReload);
        CMP(materialPath, kUnimportantChanges);
        CMP(materialAssignMode, kUnimportantChanges);
        CMP(enableMotionBlur, kUnimportantChanges);
        CMP(defaultShutterOpen, kUnimportantChanges);
        CMP(defaultShutterClose, kUnimportantChanges);
        CMP(enableLod, kUnimportantChanges);
        CMP(enableFur, kUnimportantChanges);
        CMP(furRenderPercent, kFurChanges);
        CMP(furRefineLevel, kFurChanges);

        return ret;
    }

    VtTokenArray crowdFields;
    TfToken cacheName;
    TfToken cacheDir;
    TfToken characterFiles;
    TfToken entityIds;
    bool enableLayout;
    TfToken layoutFiles;
    TfToken terrainFile;
    float renderPercent;
    TfToken displayMode;
    int geometryTag;
    TfToken dirmap;
    SdfPath materialPath;
    TfToken materialAssignMode;
    bool enableMotionBlur;
    float defaultShutterOpen;
    float defaultShutterClose;
    bool enableLod;
    bool enableFur;
    float furRenderPercent;
    int furRefineLevel;
};

/*
 * Information needed by the renderer for each entity in bbox display mode.
 */
struct BBoxEntityData
{
    GfVec3f extent;
    float scale;
    GfVec3f pos;
};

/*
 * Information needed by the renderer for each entity in mesh display mode.
 */
struct MeshEntityData
{
    size_t entityIndex;
    uint32_t crowdFieldIndex;
    uint32_t lodIndex;
    std::vector<std::shared_ptr<MeshDataSourceBase>> meshes;
    std::vector<std::shared_ptr<FurAdapter>> fur;
    HdContainerDataSourceHandle extent;
};

/*
 * Key used to uniquely identify a mesh in the rigid mesh cache.
 */
struct MeshKey {
    // from GlmSimulationData::_characterIdx
    int32_t characterIndex;

    // from OutputEntityGeoData::_geometryFileIndexes
    int32_t lodIndex;

    // from GlmFileMeshTransform::_meshIndex
    uint16_t meshIndex;

    bool operator==(const MeshKey& other) const {
        return characterIndex == other.characterIndex
            && lodIndex == other.lodIndex
            && meshIndex == other.meshIndex;
    }

    std::size_t hash() const noexcept {
        constexpr int bits = sizeof(std::size_t) * 8 / 3; // 10 or 21
        return static_cast<std::size_t>(lodIndex) << (2*bits)
            | static_cast<std::size_t>(characterIndex) << bits
            | static_cast<std::size_t>(meshIndex);
    }

    struct Hash {
        std::size_t operator()(const MeshKey& key) const noexcept {
            return key.hash();
        }
    };
};

/*
 * This is the actual plugin implementation.
 */
class GolaemProcedural: public HdGpGenerativeProcedural
{
public:
    GolaemProcedural(const SdfPath &proceduralPrimPath)
        : HdGpGenerativeProcedural(proceduralPrimPath),
          _fbxStorage(),
          _fbxBaker(_fbxStorage.touchFbxSdkManager())
    {
        glm::usdplugin::init();
        _factory = new SimulationCacheFactory();
        _updateCount = 0;
    }

    virtual ~GolaemProcedural()
    {
        TF_DEBUG_MSG(
            GLMHYDRA_TRACE,
            "[GolaemHydra] deleting simulation cache factory...");
        delete _factory;
        TF_DEBUG_MSG(GLMHYDRA_TRACE, " done\n");
        TF_DEBUG_MSG(
            GLMHYDRA_TRACE,
            "[GolaemHydra] calling glm::usdplugin::finish()...");
        glm::usdplugin::finish();
        TF_DEBUG_MSG(GLMHYDRA_TRACE, " done\n");
    }

    DependencyMap UpdateDependencies(
        const HdSceneIndexBaseRefPtr& inputScene) override;

    ChildPrimTypeMap Update(
        const HdSceneIndexBaseRefPtr& inputScene,
        const ChildPrimTypeMap& previousResult,
        const DependencyMap& dirtiedDependencies,
        HdSceneIndexObserver::DirtiedPrimEntries *outputDirtiedPrims) override;

    HdSceneIndexPrim GetChildPrim(
        const HdSceneIndexBaseRefPtr& inputScene,
        const SdfPath &childPrimPath) override;

    bool AsyncBegin(bool /*asyncEnabled*/) override
    {
        return false;
    }

    AsyncState AsyncUpdate(
        const ChildPrimTypeMap& /*previousResult*/,
        ChildPrimTypeMap* /*outputPrimTypes*/,
        HdSceneIndexObserver::DirtiedPrimEntries* /*outputDirtiedPrims*/)
        override
     {
         return Finished;
     }

private:
    Args GetArgs(
        const HdSceneIndexBaseRefPtr& inputScene, const SdfPath& primPath) const;
    void InitCrowd(const HdSceneIndexBaseRefPtr& inputScene);
    void PopulateCrowd(const HdSceneIndexBaseRefPtr& inputScene);
    void GenerateMeshesAndFur(
        MeshEntityData& meshEntityData,
        CachedSimulation& cachedSimulation, double frame, int entityIndex,
        bool motionBlur, const GfVec2d& shutter,
        bool lodEnabled, const GfVec3d& cameraPos, const GfVec3d& entityPos,
        size_t *lodLevel);
    void GenerateGCGMeshes(
        MeshEntityData& meshEntityData,
        const GlmSimulationData *simData, const GlmFrameData *frameData,
        const glm::Array<Time>& shutterOffsets,
        const glm::crowdio::InputEntityGeoData& inputData,
        const glm::crowdio::OutputEntityGeoData& outputData,
        const PrimvarDSMapRef& customPrimvars);
    void GenerateFBXMeshes(
        MeshEntityData& meshEntityData, const glm::Array<Time>& shutterOffsets,
        const glm::crowdio::InputEntityGeoData& inputData,
        const glm::crowdio::OutputEntityGeoData& outputData,
        const PrimvarDSMapRef& customPrimvars);
    PrimvarDSMapRef GenerateCustomPrimvars(
        const GlmSimulationData *simData, const GlmFrameData *frameData,
        const ShaderAssetDataContainer *shaderData,
        const GolaemCharacter *character, int entityIndex) const;
    SdfPath FindMaterialForShadingGroup(
        const GolaemCharacter *character, int shGroupIndex) const;

    using _ChildIndexMap = std::unordered_map<SdfPath, size_t, TfHash>;
    using _ChildIndexPairMap =
        std::unordered_map<SdfPath, std::pair<size_t, size_t>, TfHash>;

    // primvars provided by the procedural prim
    Args _args;

    // parsed dirmap rules for findDirmappedFile()
    glm::Array<GlmString> _dirmapRules;

    // actual cache directory after applying dirmap rules
    GlmString _mappedCacheDir;

    // in bbox display mode, maps the path of a Hydra prim to an index into
    // _bboxEntities
    _ChildIndexMap _childIndices;

    // in mesh display mode, maps the path of a Hydra prim to a pair of indices:
    // an index into _meshEntities, and an index into that structure's meshes or
    // curves (fur)
    _ChildIndexPairMap _childIndexPairs;

    // the Golaem simulation cache factory
    SimulationCacheFactory* _factory;

    // how many times Update() has been called
    int _updateCount;

    // the definition of each displayed entity in bbox display mode
    std::vector<BBoxEntityData> _bboxEntities;

    // the definition of each displayed entity in mesh display mode
    std::vector<MeshEntityData> _meshEntities;

    // cache of reusable FileMeshAdapter instances for rigid meshes
    std::unordered_map<MeshKey, std::shared_ptr<FileMeshAdapter>, MeshKey::Hash>
    _rigidMeshCache;

    // we don't use this, but glmPrepareEntityGeometry() needs it
    glm::crowdio::CrowdFBXStorage _fbxStorage;

    // we don't use this, but glmPrepareEntityGeometry() needs it
    glm::crowdio::CrowdFBXBaker _fbxBaker;
};

/*
 * Fetches the primvar of type T identified by the given token and
 * stores it in result, if found.
 */
template<typename T>
void GetTypedPrimvar(
    HdPrimvarsSchema& primvars, TfToken token, T& result)
{
    HdSampledDataSourceHandle src =
        primvars.GetPrimvar(token).GetPrimvarValue();

    if (src) {
        VtValue v = src->GetValue(0.0f);
        if (v.IsHolding<T>()) {
            result = v.UncheckedGet<T>();
        }
    }
}

/*
 * Fetches a primvar which is a token containing a list of names separated by
 * semicolons. Stores the names found in result.
 */
void GetTokenArrayPrimvar(
    HdPrimvarsSchema& primvars, TfToken token, VtTokenArray& result)
{
    HdSampledDataSourceHandle src =
        primvars.GetPrimvar(token).GetPrimvarValue();

    if (src) {
        VtValue v = src->GetValue(0.0f);
        if (v.IsHolding<TfToken>()) {
            std::string str = v.UncheckedGet<TfToken>();
            std::string::size_type pos, lastpos = 0;
            while ((pos = str.find(';', lastpos)) != std::string::npos) {
                result.push_back(TfToken(str.substr(lastpos, pos - lastpos)));
                lastpos = pos + 1;
            }
            if (result.empty()) {
                result.push_back(v.UncheckedGet<TfToken>());
            } else {
                result.push_back(
                    TfToken(str.substr(lastpos, str.size() - lastpos)));
            }
        }
    }
}

Args GolaemProcedural::GetArgs(
    const HdSceneIndexBaseRefPtr& inputScene, const SdfPath& primPath) const
{
    Args result;

    HdSceneIndexPrim prim = inputScene->GetPrim(primPath);
    HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(prim.dataSource);

    GetTokenArrayPrimvar(
        primvars, golaemTokens->crowdFields, result.crowdFields);
    GetTypedPrimvar(
        primvars, golaemTokens->cacheName, result.cacheName);
    GetTypedPrimvar(
        primvars, golaemTokens->cacheDir, result.cacheDir);
    GetTypedPrimvar(
        primvars, golaemTokens->characterFiles, result.characterFiles);
    GetTypedPrimvar(
        primvars, golaemTokens->entityIds, result.entityIds);
    GetTypedPrimvar(
        primvars, golaemTokens->enableLayout, result.enableLayout);
    GetTypedPrimvar(
        primvars, golaemTokens->layoutFiles, result.layoutFiles);
    GetTypedPrimvar(
        primvars, golaemTokens->terrainFile, result.terrainFile);
    GetTypedPrimvar(
        primvars, golaemTokens->renderPercent, result.renderPercent);
    GetTypedPrimvar(
        primvars, golaemTokens->displayMode, result.displayMode);
    GetTypedPrimvar(
        primvars, golaemTokens->geometryTag, result.geometryTag);
    GetTypedPrimvar(
        primvars, golaemTokens->dirmap, result.dirmap);
    GetTypedPrimvar(
        primvars, golaemTokens->materialAssignMode, result.materialAssignMode);
    GetTypedPrimvar(
        primvars, golaemTokens->enableMotionBlur, result.enableMotionBlur);
    GetTypedPrimvar(
        primvars, golaemTokens->defaultShutterOpen, result.defaultShutterOpen);
    GetTypedPrimvar(
        primvars, golaemTokens->defaultShutterClose, result.defaultShutterClose);
    GetTypedPrimvar(
        primvars, golaemTokens->enableLod, result.enableLod);
    GetTypedPrimvar(
        primvars, golaemTokens->enableFur, result.enableFur);
    GetTypedPrimvar(
        primvars, golaemTokens->furRenderPercent, result.furRenderPercent);
    GetTypedPrimvar(
        primvars, golaemTokens->furRefineLevel, result.furRefineLevel);

    // a primvar cannot be a relationship, so we convert the materialPath
    // argument (a token) to an SdfPath, which can be relative to the procedural
    // prim

    TfToken matpath;
    GetTypedPrimvar(primvars, golaemTokens->materialPath, matpath);

    if (matpath.IsEmpty()) {
        result.materialPath = primPath.AppendElementString("Materials");
    } else {
        std::string stdpath = matpath.GetString();
        if (stdpath.back() == '/') {
            stdpath.pop_back();
        }
        result.materialPath = SdfPath(stdpath).MakeAbsolutePath(primPath);
    }

    return result;
}

/*
 * Called by Update() one time only, once the arguments (cache file, crowd field
 * names, etc.) are known. We assume that the arguments never change.
 */
void GolaemProcedural::InitCrowd(const HdSceneIndexBaseRefPtr& /*inputScene*/)
{
    // apply dirmap rules to find actual paths of character files and load them

    if (_args.characterFiles.size() > 0) {
        glm::Array<GlmString> fileList;
        split(_args.characterFiles.GetString(), ";", fileList);

        for (int i = 0; i < fileList.size(); ++i) {
            GlmString mappedPath;
            findDirmappedFile(mappedPath, fileList[i], _dirmapRules);
            fileList[i] = mappedPath;
            TF_DEBUG_MSG(
                GLMHYDRA_TRACE,
                "[GolaemHydra] loading Golaem character file: %s\n",
                mappedPath.c_str());
        }

        GlmString characterFiles = glm::stringArrayToString(fileList, ";");
        _factory->loadGolaemCharacters(characterFiles);
    }

    // dirmap and load layout and terrain files

    if (_args.enableLayout && !_args.layoutFiles.IsEmpty()) {

        // load layout files

        glm::Array<GlmString> fileList;
        split(_args.layoutFiles.GetString(), ";", fileList);

        for (int i = 0; i < fileList.size(); ++i) {
            GlmString mappedPath;
            findDirmappedFile(mappedPath, fileList[i], _dirmapRules);
            _factory->loadLayoutHistoryFile(
                _factory->getLayoutHistoryCount(), mappedPath.c_str());
        }

        // load terrain files

        glm::crowdio::crowdTerrain::TerrainMesh *srcTerrain = nullptr;
        glm::crowdio::crowdTerrain::TerrainMesh *dstTerrain = nullptr;

        if (_args.crowdFields.size() > 0) {
            GlmString glmpath =
                _mappedCacheDir + "/" + _args.cacheName.GetText() +
                "." + _args.crowdFields[0].GetText() + ".gtg";
            srcTerrain = glm::crowdio::crowdTerrain::loadTerrainAsset(
                glmpath.c_str());
        }

        if (!_args.terrainFile.IsEmpty()) {
            GlmString mappedPath;
            findDirmappedFile(
                mappedPath, _args.terrainFile.GetString(), _dirmapRules);
            dstTerrain = glm::crowdio::crowdTerrain::loadTerrainAsset(
                mappedPath.c_str());
        }

        if (dstTerrain == nullptr) {
            dstTerrain = srcTerrain;
        }

        _factory->setTerrainMeshes(srcTerrain, dstTerrain);
    }
}

/*
 * Fetches and returns the current frame number from globals.
 */
double GetCurrentFrame(const HdSceneIndexBaseRefPtr& inputScene)
{
    double frame = 0.0;

    const HdSceneGlobalsSchema globals =
        HdSceneGlobalsSchema::GetFromSceneIndex(inputScene);

    if (globals) {
        HdDoubleDataSourceHandle frameDS = globals.GetCurrentFrame();
        if (frameDS) {
            frame = frameDS->GetTypedValue(0);
            if (std::isnan(frame)) {
                TF_WARN("[GolaemHydra] changing current frame NAN to 0!\n");
                frame = 0;
            }
        }
    }

    return frame;
}

/*
 * Fetches and returns the shutter interval for motion blur from the active
 * render settings prim, if there is one. Returns true if there is, false if
 * not.
 */
bool GetShutterFromRenderSettings(
    const HdSceneIndexBaseRefPtr& inputScene, GfVec2d *shutter)
{
    const HdSceneGlobalsSchema globals =
        HdSceneGlobalsSchema::GetFromSceneIndex(inputScene);
    if (!globals) {
        return false;
    }

    HdPathDataSourceHandle rsPathDS = globals.GetActiveRenderSettingsPrim();
    if (!rsPathDS) {
        return false;
    }

    const SdfPath rsPath = rsPathDS->GetTypedValue(0);
    if (rsPath.IsEmpty()) {
        return false;
    }

    const HdSceneIndexPrim rsPrim = inputScene->GetPrim(rsPath);
    HdRenderSettingsSchema rs =
        HdRenderSettingsSchema::GetFromParent(rsPrim.dataSource);
    if (!rs) {
        return false;
    }

    HdVec2dDataSourceHandle shutterDS = rs.GetShutterInterval();
    if (!shutterDS) {
        return false;
    }

    *shutter = shutterDS->GetTypedValue(0);
    return true;
}

/*
 * Returns the global transformation matrix for the prim at the given path.
 */
GfMatrix4d GetPrimWorldMatrix(
    const HdSceneIndexBaseRefPtr& inputScene, SdfPath path)
{
    GfMatrix4d mtx(1);

    while (!path.IsEmpty()) {
        HdSceneIndexPrim prim = inputScene->GetPrim(path);
        if (!prim.dataSource) {
            break;
        }
        HdXformSchema xform =
            HdXformSchema::GetFromParent(prim.dataSource);
        if (!xform) {
            break;
        }
        HdMatrixDataSourceHandle mtxDs = xform.GetMatrix();
        if (mtxDs) {
            mtx *= mtxDs->GetTypedValue(0);
        }
        HdBoolDataSourceHandle resetDs = xform.GetResetXformStack();
        if (resetDs && resetDs->GetTypedValue(0)) {
            break;
        }
        path = path.GetParentPath();
    }

    return mtx;
}

/*
 * Returns the path of the primary camera, if there is one.
 */
SdfPath GetCameraPath(const HdSceneIndexBaseRefPtr& inputScene)
{
    const HdSceneGlobalsSchema globals =
        HdSceneGlobalsSchema::GetFromSceneIndex(inputScene);

    if (!globals) {
        return SdfPath();
    }

    HdPathDataSourceHandle camPathDS = globals.GetPrimaryCameraPrim();
    if (!camPathDS) {
        return SdfPath();
    }

    return camPathDS->GetTypedValue(0);
}

/*
 * Fetches and returns the location in world coordinates of the primary camera,
 * if there is one. Returns true if there is, false if not.
 */
bool GetCameraPos(const HdSceneIndexBaseRefPtr& inputScene, GfVec3d *pos)
{
    const SdfPath camPath = GetCameraPath(inputScene);
    if (camPath.IsEmpty()) {
        return false;
    }
    GfMatrix4d mtx = GetPrimWorldMatrix(inputScene, camPath);
    *pos = mtx.ExtractTranslation();
    return true;
}

/*
 * Fetches and returns the shutter interval from the primary camera prim, if
 * there is one. Returns true if there is, false if not.
 */
bool GetShutterFromCamera(
    const HdSceneIndexBaseRefPtr& inputScene, GfVec2d *shutter)
{
    SdfPath path = GetCameraPath(inputScene);
    if (path.IsEmpty()) {
        return false;
    }

    HdSceneIndexPrim prim = inputScene->GetPrim(path);
    HdCameraSchema cam = HdCameraSchema::GetFromParent(prim.dataSource);
    if (!cam) {
        return false;
    }

    HdDoubleDataSourceHandle openDS = cam.GetShutterOpen();
    HdDoubleDataSourceHandle closeDS = cam.GetShutterClose();
    if (!(openDS && closeDS)) {
        return false;
    }

    shutter->Set(openDS->GetTypedValue(0), closeDS->GetTypedValue(0));
    return true;
}

/*
 * Returns a data source that returns the given extent.
 */
HdContainerDataSourceHandle GetExtentDataSource(
    const GfVec3d& min, const GfVec3d& max)
{
    return HdExtentSchema::Builder()
        .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(min))
        .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(max))
        .Build();
}

/*
 * Returns a data source that returns the topology of a cube.
 */
HdContainerDataSourceHandle GetCubeMeshDataSource()
{
    static const VtIntArray faceVertexCounts =
        {4, 4, 4, 4, 4, 4};

    static const VtIntArray faceVertexIndices =
        {0, 1, 3, 2, 2, 3, 5, 4, 4, 5, 7, 6, 6, 7, 1, 0, 1,
            7, 5, 3, 6, 0, 2, 4};

    using IntArrayDs = HdRetainedTypedSampledDataSource<VtIntArray>;

    static const IntArrayDs::Handle fvcDs = IntArrayDs::New(faceVertexCounts);

    static const IntArrayDs::Handle fviDs = IntArrayDs::New(faceVertexIndices);

    static const HdContainerDataSourceHandle meshDs =
        HdMeshSchema::Builder()
            .SetTopology(HdMeshTopologySchema::Builder()
                .SetFaceVertexCounts(fvcDs)
                .SetFaceVertexIndices(fviDs)
                .Build())
            .Build();

    return meshDs;
}

/*
 * Returns a data source that returns the vertices of a cube.
 */
HdContainerDataSourceHandle GetCubePrimvarsDataSource()
{
    static const VtArray<GfVec3f> points = {
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f}};

    using PointArrayDs = HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>;

    static const HdContainerDataSourceHandle primvarsDs =
        HdRetainedContainerDataSource::New(
            HdPrimvarsSchemaTokens->points,
            HdPrimvarSchema::Builder()
            .SetPrimvarValue(PointArrayDs::New(points))
            .SetInterpolation(glmhydra::GetVertexInterpDataSource())
            .SetRole(HdPrimvarSchema::
                     BuildRoleDataSource(HdPrimvarSchemaTokens->point))
            .Build()
            );

    return primvarsDs;
}

/*
 * Returns a data source that returns the extent of a unit cube.
 */
HdContainerDataSourceHandle GetCubeExtentDataSource()
{
    static const HdContainerDataSourceHandle extentDs =
        GetExtentDataSource(GfVec3d(-1.0), GfVec3d(1.0));

    return extentDs;
}

/*
 * Called by Update() to query the Golaem cache for the frame to be rendered.
 * Regenerates either _bboxEntities or _meshEntities, depending on the display
 * mode, which is then used by GetChildPrim() to generate meshes.
 */
void GolaemProcedural::PopulateCrowd(const HdSceneIndexBaseRefPtr& inputScene)
{
    // fetch the current frame number

    double frame = GetCurrentFrame(inputScene);
    TF_DEBUG_MSG(GLMHYDRA_TRACE, "[GolaemHydra] frame number: %g\n", frame);

    // fetch the camera position and the root prim's transformation matrix, for
    // LOD computation

    bool lodEnabled;
    GfVec3d cameraPos;
    GfMatrix4d rootMtx;

    if (_args.enableLod) {
        lodEnabled = GetCameraPos(inputScene, &cameraPos);
        rootMtx = GetPrimWorldMatrix(inputScene, _GetProceduralPrimPath());
    } else {
        lodEnabled = false;
        cameraPos.Set(0, 0, 0);
        rootMtx.SetIdentity();
    }

    // fetch the shutter interval from the render settings or from the primary
    // camera, if motion blur is enabled

    bool motionBlur = false;
    GfVec2d shutter;

    if (_args.enableMotionBlur) {
        if (GetShutterFromRenderSettings(inputScene, &shutter)) {
            TF_DEBUG_MSG(
                GLMHYDRA_MOTION_BLUR,
                "[GolaemHydra] motion blur shutter from render settings: %g %g\n",
                shutter[0], shutter[1]);
        } else if (GetShutterFromCamera(inputScene, &shutter)) {
            TF_DEBUG_MSG(
                GLMHYDRA_MOTION_BLUR,
                "[GolaemHydra] motion blur shutter from camera: %g %g\n",
                shutter[0], shutter[1]);
        } else {
            shutter.Set(_args.defaultShutterOpen, _args.defaultShutterClose);
            TF_DEBUG_MSG(
                GLMHYDRA_MOTION_BLUR,
                "[GolaemHydra] using default motion blur shutter: %g %g\n",
                shutter[0], shutter[1]);
        }
        motionBlur = (shutter[0] < shutter[1]);
    }

    // iterate over entities in crowd fields

    _bboxEntities.clear();
    _meshEntities.clear();

    glm::IdsFilter entityIdsFilter(_args.entityIds.GetText());

    for (int ifield = 0; ifield < _args.crowdFields.size(); ++ifield) {
        TfToken fieldName = _args.crowdFields[ifield];
        if (fieldName.IsEmpty()) {
            continue;
        }
        TF_DEBUG_MSG(
            GLMHYDRA_TRACE,
            "[GolaemHydra] crowd field: %s\n", fieldName.GetText());

        CachedSimulation& cachedSimulation =
            _factory->getCachedSimulation(
                _mappedCacheDir.c_str(), _args.cacheName.GetText(),
                fieldName.GetText());

        const GlmSimulationData *simData =
            cachedSimulation.getFinalSimulationData();

        if (simData == nullptr) {
            TF_DEBUG_MSG(
                GLMHYDRA_TRACE,
                "[GolaemHydra] no simulation data, skipping field\n");
            continue;
        }

        const GlmFrameData *frameData =
            cachedSimulation.getFinalFrameData(frame, UINT32_MAX, true);

        if (frameData == nullptr) {
            TF_DEBUG_MSG(
                GLMHYDRA_TRACE,
                "[GolaemHydra] no frame data, skipping field\n");
            continue;
        }

        int entityCount = simData->_entityCount;
        if (_args.renderPercent < 100.0f) {
            entityCount = std::lround(entityCount * _args.renderPercent * 0.01f);
        }
        if (_args.displayMode == golaemTokens->bbox) {
            _bboxEntities.reserve(_bboxEntities.size() + entityCount);
        } else {
            _meshEntities.reserve(_meshEntities.size() + entityCount);
        }

        for (int ientity = 0; ientity < entityCount; ++ientity) {

            // do nothing if the entity has been killed or excluded

            auto id = simData->_entityIds[ientity];
            if (id < 0 || !entityIdsFilter(id)) {
                continue;
            }

            // fetch the corresponding character

            auto entityType = simData->_entityTypes[ientity];
            auto characterIndex = simData->_characterIdx[ientity];
            const GolaemCharacter* character =
                _factory->getGolaemCharacter(characterIndex);

            if (character == nullptr) {
                TF_DEBUG_MSG(
                    GLMHYDRA_TRACE, "[GolaemHydra] character %d not found\n",
                    characterIndex);
                continue;
            }

            // fetch the position of this entity, if needed

            int frameDataIndex = -1;
            GfVec3f localPos(0);
            GfVec3d globalPos(0);

            if (_args.displayMode == golaemTokens->bbox || lodEnabled) {
                const auto& animData = character->_converterMapping;
                const auto *rootBone =
                    animData._skeletonDescription->getRootBone();
                auto rootBoneIndex = rootBone->getSpecificBoneIndex();
                auto boneCount = simData->_boneCount[entityType];

                frameDataIndex = rootBoneIndex
                    + simData->_iBoneOffsetPerEntityType[entityType]
                    + simData->_indexInEntityType[ientity] * boneCount;

                localPos.Set(frameData->_bonePositions[frameDataIndex]);
            }

            // save data needed for rendering bounding boxes

            if (_args.displayMode == golaemTokens->bbox) {
                const glm::GeometryAsset *asset =
                    character->getGeometryAssetFirstLOD(
                        static_cast<short>(_args.geometryTag));
                const glm::Vector3& extent = asset->_halfExtentsYUp;

                _bboxEntities.resize(_bboxEntities.size() + 1);
                BBoxEntityData& entity = _bboxEntities.back();

                entity.extent.Set(extent.getFloatValues());
                entity.scale = simData->_scales[ientity];
                entity.pos = localPos;
            }

            // save data needed for rendering meshes

            else {
                if (lodEnabled) {
                    globalPos = rootMtx.Transform(localPos);
                }
 
                MeshEntityData entity;
                size_t lodLevel;

                GenerateMeshesAndFur(
                    entity, cachedSimulation, frame, ientity, motionBlur,
                    shutter, lodEnabled, cameraPos, globalPos, &lodLevel);

                if (entity.meshes.empty() && entity.fur.empty()) {
                    continue;
                }

                entity.entityIndex = ientity;
                entity.crowdFieldIndex = static_cast<uint32_t>(ifield);
                entity.lodIndex = static_cast<uint32_t>(lodLevel);

                const glm::GeometryAsset *asset = character->getGeometryAsset(
                    static_cast<short>(_args.geometryTag), lodLevel);
                const glm::Vector3& localExtent = asset->_halfExtentsYUp;

                GfVec3d extent(localExtent.x, localExtent.y, localExtent.z);
                extent *= simData->_scales[ientity];
                entity.extent = GetExtentDataSource(
                    -extent + localPos, extent + localPos);

                _meshEntities.push_back(std::move(entity));
            }
        }
    }
}

/*
 * Finds all the shader and PP attributes defined for the given entity and
 * generates a Hydra data source of the appropriate type for each. Returns a
 * shared pointer to a hash map containing the name and data source for each.
 * Pass that hash map to each FileMeshInstance or FbxMeshAdapter so that all of
 * the mesh's entities share them.
 */
PrimvarDSMapRef GolaemProcedural::GenerateCustomPrimvars(
    const GlmSimulationData *simData, const GlmFrameData *frameData,
    const ShaderAssetDataContainer* shaderData,
    const GolaemCharacter *character, int entityIndex) const
{
    PrimvarDSMapRef dataSources = std::make_shared<PrimvarDSMap>();

    size_t shaderAttrCount = character->_shaderAttributes.size();
    size_t totalCount = shaderAttrCount
        + simData->_ppFloatAttributeCount
        + simData->_ppVectorAttributeCount;

    if (totalCount == 0) {
        return dataSources;
    }

    dataSources->reserve(totalCount);

    auto characterIndex = simData->_characterIdx[entityIndex];
    auto bakeIndex = simData->_entityToBakeIndex[entityIndex];

    // shader attributes (int, float, string, vector)

    const PODArray<int>& intData =
        shaderData->intData[entityIndex];
    const PODArray<float>& floatData =
        shaderData->floatData[entityIndex];
    const glm::Array<glm::Vector3>& vectorData =
        shaderData->vectorData[entityIndex];
    const glm::Array<GlmString>& stringData =
        shaderData->stringData[entityIndex];

    const PODArray<size_t>& globalToSpecificShaderAttrIdx =
        shaderData->globalToSpecificShaderAttrIdxPerChar[characterIndex];

    for (size_t i = 0; i < shaderAttrCount; ++i) {
        const ShaderAttribute& attr = character->_shaderAttributes[i];

        // ensure the attribute name is a valid identifier, and maybe prefix it
        // with "arnold:"

        std::string stdname;
        GlmString glmname = attr._name.c_str();
        GlmString subAttrName;
        glm::crowdio::RendererAttributeType::Value overrideType =
            glm::crowdio::RendererAttributeType::END;
        if (glm::crowdio::parseRendererAttribute(
                "arnold", attr._name, glmname, subAttrName, overrideType)) {
            stdname = "arnold:" + TfMakeValidIdentifier(glmname.c_str());
        } else {
            stdname = TfMakeValidIdentifier(glmname.c_str());
        }
        TfToken name(stdname);

        // create a data source that returns the attribute's value

        size_t index = globalToSpecificShaderAttrIdx[i];
        switch (attr._type) {
        case ShaderAttributeType::INT:
            (*dataSources)[name] =
                HdRetainedTypedSampledDataSource<int>::New(
                    intData[index]);
            break;
        case ShaderAttributeType::FLOAT:
            (*dataSources)[name] =
                HdRetainedTypedSampledDataSource<float>::New(
                    floatData[index]);
            break;
        case ShaderAttributeType::STRING:
            (*dataSources)[name] =
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    TfToken(stringData[index].c_str()));
            break;
        case ShaderAttributeType::VECTOR:
            (*dataSources)[name] =
                HdRetainedTypedSampledDataSource<GfVec3f>::New(
                    GfVec3f(vectorData[index].getFloatValues()));
            break;
        }
    }

    // PP attributes (float and vector)

    for (size_t i = 0; i < simData->_ppFloatAttributeCount; ++i) {
        TfToken name(TfMakeValidIdentifier(simData->_ppFloatAttributeNames[i]));
        (*dataSources)[name] =
            HdRetainedTypedSampledDataSource<float>::New(
                frameData->_ppFloatAttributeData[i][bakeIndex]);
    }

    for (size_t i = 0; i < simData->_ppVectorAttributeCount; ++i) {
        TfToken name(TfMakeValidIdentifier(simData->_ppVectorAttributeNames[i]));
        (*dataSources)[name] =
            HdRetainedTypedSampledDataSource<GfVec3f>::New(
                GfVec3f(frameData->_ppVectorAttributeData[i][bakeIndex]));
    }

    return dataSources;
}

/*
 * Returns the absolute SdfPath of the material for the given shading group in
 * the given character. Returns an empty path if the material assign mode is
 * "none" or if the shading group index is negative.
 */
SdfPath GolaemProcedural::FindMaterialForShadingGroup(
    const GolaemCharacter *character, int shGroupIndex) const
{
    if (_args.materialAssignMode == golaemTokens->none || shGroupIndex < 0) {
        return SdfPath();
    }

    const glm::ShadingGroup& shGroup = character->_shadingGroups[shGroupIndex];
    std::string matname;

    // assign material by shading group

    if (_args.materialAssignMode == golaemTokens->byShadingGroup) {
        const GlmString& glmname = shGroup._name;
        matname.assign(glmname.c_str(), glmname.size());
    }

    // assign material by surface shader

    else {
        int shAssetIndex = character->findShaderAsset(shGroup, "surface");
        if (shAssetIndex >= 0) {
            const GlmString& glmname =
                character->_shaderAssets[shAssetIndex]._name;
            matname.assign(glmname.c_str(), glmname.size());
        } else {
            matname = "DefaultGolaemMat";
        }
    }

    return _args.materialPath.AppendElementString(matname);
}

/*
 * Generates meshes and/or fur curves for the given entity at the given frame.
 * Meshes are added to the MeshEntityData's meshes vector; curves are added to
 * its fur vector.
 */
void GolaemProcedural::GenerateMeshesAndFur(
    MeshEntityData& meshEntityData,
    CachedSimulation& cachedSimulation, double frame, int entityIndex,
    bool motionBlur, const GfVec2d& shutter, bool lodEnabled,
    const GfVec3d& cameraPos, const GfVec3d& entityPos, size_t *lodLevel)
{
    // fetch simulation data, frame data and assets, then call
    // glmPrepareEntityGeometry() to generate information about this entity at
    // this frame

    const GlmSimulationData *simData =
        cachedSimulation.getFinalSimulationData();
    const GlmFrameData *frameData =
        cachedSimulation.getFinalFrameData(frame, UINT32_MAX, true);

    auto characterIndex = simData->_characterIdx[entityIndex];
    const GolaemCharacter* character =
        _factory->getGolaemCharacter(characterIndex);

    const glm::Array<glm::PODArray<int>>& entityAssets =
        cachedSimulation.getFinalEntityAssets(frame);

    glm::crowdio::InputEntityGeoData inputData;
    inputData._simuData = simData;
    inputData._characterIdx = characterIndex;
    inputData._character = character;
    inputData._assets = &entityAssets[entityIndex];
    inputData._entityIndex = entityIndex;
    inputData._entityToBakeIndex = simData->_entityToBakeIndex[entityIndex];
    inputData._entityId = simData->_entityIds[entityIndex];
    inputData._dirMapRules = _dirmapRules;
    inputData._enableLOD = lodEnabled;
    inputData._geometryTag = static_cast<short>(_args.geometryTag);
    inputData._fbxStorage = &_fbxStorage;
    inputData._fbxBaker = &_fbxBaker;
    inputData._generateFur = _args.enableFur;

    glm::Vector3 glmCamPos, glmEntPos;

    if (lodEnabled) {
        glmCamPos[0] = static_cast<float>(cameraPos[0]);
        glmCamPos[1] = static_cast<float>(cameraPos[1]);
        glmCamPos[2] = static_cast<float>(cameraPos[2]);
        glmEntPos[0] = static_cast<float>(entityPos[0]);
        glmEntPos[1] = static_cast<float>(entityPos[1]);
        glmEntPos[2] = static_cast<float>(entityPos[2]);
        inputData._entityPos = glmEntPos.getFloatValues();
        inputData._cameraWorldPosition = glmCamPos.getFloatValues();
        inputData._geoFileIndex = -1;
    } else {
        inputData._geoFileIndex = 0;
    }

    glm::Array<Time> shutterOffsets;

    if (motionBlur) {
        inputData._frames.reserve(3);
        inputData._frameDatas.reserve(3);
        if (shutter[0] != 0.0) {
            inputData._frames.push_back(frame + shutter[0]);
            inputData._frameDatas.push_back(
                cachedSimulation.getFinalFrameData(
                    frame + shutter[0], UINT32_MAX, true));
            shutterOffsets.push_back(static_cast<float>(shutter[0]));
        }
        if (shutter[0] <= 0.0 && shutter[1] >= 0.0) {
            inputData._frames.push_back(frame);
            inputData._frameDatas.push_back(frameData);
            shutterOffsets.push_back(0);
        }
        if (shutter[1] != 0.0) {
            inputData._frames.push_back(frame + shutter[1]);
            inputData._frameDatas.push_back(
                cachedSimulation.getFinalFrameData(
                    frame + shutter[1], UINT32_MAX, true));
            shutterOffsets.push_back(static_cast<float>(shutter[1]));
        }
    } else {
        inputData._frames.assign(1, frame);
        inputData._frameDatas.assign(1, frameData);
        shutterOffsets.push_back(0);
    }

    glm::crowdio::OutputEntityGeoData outputData;
    glm::crowdio::GlmGeometryGenerationStatus geoStatus =
        glm::crowdio::glmPrepareEntityGeometry(&inputData, &outputData);

    if (geoStatus != glm::crowdio::GIO_SUCCESS) {
        TF_WARN("[GolaemHydra] glmPrepareEntityGeometry() returned error: %s\n",
                glmConvertGeometryGenerationStatus(geoStatus));
        return;
    }

    if (lodEnabled) {
        *lodLevel = outputData._geometryFileIndexes[0];
    } else {
        *lodLevel = 0;
    }

    // fetch custom primvars for this entity: shader attributes and PP
    // attributes

    const ShaderAssetDataContainer* shaderData =
        cachedSimulation.getFinalShaderData(frame, UINT32_MAX, true);

    PrimvarDSMapRef customPrimvars = GenerateCustomPrimvars(
        simData, frameData, shaderData, character, entityIndex);

    // how meshes are generated depends on the geometry file type (GCG or FBX)

    switch (outputData._geoType) {
    case glm::crowdio::GeometryType::GCG:
        GenerateGCGMeshes(
            meshEntityData, simData, frameData, shutterOffsets,
            inputData, outputData, customPrimvars);
        break;
    case glm::crowdio::GeometryType::FBX:
        GenerateFBXMeshes(
            meshEntityData, shutterOffsets, inputData, outputData,
            customPrimvars);
        break;
    default:
        return;
    }

    // fur?

    if (_args.enableFur) {
        size_t nfur = outputData._furIdsArray.size();
        meshEntityData.fur.reserve(nfur);

        for (size_t ifur = 0; ifur < nfur; ++ifur) {

            // find the material for the fur's shading group

            SdfPath furmat = FindMaterialForShadingGroup(
                inputData._character, outputData._furShadingGroups[ifur]);

            // construct an instance of FurAdapter to generate Hydra data
            // sources for the fur's topology, geometry, material and custom
            // primvars

            const glm::crowdio::FurIds& furids = outputData._furIdsArray[ifur];

            std::shared_ptr<FurAdapter> furAdapter =
                std::make_shared<FurAdapter>(
                    outputData._furCacheArray[furids._furCacheIdx],
                    furids._meshInFurIdx, simData->_scales[entityIndex],
                    furmat, customPrimvars, _args.furRenderPercent,
                    _args.furRefineLevel);

            if (motionBlur) {
                furAdapter->SetGeometry(
                    shutterOffsets, outputData._deformedFurVertices, ifur);
            } else {
                furAdapter->SetGeometry(
                    outputData._deformedFurVertices[0][ifur]);
            }

            meshEntityData.fur.emplace_back(furAdapter);
        }
    }
}

/*
 * Generates meshes for the given GCG character entity at the given frame, and
 * adds them to the MeshEntityData's meshes vector.
 */
void GolaemProcedural::GenerateGCGMeshes(
    MeshEntityData& meshEntityData,
    const GlmSimulationData *simData, const GlmFrameData *frameData,
    const glm::Array<Time>& shutterOffsets,
    const glm::crowdio::InputEntityGeoData& inputData,
    const glm::crowdio::OutputEntityGeoData& outputData,
    const PrimvarDSMapRef& customPrimvars)
{
    CrowdGcgCharacter *gcgCharacter = outputData._gcgCharacters[0];
    const GlmGeometryFile& geoFile = gcgCharacter->getGeometry();
    size_t meshCount = outputData._meshAssetNameIndices.size();
    meshEntityData.meshes.reserve(meshCount);

    for (size_t imesh = 0; imesh < meshCount; ++imesh) {

        // fetch the mesh itself

        const GlmFileMeshTransform& meshXform =
            geoFile._transforms[outputData._transformIndicesInGcgFile[imesh]];
        const GlmFileMesh& fileMesh = geoFile._meshes[meshXform._meshIndex];

        // find the material for the mesh's shading group

        SdfPath material = FindMaterialForShadingGroup(
            inputData._character, outputData._meshShadingGroups[imesh]);

        // construct an instance of FileMeshAdapter to generate Hydra data
        // sources for the mesh's topology and geometry; if the mesh is rigid,
        // we can cache the FileMeshAdapter and reuse it for all instances of
        // the same mesh

        std::shared_ptr<FileMeshAdapter> adapter;
        bool isRigid = glmhydra::kEnableRigidEntities
            && fileMesh._skinningType == glm::crowdio::GLM_SKIN_RIGID;

        if (isRigid) {
            MeshKey meshKey;
            meshKey.characterIndex = inputData._characterIdx;
            if (inputData._geoFileIndex < 0) {
                meshKey.lodIndex =
                    static_cast<int32_t>(outputData._geometryFileIndexes[0]);
            } else {
                meshKey.lodIndex = 0;
            }
            meshKey.meshIndex = meshXform._meshIndex;
            auto it = _rigidMeshCache.find(meshKey);
            if (it == _rigidMeshCache.end()) {
                adapter = std::make_shared<FileMeshAdapter>(fileMesh);
                _rigidMeshCache[meshKey] = adapter;
            } else {
                adapter = it->second;
            }
        } else {
            adapter = std::make_shared<FileMeshAdapter>(fileMesh);
            if (shutterOffsets.size() > 1) {
                adapter->SetGeometry(
                    shutterOffsets, outputData._deformedVertices,
                    outputData._deformedNormals, imesh);
            } else {
                adapter->SetGeometry(
                    outputData._deformedVertices[0][imesh],
                    outputData._deformedNormals[0][imesh]);
            }
        }

        // construct a FileMeshInstance to add data sources for the mesh's
        // material, custom primvars and xform (if it is rigid)

        std::shared_ptr<FileMeshInstance> instance =
            std::make_shared<FileMeshInstance>(
                adapter, material, customPrimvars);

        if (isRigid) {

            // TODO: this is wrong! I don't know how to calculate the mesh's
            // transformation matrix correctly.

            auto boneIndex = meshXform._rigidSkinningBoneId;
            auto entityIndex = inputData._entityIndex;
            auto entityType = simData->_entityTypes[entityIndex];
            auto boneCount = simData->_boneCount[entityType];
            auto frameDataIndex = boneIndex
                + simData->_iBoneOffsetPerEntityType[entityType]
                + simData->_indexInEntityType[entityIndex] * boneCount;

            // TODO: if rigid body support is enabled one day, implement a
            // variant of SetTransform() with multiple samples for motion blur

            instance->SetTransform(
                frameData->_bonePositions[frameDataIndex],
                frameData->_boneOrientations[frameDataIndex],
                simData->_scales[entityIndex]);
        }

        meshEntityData.meshes.emplace_back(instance);
    }
}

/*
 * Generates meshes for the given FBX character entity at the given frame, and
 * adds them to the MeshEntityData's meshes vector.
 */
void GolaemProcedural::GenerateFBXMeshes(
    MeshEntityData& meshEntityData,
    const glm::Array<Time>& shutterOffsets,
    const glm::crowdio::InputEntityGeoData& inputData,
    const glm::crowdio::OutputEntityGeoData& outputData,
    const PrimvarDSMapRef& customPrimvars)
{
    glm::crowdio::CrowdFBXCharacter *fbxCharacter = outputData._fbxCharacters[0];

    // fbxTimes are needed to access the FBX mesh node's global transformation
    // matrix

    const glm::crowdio::GeometryBehaviorInfo& behavior = outputData._geoBeInfo;
    glm::Array<FbxTime> fbxTimes(shutterOffsets.size());

    if (behavior._idGeometryFileIdx >= 0) {
        FbxTime::EMode timeMode =
            fbxCharacter->touchFBXScene()->GetGlobalSettings().GetTimeMode();
        double frameRate = FbxTime::GetFrameRate(timeMode);
        for (int isample = 0; isample < shutterOffsets.size(); ++isample) {
            const float (&geoCacheData)[3] =
                inputData._frameDatas[isample]->
                _geoBehaviorAnimFrameInfo[behavior._geoDataIndex];
            float currentFrame = geoCacheData[0];
            fbxTimes[isample].SetGlobalTimeMode(FbxTime::eCustom, frameRate);
            fbxTimes[isample].SetMilliSeconds(
                std::llround(currentFrame / frameRate * 1000.0));
        }
    }

    // construct an instance of FbxMeshAdapter to generate Hydra data sources
    // for each mesh

    size_t meshCount = outputData._meshAssetNameIndices.size();
    meshEntityData.meshes.reserve(meshCount);

    for (size_t imesh = 0; imesh < meshCount; ++imesh) {
        SdfPath material = FindMaterialForShadingGroup(
            inputData._character, outputData._meshShadingGroups[imesh]);

        meshEntityData.meshes.emplace_back(
            std::make_shared<FbxMeshAdapter>(
                *fbxCharacter, outputData._meshAssetNameIndices[imesh],
                fbxTimes, shutterOffsets,
                outputData._deformedVertices, outputData._deformedNormals,
                outputData._meshAssetMaterialIndices[imesh],
                material, customPrimvars));
    }
}

/*
 * Entry point called by Hydra to ask what data sources of what prims the
 * procedural depends on, that is, what changes will cause Hydra to call
 * Update() again.
 */
HdGpGenerativeProcedural::DependencyMap
GolaemProcedural::UpdateDependencies(const HdSceneIndexBaseRefPtr &inputScene)
{
    DependencyMap result;

    // always call Update() when the current frame changes

    SdfPath primPath = HdSceneGlobalsSchema::GetDefaultPrimPath();
    result[primPath] = HdSceneGlobalsSchema::GetCurrentFrameLocator();

    // no motion blur or LOD in bbox display mode

    if (_args.displayMode == golaemTokens->bbox) {
        return result;
    }

    // update when the camera changes if motion blur or LOD is enabled (and note
    // the path of the camera prim for later)

    SdfPath camPath;

    if (_args.enableMotionBlur || _args.enableLod) {
        result[primPath].insert(
            HdSceneGlobalsSchema::GetPrimaryCameraPrimLocator());
        camPath = GetCameraPath(inputScene);
    }

    // update when the camera moves if LOD is enabled

    if (_args.enableLod && !camPath.IsEmpty()) {
        TF_DEBUG_MSG(
            GLMHYDRA_DEPENDENCIES,
            "[GolaemHydra] add dependency on camera xform: %s\n",
            camPath.GetAsString().c_str());
        result[camPath].insert(HdXformSchema::GetDefaultLocator());
    }

    // if motion blur is enabled, update when the render settings change or when
    // the shutter interval changes: if there is an active render settings prim,
    // get the shutter interval from there, otherwise use the primary camera's
    // shutter settings

    if (_args.enableMotionBlur) {
        result[primPath].insert(
            HdSceneGlobalsSchema::GetActiveRenderSettingsPrimLocator());

        const HdSceneGlobalsSchema globals =
            HdSceneGlobalsSchema::GetFromSceneIndex(inputScene);

        if (globals) {
            HdPathDataSourceHandle rsPrimDS =
                globals.GetActiveRenderSettingsPrim();
            if (rsPrimDS) {
                const SdfPath rsPath = rsPrimDS->GetTypedValue(0);
                if (!rsPath.IsEmpty()) {
                    TF_DEBUG_MSG(
                        GLMHYDRA_DEPENDENCIES,
                        "[GolaemHydra] add dependency on render settings shutter\n");
                    result[rsPath] = {
                        HdRenderSettingsSchema::GetShutterIntervalLocator()
                    };
                }
            } else if (!camPath.IsEmpty()) {
                TF_DEBUG_MSG(
                    GLMHYDRA_DEPENDENCIES,
                    "[GolaemHydra] add dependency on camera shutter: %s\n",
                    camPath.GetAsString().c_str());
                result[camPath].insert(
                    HdCameraSchema::GetShutterOpenLocator());
                result[camPath].insert(
                    HdCameraSchema::GetShutterCloseLocator());
            }
        }
    }

    return result;
}

/*
 * Entry point called by Hydra to "cook" the procedural. It returns a list of
 * the procedural's child prims and their types. If a given prim was already
 * present in the previous call to Update(), it also tells Hydra which of its
 * data sources may have changed since then.
 *
 * After Update() returns, Hydra will call GetChildPrim() (in multiple parallel
 * threads) for the actual content of each prim.
 */
HdGpGenerativeProcedural::ChildPrimTypeMap GolaemProcedural::Update(
    const HdSceneIndexBaseRefPtr& inputScene,
    const ChildPrimTypeMap& previousResult,
    const DependencyMap& dirtiedDependencies,
    HdSceneIndexObserver::DirtiedPrimEntries *outputDirtiedPrims)
{
    if (TfDebug::IsEnabled(GLMHYDRA_DEPENDENCIES)
        && dirtiedDependencies.size() > 0) {
        std::ostringstream strm;
        for (const auto& pair: dirtiedDependencies) {
            strm << "[GolaemHydra] dirtied prim: " << pair.first << " "
                 << pair.second << '\n';
        }
        TfDebug::Helper().Msg(strm.str());
    }

    // Fetch arguments (primvars) and see if any have changed since the last
    // update. Note that most changes to the arguments don't require us to do
    // anything in particular, because PopulateCrowd() and then GetChildPrim()
    // regenerate all the Hydra prims anyway. Most of the time, all we need to
    // do is to tell Hydra how each prim may have changed.

    Args newArgs = GetArgs(inputScene, _GetProceduralPrimPath());
    bool updateAll = false;
    bool updateFur = false;

    if (_updateCount == 0) {
        _args = newArgs;
        updateAll = true;
    } else {
        Args::ArgChanges changes = newArgs.compare(_args);
        if (changes != Args::kNoChanges) {
            _args = newArgs;
            switch (changes) {
            case Args::kFurChanges:
                updateFur = true;
                break;
            case Args::kReload:
                _factory->clear(glm::crowdio::FactoryClearMode::ALL);
                updateAll = true;
                break;
            }
        }
    }
    ++_updateCount;

    // the first time, or if any important attributes have changed, start over,
    // loading the cache, character files, layout and terrain files, etc.

    if (updateAll) {
        _dirmapRules = glm::stringToStringArray(_args.dirmap.GetString(), ";");
        findDirmappedFile(
            _mappedCacheDir, _args.cacheDir.GetString(), _dirmapRules);
        InitCrowd(inputScene);
    }

    // repopulate the scene (_bboxEntities or _meshEntities, depending on the
    // display mode)

    PopulateCrowd(inputScene);

    ChildPrimTypeMap result;
    SdfPath myPath = _GetProceduralPrimPath();
    char buffer[128];

    _childIndices.clear();
    _childIndexPairs.clear();

    // bbox display mode

    if (_args.displayMode == golaemTokens->bbox) {

        // generate a prim for each entity in the crowd

        for (size_t i = 0; i < _bboxEntities.size(); ++i) {
            std::snprintf(buffer, sizeof(buffer), "c%zu", i);
            SdfPath childPath = myPath.AppendChild(TfToken(buffer));
            result[childPath] = HdPrimTypeTokens->mesh;
            _childIndices[childPath] = i;

            // if the same path was generated by the previous call to Update(),
            // too, tell Hydra its xform may have changed

            if (previousResult.size() > 0) {
                outputDirtiedPrims->emplace_back(
                    childPath, HdXformSchema::GetDefaultLocator());
            }
        }
    }

    // mesh display mode

    else {

        // generate a prim for each mesh for each entity

        for (size_t i = 0; i < _meshEntities.size(); ++i) {
            const MeshEntityData& entity = _meshEntities[i];

            // including the crowd field, entity, LOD and mesh in the path
            // enables us to tell Hydra that, if the same prim appears in two
            // successive updates, only the points, normals and extent will have
            // changed (not the topology)

            // a group node that provides the extent for all the meshes below it

            std::snprintf(
                buffer, sizeof(buffer), "c%ue%zul%u",
                entity.crowdFieldIndex, entity.entityIndex, entity.lodIndex);
            SdfPath groupPath = myPath.AppendChild(TfToken(buffer));
            _childIndexPairs[groupPath] =
                {i, std::numeric_limits<size_t>::max()};

            if (previousResult.size() > 0) {
                outputDirtiedPrims->emplace_back(
                    groupPath, updateAll
                    ? HdDataSourceLocatorSet::UniversalSet()
                    : HdExtentSchema::GetDefaultLocator());
            }

            // a child node for each mesh

            for (size_t j = 0; j < entity.meshes.size(); ++j) {
                std::snprintf(buffer, sizeof(buffer), "m%zu", j);
                SdfPath childPath = groupPath.AppendChild(TfToken(buffer));
                result[childPath] = HdPrimTypeTokens->mesh;
                _childIndexPairs[childPath] = {i, j};

                if (previousResult.size() > 0) {
                    outputDirtiedPrims->emplace_back(
                        childPath, updateAll
                        ? HdDataSourceLocatorSet::UniversalSet()
                        : entity.meshes[j]->GetVariableDataSources());
                }
            }

            // and a child node for each fur cache

            for (size_t j = 0; j < entity.fur.size(); ++j) {
                std::snprintf(buffer, sizeof(buffer), "f%zu", j);
                SdfPath childPath = groupPath.AppendChild(TfToken(buffer));
                result[childPath] = HdPrimTypeTokens->basisCurves;
                _childIndexPairs[childPath] = {i, j};

                if (previousResult.size() > 0) {
                    outputDirtiedPrims->emplace_back(
                        childPath, updateAll || updateFur
                        ? HdDataSourceLocatorSet::UniversalSet()
                        : HdPrimvarsSchema::GetPointsLocator());
                }
            }
        }
    }

    return result;
}

/*
 * Entry point called by Hydra to retrieve the contents of a single prim. This
 * method may be called concurrently by multiple threads.
 */
HdSceneIndexPrim GolaemProcedural::GetChildPrim(
    const HdSceneIndexBaseRefPtr &/*inputScene*/, const SdfPath &childPrimPath)
{
    HdSceneIndexPrim result;

    // bbox display mode

    if (_args.displayMode == golaemTokens->bbox) {
        auto it = _childIndices.find(childPrimPath);
        if (it == _childIndices.end()) {
            return result;
        }

        const BBoxEntityData& entity = _bboxEntities[it->second];
        GfMatrix4d mtx;
        mtx.SetScale(entity.extent * entity.scale);
        mtx.SetTranslateOnly(entity.pos);

        result.primType = HdPrimTypeTokens->mesh;
        result.dataSource = HdRetainedContainerDataSource::New(
            HdXformSchemaTokens->xform,
            HdXformSchema::Builder()
            .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(mtx))
            .Build(),
            HdExtentSchemaTokens->extent,
            GetCubeExtentDataSource(),
            HdMeshSchemaTokens->mesh,
            GetCubeMeshDataSource(),
            HdPrimvarsSchemaTokens->primvars,
            GetCubePrimvarsDataSource());
    }

    // mesh display mode

    else {
        auto it = _childIndexPairs.find(childPrimPath);
        if (it == _childIndexPairs.end()) {
            return result;
        }

        size_t entityIndex = it->second.first;
        size_t subIndex = it->second.second;
        const MeshEntityData& meshEntity = _meshEntities[entityIndex];

        // the entity group node supplies the extent for all the meshes and/or
        // curves beneath it, but note that each mesh and curve must supply its
        // own xform, otherwise RenderMan refuses to render it!

        if (subIndex == std::numeric_limits<size_t>::max()) {
            result.primType = TfToken();
            result.dataSource = HdRetainedContainerDataSource::New(
                HdExtentSchemaTokens->extent,
                meshEntity.extent);
        }

        // mesh nodes

        else if (childPrimPath.GetName()[0] == 'm') {
            const std::shared_ptr<MeshDataSourceBase>& instance =
                meshEntity.meshes[subIndex];

            result.primType = HdPrimTypeTokens->mesh;
            result.dataSource = instance->GetDataSource();
        }

        // curve nodes for fur

        else {
            const std::shared_ptr<FurAdapter>& instance =
                meshEntity.fur[subIndex];

            result.primType = HdPrimTypeTokens->basisCurves;
            result.dataSource = instance->GetDataSource();
        }
    }

    return result;
}

}  // anonymous namespace

class GolaemProceduralPlugin: public HdGpGenerativeProceduralPlugin
{
public:
    GolaemProceduralPlugin() = default;

    HdGpGenerativeProcedural *Construct(
        const SdfPath &proceduralPrimPath) override
    {
        return new GolaemProcedural(proceduralPrimPath);
    }
};

TF_REGISTRY_FUNCTION(TfType)
{
    HdGpGenerativeProceduralPluginRegistry::Define<
        GolaemProceduralPlugin,
        HdGpGenerativeProceduralPlugin>();
}

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(
        GLMHYDRA_TRACE, "track loading of characters and cleanup");
    TF_DEBUG_ENVIRONMENT_SYMBOL(
        GLMHYDRA_DEPENDENCIES, "track dependencies and dirtied prims");
    TF_DEBUG_ENVIRONMENT_SYMBOL(
        GLMHYDRA_MOTION_BLUR, "motion blur debugging");
}
