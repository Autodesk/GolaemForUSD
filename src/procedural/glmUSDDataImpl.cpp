/***************************************************************************
 *                                                                          *
 *  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
 *                                                                          *
 ***************************************************************************/

#include "glmUSDDataImpl.h"
#include "glmUSDFileFormat.h"

USD_INCLUDES_START
#include <pxr/pxr.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/sdf/reference.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usd/tokens.h>
USD_INCLUDES_END

#include <glmCore.h>
#include <glmLog.h>
#include <glmFileDir.h>
#include <glmSimulationCacheLibrary.h>
#include <glmSimulationCacheInformation.h>
#include <glmGolaemCharacter.h>
#include <glmCrowdFBXStorage.h>
#include <glmCrowdFBXBaker.h>
#include <glmCrowdFBXCharacter.h>
#include <glmCrowdGcgCharacter.h>
#include <glmCrowdIOUtils.h>

#ifdef TRACY_ENABLE
#include <glmTracy.h>
#endif

#include <glmDistance.h>

#include <glmIdsFilter.h>

#include <fstream>

namespace glm
{
    namespace usdplugin
    {
        // All leaf prims have the same properties, so we set up some static data about
        // these properties that will always be true.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4003)
#endif
        // clang-format off
        // Define tokens for the property names we know about from usdGeom

        TF_DEFINE_PRIVATE_TOKENS(
            _skinMeshEntityPropertyTokens,
            ((xformOpOrder, "xformOpOrder"))
            ((xformOpTranslate, "xformOp:translate"))
            ((displayColor, "primvars:displayColor"))
            ((visibility, "visibility"))
            ((entityId, "entityId"))
            ((extentsHint, "extentsHint"))
            ((geometryTagId, "geometryTagId"))
            ((geometryFileId, "geometryFileId"))
            ((lodName, "lodName"))
        );

        TF_DEFINE_PRIVATE_TOKENS(
            _skelEntityPropertyTokens,
            ((visibility, "visibility"))
            ((entityId, "entityId"))
            ((extent, "extent"))
            ((geometryTagId, "geometryTagId"))
            ((geometryFileId, "geometryFileId"))
            ((lodName, "lodName"))
        );

        TF_DEFINE_PRIVATE_TOKENS(
            _skinMeshPropertyTokens,
            ((faceVertexCounts, "faceVertexCounts"))
            ((faceVertexIndices, "faceVertexIndices"))
            ((orientation, "orientation"))
            ((points, "points"))
            ((subdivisionScheme, "subdivisionScheme"))
            ((normals, "normals"))
            ((uvs, "primvars:st"))
            ((velocities, "velocities"))
        );

        TF_DEFINE_PRIVATE_TOKENS(
            _skinMeshLodPropertyTokens,
            ((visibility, "visibility"))
        );

        TF_DEFINE_PRIVATE_TOKENS(
            _skelEntityRelationshipTokens,
            ((animationSource, "skel:animationSource"))
            ((skeleton, "skel:skeleton"))
        );

        TF_DEFINE_PRIVATE_TOKENS(
            _skinMeshRelationshipTokens,
            ((materialBinding, "material:binding"))
        );

        TF_DEFINE_PRIVATE_TOKENS(
            _skelAnimPropertyTokens,
            ((joints, "joints"))
            ((rotations, "rotations"))
            ((scales, "scales"))
            ((translations, "translations"))
        );

        TF_DEFINE_PRIVATE_TOKENS(
            _golaemTokens,
            ((__glmNodeId__, "__glmNodeId__"))
            ((__glmNodeType__, "__glmNodeType__"))
            ((glmCameraPos, "glmCameraPos"))
        );
        // clang-format on
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        // We create a static map from property names to the info about them that
        // we'll be querying for specs.
        struct _PrimPropertyInfo
        {
            VtValue defaultValue;
            TfToken typeName;
            // Most of our properties are animated.
            bool isAnimated = true;
            bool hasInterpolation = false;
            TfToken interpolation;
        };

        using _LeafPrimPropertyMap =
            std::map<TfToken, _PrimPropertyInfo, TfTokenFastArbitraryLessThan>;

        struct _PrimRelationshipInfo
        {
            SdfPathListOp defaultTargetPath;
        };

        using _LeafPrimRelationshiphMap =
            std::map<TfToken, _PrimRelationshipInfo, TfTokenFastArbitraryLessThan>;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4459)
#endif

        TF_MAKE_STATIC_DATA(
            (_LeafPrimPropertyMap), _skinMeshEntityProperties)
        {

            // Define the default value types for our animated properties.
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->xformOpTranslate].defaultValue = VtValue(GfVec3f(0));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->xformOpTranslate].isAnimated = true;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->xformOpOrder].defaultValue = VtValue(VtTokenArray({_skinMeshEntityPropertyTokens->xformOpTranslate}));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->xformOpOrder].isAnimated = false;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->displayColor].defaultValue = VtValue(VtVec3fArray({GfVec3f(1, 0.5, 0)}));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->displayColor].isAnimated = false;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->extentsHint].defaultValue = VtValue(VtVec3fArray({GfVec3f(-0.5, -0.5, -0.5), GfVec3f(0.5, 0.5, 0.5)}));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->extentsHint].isAnimated = false;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->visibility].defaultValue = VtValue(UsdGeomTokens->inherited);
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->visibility].isAnimated = true;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->entityId].defaultValue = VtValue(int64_t(-1));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->entityId].isAnimated = false;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->geometryTagId].defaultValue = VtValue(int32_t(0));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->geometryTagId].isAnimated = false;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->geometryFileId].defaultValue = VtValue(int32_t(0));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->geometryFileId].isAnimated = true;

            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->lodName].defaultValue = VtValue(TfToken(""));
            (*_skinMeshEntityProperties)[_skinMeshEntityPropertyTokens->lodName].isAnimated = true;

            // Use the schema to derive the type name tokens from each property's
            // default value.
            for (auto& it : *_skinMeshEntityProperties)
            {
                it.second.typeName =
                    SdfSchema::GetInstance().FindType(it.second.defaultValue).GetAsToken();
            }
        }

        TF_MAKE_STATIC_DATA(
            (_LeafPrimPropertyMap), _skelEntityProperties)
        {

            // Define the default value types for our animated properties.
            (*_skelEntityProperties)[_skelEntityPropertyTokens->visibility].defaultValue = VtValue(UsdGeomTokens->inherited);
            (*_skelEntityProperties)[_skelEntityPropertyTokens->visibility].isAnimated = true;

            (*_skelEntityProperties)[_skelEntityPropertyTokens->entityId].defaultValue = VtValue(int64_t(-1));
            (*_skelEntityProperties)[_skelEntityPropertyTokens->entityId].isAnimated = false;

            (*_skelEntityProperties)[_skelEntityPropertyTokens->extent].defaultValue = VtValue(VtVec3fArray({GfVec3f(-0.5, -0.5, -0.5), GfVec3f(0.5, 0.5, 0.5)}));
            (*_skelEntityProperties)[_skelEntityPropertyTokens->extent].isAnimated = true;

            (*_skelEntityProperties)[_skelEntityPropertyTokens->geometryTagId].defaultValue = VtValue(int32_t(0));
            (*_skelEntityProperties)[_skelEntityPropertyTokens->geometryTagId].isAnimated = false;

            (*_skelEntityProperties)[_skelEntityPropertyTokens->geometryFileId].defaultValue = VtValue(int32_t(-1));
            (*_skelEntityProperties)[_skelEntityPropertyTokens->geometryFileId].isAnimated = false; // skel entities do not support dynamic lods

            (*_skelEntityProperties)[_skelEntityPropertyTokens->lodName].defaultValue = VtValue(TfToken(""));
            (*_skelEntityProperties)[_skelEntityPropertyTokens->lodName].isAnimated = false; // skel entities do not support dynamic lods

            // Use the schema to derive the type name tokens from each property's
            // default value.
            for (auto& it : *_skelEntityProperties)
            {
                it.second.typeName =
                    SdfSchema::GetInstance().FindType(it.second.defaultValue).GetAsToken();
            }
        }

        TF_MAKE_STATIC_DATA(
            (_LeafPrimPropertyMap), _skinMeshProperties)
        {
            // Define the default value types for our animated properties.
            (*_skinMeshProperties)[_skinMeshPropertyTokens->points].defaultValue = VtValue(VtVec3fArray());
            (*_skinMeshProperties)[_skinMeshPropertyTokens->points].isAnimated = true;

            (*_skinMeshProperties)[_skinMeshPropertyTokens->velocities].defaultValue = VtValue(VtVec3fArray());
            (*_skinMeshProperties)[_skinMeshPropertyTokens->velocities].isAnimated = true;

            (*_skinMeshProperties)[_skinMeshPropertyTokens->normals].defaultValue = VtValue(VtVec3fArray());
            (*_skinMeshProperties)[_skinMeshPropertyTokens->normals].isAnimated = true;
            (*_skinMeshProperties)[_skinMeshPropertyTokens->normals].hasInterpolation = true;
            (*_skinMeshProperties)[_skinMeshPropertyTokens->normals].interpolation = UsdGeomTokens->faceVarying;

            // set the subdivision scheme to none in order to take normals into account
            (*_skinMeshProperties)[_skinMeshPropertyTokens->subdivisionScheme].defaultValue = UsdGeomTokens->none;
            (*_skinMeshProperties)[_skinMeshPropertyTokens->subdivisionScheme].isAnimated = false;

            (*_skinMeshProperties)[_skinMeshPropertyTokens->faceVertexCounts].defaultValue = VtValue(VtIntArray());
            (*_skinMeshProperties)[_skinMeshPropertyTokens->faceVertexCounts].isAnimated = false;

            (*_skinMeshProperties)[_skinMeshPropertyTokens->faceVertexIndices].defaultValue = VtValue(VtIntArray());
            (*_skinMeshProperties)[_skinMeshPropertyTokens->faceVertexIndices].isAnimated = false;

            (*_skinMeshProperties)[_skinMeshPropertyTokens->uvs].defaultValue = VtValue(VtVec2fArray());
            (*_skinMeshProperties)[_skinMeshPropertyTokens->uvs].isAnimated = false;
            (*_skinMeshProperties)[_skinMeshPropertyTokens->uvs].hasInterpolation = true;
            (*_skinMeshProperties)[_skinMeshPropertyTokens->uvs].interpolation = UsdGeomTokens->faceVarying;

            (*_skinMeshProperties)[_skinMeshPropertyTokens->orientation].defaultValue = VtValue(UsdGeomTokens->rightHanded);
            (*_skinMeshProperties)[_skinMeshPropertyTokens->orientation].isAnimated = false;

            // Use the schema to derive the type name tokens from each property's
            // default value.
            for (auto& it : *_skinMeshProperties)
            {
                it.second.typeName =
                    SdfSchema::GetInstance().FindType(it.second.defaultValue).GetAsToken();
            }
        }

        TF_MAKE_STATIC_DATA(
            (_LeafPrimPropertyMap), _skinMeshLodProperties)
        {
            // Define the default value types for our animated properties.
            (*_skinMeshLodProperties)[_skinMeshLodPropertyTokens->visibility].defaultValue = VtValue(UsdGeomTokens->inherited);

            // Use the schema to derive the type name tokens from each property's
            // default value.
            for (auto& it : *_skinMeshLodProperties)
            {
                it.second.typeName =
                    SdfSchema::GetInstance().FindType(it.second.defaultValue).GetAsToken();
            }
        }

        TF_MAKE_STATIC_DATA(
            (_LeafPrimRelationshiphMap), _skinMeshRelationships)
        {
            (*_skinMeshRelationships)[_skinMeshRelationshipTokens->materialBinding].defaultTargetPath = SdfPathListOp::CreateExplicit({SdfPath("/Root/Materials/DefaultGolaemMat")});
        }

        TF_MAKE_STATIC_DATA(
            (_LeafPrimRelationshiphMap), _skelEntityRelationships)
        {
            (*_skelEntityRelationships)[_skelEntityRelationshipTokens->animationSource].defaultTargetPath = SdfPathListOp::CreateExplicit({SdfPath("Rig/SkelAnim")});
            (*_skelEntityRelationships)[_skelEntityRelationshipTokens->skeleton].defaultTargetPath = SdfPathListOp::CreateExplicit({SdfPath("Rig/Skel")});
        }

        TF_MAKE_STATIC_DATA(
            (_LeafPrimPropertyMap), _skelAnimProperties)
        {

            // Define the default value types for our animated properties.

            (*_skelAnimProperties)[_skelAnimPropertyTokens->joints].defaultValue = VtValue(VtTokenArray());
            (*_skelAnimProperties)[_skelAnimPropertyTokens->joints].isAnimated = false;

            (*_skelAnimProperties)[_skelAnimPropertyTokens->rotations].defaultValue = VtValue(VtQuatfArray());
            (*_skelAnimProperties)[_skelAnimPropertyTokens->rotations].isAnimated = true;

            (*_skelAnimProperties)[_skelAnimPropertyTokens->scales].defaultValue = VtValue(VtVec3hArray());
            (*_skelAnimProperties)[_skelAnimPropertyTokens->scales].isAnimated = true;

            (*_skelAnimProperties)[_skelAnimPropertyTokens->translations].defaultValue = VtValue(VtVec3fArray());
            (*_skelAnimProperties)[_skelAnimPropertyTokens->translations].isAnimated = true;

            // Use the schema to derive the type name tokens from each property's
            // default value.
            for (auto& it : *_skelAnimProperties)
            {
                it.second.typeName =
                    SdfSchema::GetInstance().FindType(it.second.defaultValue).GetAsToken();
            }
        }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

        // Helper function for getting the root prim path.
        static const SdfPath& _GetRootPrimPath()
        {
            static const SdfPath rootPrimPath("/Root");
            return rootPrimPath;
        }

// Helper macro for many of our functions need to optionally set an output
// VtValue when returning true.
#define RETURN_TRUE_WITH_OPTIONAL_VALUE(val) \
    if (value)                               \
    {                                        \
        *value = VtValue(val);               \
    }                                        \
    return true;

        static glm::Mutex _fbxMutex;

        //-----------------------------------------------------------------------------
        glm::crowdio::CrowdFBXStorage& getFbxStorage()
        {
            glm::ScopedLock<glm::Mutex> lock(_fbxMutex);
            static glm::crowdio::CrowdFBXStorage fbxStorage;
            return fbxStorage;
        }

        //-----------------------------------------------------------------------------
        glm::crowdio::CrowdFBXBaker& getFbxBaker()
        {
            glm::crowdio::CrowdFBXStorage& fbxStorage = getFbxStorage();
            glm::ScopedLock<glm::Mutex> lock(_fbxMutex);
            static glm::crowdio::CrowdFBXBaker fbxBaker(fbxStorage.touchFbxSdkManager());
            return fbxBaker;
        }

        //-----------------------------------------------------------------------------
        GolaemUSD_DataImpl::EntityData::~EntityData()
        {
            delete entityComputeLock;
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::EntityData::initEntityLock()
        {
            GLM_DEBUG_ASSERT(entityComputeLock == NULL);
            entityComputeLock = new glm::Mutex();
        }

        //-----------------------------------------------------------------------------
        GolaemUSD_DataImpl::GolaemUSD_DataImpl(const GolaemUSD_DataParams& params)
            : _params(params)
            , _factory(new crowdio::SimulationCacheFactory())
        {
            _rootNodeIdInFinalStage = usdplugin::init();
            _usdParams[_golaemTokens->__glmNodeId__] = _rootNodeIdInFinalStage;
            _usdParams[_golaemTokens->__glmNodeType__] = GolaemUSDFileFormatTokens->Id;
            if (_params.glmLodMode == 2)
            {
                // dynamic lod mode
                // add camera position parameter
                _usdParams[_golaemTokens->glmCameraPos] = _params.glmCameraPos;
            }
            _shaderAttrTypes.resize(ShaderAttributeType::END);
            _shaderAttrDefaultValues.resize(ShaderAttributeType::END);
            {
                int intValue = 0;
                VtValue value(intValue);
                _shaderAttrTypes[ShaderAttributeType::INT] = SdfSchema::GetInstance().FindType(value).GetAsToken();
                _shaderAttrDefaultValues[ShaderAttributeType::INT] = value;
            }
            {
                float floatValue = 0.1f;
                VtValue value(floatValue);
                _shaderAttrTypes[ShaderAttributeType::FLOAT] = SdfSchema::GetInstance().FindType(value).GetAsToken();
                _shaderAttrDefaultValues[ShaderAttributeType::FLOAT] = value;
            }
            {
                TfToken stringValue;
                VtValue value(stringValue);
                _shaderAttrTypes[ShaderAttributeType::STRING] = SdfSchema::GetInstance().FindType(value).GetAsToken();
                _shaderAttrDefaultValues[ShaderAttributeType::STRING] = value;
            }
            {
                GfVec3f vectorValue;
                VtValue value(vectorValue);
                _shaderAttrTypes[ShaderAttributeType::VECTOR] = SdfSchema::GetInstance().FindType(value).GetAsToken();
                _shaderAttrDefaultValues[ShaderAttributeType::VECTOR] = value;
            }
            // pp attributes have 2 possible types: float, vector
            _ppAttrTypes.resize(2);
            _ppAttrDefaultValues.resize(2);
            {
                float floatValue = 0.1f;
                VtValue value(floatValue);
                int attrTypeIdx = crowdio::GSC_PP_FLOAT - 1; // enum starts at 1
                _ppAttrTypes[attrTypeIdx] = SdfSchema::GetInstance().FindType(value).GetAsToken();
                _ppAttrDefaultValues[attrTypeIdx] = value;
            }
            {
                GfVec3f vectorValue;
                VtValue value(vectorValue);
                int attrTypeIdx = crowdio::GSC_PP_VECTOR - 1; // enum starts at 1
                _ppAttrTypes[attrTypeIdx] = SdfSchema::GetInstance().FindType(value).GetAsToken();
                _ppAttrDefaultValues[attrTypeIdx] = value;
            }
            _InitFromParams();
        }

        //-----------------------------------------------------------------------------
        GolaemUSD_DataImpl::~GolaemUSD_DataImpl()
        {
            delete _factory;
            for (glm::Mutex* lock : _cachedSimulationLocks)
            {
                delete lock;
            }
            _cachedSimulationLocks.clear();
            usdplugin::finish();
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::IsEmpty() const
        {
            return _primSpecPaths.empty();
        }

        //-----------------------------------------------------------------------------
        SdfSpecType GolaemUSD_DataImpl::GetSpecType(const SdfPath& path) const
        {
            // All specs are generated.
            if (path.IsPropertyPath()) // IsPropertyPath includes relational attributes
            {
                const TfToken& nameToken = path.GetNameToken();
                SdfPath primPath = path.GetAbsoluteRootOrPrimPath();

                if (primPath == _GetRootPrimPath())
                {
                    return SdfSpecTypeAttribute;
                }

                // A specific set of defined properties exist on the leaf prims only
                // as attributes. Non leaf prims have no properties.
                if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                {
                    if (TfMapLookupPtr(*_skelEntityProperties, nameToken) != NULL)
                    {
                        if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                        {
                            return SdfSpecTypeAttribute;
                        }
                    }
                    if (TfMapLookupPtr(*_skelEntityRelationships, nameToken) != NULL)
                    {
                        if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                        {
                            return SdfSpecTypeRelationship;
                        }
                    }
                    if (TfMapLookupPtr(*_skelAnimProperties, nameToken) != NULL)
                    {
                        if (TfMapLookupPtr(_skelAnimDataMap, primPath) != NULL)
                        {
                            return SdfSpecTypeAttribute;
                        }
                    }
                    if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                    {
                        if (TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken) != NULL ||
                            TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken) != NULL)
                        {
                            return SdfSpecTypeAttribute;
                        }
                    }
                }
                else
                {
                    if (TfMapLookupPtr(*_skinMeshEntityProperties, nameToken) != NULL)
                    {
                        if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                        {
                            return SdfSpecTypeAttribute;
                        }
                    }
                    if (TfMapLookupPtr(*_skinMeshLodProperties, nameToken) != NULL)
                    {
                        if (TfMapLookupPtr(_skinMeshLodDataMap, primPath) != NULL)
                        {
                            return SdfSpecTypeAttribute;
                        }
                    }
                    if (TfMapLookupPtr(*_skinMeshProperties, nameToken) != NULL)
                    {
                        if (TfMapLookupPtr(_skinMeshDataMap, primPath) != NULL)
                        {
                            return SdfSpecTypeAttribute;
                        }
                    }
                    if (TfMapLookupPtr(*_skinMeshRelationships, nameToken) != NULL)
                    {
                        if (TfMapLookupPtr(_skinMeshDataMap, primPath) != NULL)
                        {
                            return SdfSpecTypeRelationship;
                        }
                    }
                    if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                    {
                        if (TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken) != NULL ||
                            TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken) != NULL)
                        {
                            return SdfSpecTypeAttribute;
                        }
                    }
                }
            }
            else
            {
                // Special case for pseudoroot.
                if (path == SdfPath::AbsoluteRootPath())
                {
                    return SdfSpecTypePseudoRoot;
                }
                // All other valid prim spec paths are cached.
                if (_primSpecPaths.find(path) != _primSpecPaths.end())
                {
                    return SdfSpecTypePrim;
                }
            }

            return SdfSpecTypeUnknown;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::Has(const SdfPath& path, const TfToken& field, VtValue* value)
        {
            // If property spec, check property fields
            if (path.IsPropertyPath())
            {
                if (field == SdfFieldKeys->TypeName)
                {
                    return _HasPropertyTypeNameValue(path, value);
                }
                else if (field == SdfFieldKeys->Default)
                {
                    return _HasPropertyDefaultValue(path, value);
                }
                else if (field == UsdGeomTokens->interpolation)
                {
                    return _HasPropertyInterpolation(path, value);
                }
                else if (field == SdfFieldKeys->TargetPaths)
                {
                    return _HasTargetPathValue(path, value);
                }
                else if (field == SdfFieldKeys->TimeSamples)
                {
                    // Only animated properties have time samples.
                    if (_IsAnimatedProperty(path))
                    {
                        // Will need to generate the full SdfTimeSampleMap with a
                        // time sample value for each discrete animated frame if the
                        // value of the TimeSamples field is requested. Use a generator
                        // function in case we don't need to output the value as this
                        // can be expensive.
                        auto _MakeTimeSampleMap = [this, &path]()
                        {
                            SdfTimeSampleMap sampleMap;
                            for (auto& time : _animTimeSampleTimes)
                            {
                                QueryTimeSample(path, time, &sampleMap[time]);
                            }
                            return sampleMap;
                        };

                        RETURN_TRUE_WITH_OPTIONAL_VALUE(_MakeTimeSampleMap());
                    }
                }
            }
            else if (path == SdfPath::AbsoluteRootPath())
            {
                // Special case check for the pseudoroot prim spec.
                if (field == SdfChildrenKeys->PrimChildren)
                {
                    // Pseudoroot only has the root prim as a child
                    static TfTokenVector rootChildren({_GetRootPrimPath().GetNameToken()});
                    RETURN_TRUE_WITH_OPTIONAL_VALUE(rootChildren);
                }
                // Default prim is always the root prim.
                if (field == SdfFieldKeys->DefaultPrim)
                {
                    RETURN_TRUE_WITH_OPTIONAL_VALUE(_GetRootPrimPath().GetNameToken());
                }
                if (field == SdfFieldKeys->StartTimeCode)
                {
                    RETURN_TRUE_WITH_OPTIONAL_VALUE(double(_startFrame));
                }
                if (field == SdfFieldKeys->EndTimeCode)
                {
                    RETURN_TRUE_WITH_OPTIONAL_VALUE(double(_endFrame));
                }
                if (field == SdfFieldKeys->FramesPerSecond || field == SdfFieldKeys->TimeCodesPerSecond)
                {
                    RETURN_TRUE_WITH_OPTIONAL_VALUE(double(_fps));
                }
            }
            else
            {
                // Otherwise check prim spec fields.
                if (field == SdfFieldKeys->Specifier)
                {
                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        if (TfMapLookupPtr(_entityDataMap, path) != NULL)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(SdfSpecifierOver);
                        }
                        if (TfMapLookupPtr(_skelAnimDataMap, path) != NULL)
                        {
                            // SkelAnim node is defined
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(SdfSpecifierDef);
                        }
                    }
                    if (_primSpecPaths.find(path) != _primSpecPaths.end())
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(SdfSpecifierDef);
                    }
                }

                if (field == SdfFieldKeys->TypeName)
                {
                    // Only the leaf prim specs have a type name determined from the
                    // params.
                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        if (TfMapLookupPtr(_entityDataMap, path) != NULL)
                        {
                            // empty type for overrides
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken(""));
                        }
                        if (TfMapLookupPtr(_skelAnimDataMap, path) != NULL)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken("SkelAnimation"));
                        }
                    }
                    else
                    {
                        if (TfMapLookupPtr(_entityDataMap, path) != NULL || TfMapLookupPtr(_skinMeshLodDataMap, path) != NULL)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken("Xform"));
                        }

                        if (TfMapLookupPtr(_skinMeshDataMap, path) != NULL)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken("Mesh"));
                        }
                    }
                }

                if (field == UsdTokens->apiSchemas)
                {
                    if (_params.glmDisplayMode == GolaemDisplayMode::SKINMESH)
                    {
                        if (TfMapLookupPtr(_skinMeshDataMap, path) != NULL)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(SdfTokenListOp::CreateExplicit({TfToken("MaterialBindingAPI")}));
                        }
                    }
                }

                if (field == SdfFieldKeys->Kind)
                {
                    if (TfMapLookupPtr(_primChildNames, path) != NULL && TfMapLookupPtr(_entityDataMap, path) == NULL)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken("group"));
                    }

                    else if (TfMapLookupPtr(_entityDataMap, path) != NULL)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken("component"));
                    }
                }

                if (field == SdfFieldKeys->Active)
                {
                    SdfPath primPath = path.GetAbsoluteRootOrPrimPath();

                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(!(*entityDataPtr)->excluded);
                        }
                    }
                    else
                    {
                        if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(!(*entityDataPtr)->excluded);
                        }
                        if (SkinMeshLodMapData* lodMapData = TfMapLookupPtr(_skinMeshLodDataMap, primPath))
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(_params.glmLodMode == 2 || lodMapData->entityData->lodEnabled[lodMapData->lodIndex] > 0); // always active when not using static lod
                        }
                    }
                }

                if (field == SdfFieldKeys->References)
                {
                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        SdfPath primPath = path.GetAbsoluteRootOrPrimPath();
                        if (EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(static_cast<SkelEntityData*>(entityDataPtr->getImpl())->referencedUsdCharacter);
                        }
                    }
                }

                if (field == SdfFieldKeys->VariantSelection)
                {
                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        SdfPath primPath = path.GetAbsoluteRootOrPrimPath();
                        if (EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(static_cast<SkelEntityData*>(entityDataPtr->getImpl())->geoVariants);
                        }
                    }
                }

                if (field == SdfChildrenKeys->PrimChildren)
                {
                    // Non-leaf prims have the prim children. The list is the same set
                    // of prim child names for each non-leaf prim regardless of depth.

                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        if (TfMapLookupPtr(_entityDataMap, path) == NULL && TfMapLookupPtr(_skelAnimDataMap, path) == NULL)
                        {
                            if (const std::vector<TfToken>* childNames = TfMapLookupPtr(_primChildNames, path))
                            {
                                RETURN_TRUE_WITH_OPTIONAL_VALUE(*childNames);
                            }
                        }
                    }
                    else
                    {
                        if (TfMapLookupPtr(_skinMeshDataMap, path) == NULL)
                        {
                            if (const std::vector<TfToken>* childNames = TfMapLookupPtr(_primChildNames, path))
                            {
                                RETURN_TRUE_WITH_OPTIONAL_VALUE(*childNames);
                            }
                        }
                    }
                }

                if (field == SdfChildrenKeys->PropertyChildren)
                {
                    if (path == _GetRootPrimPath())
                    {
                        std::vector<TfToken> usdTokens;
                        for (const auto& itDict : _usdParams)
                        {
                            usdTokens.push_back(TfToken(itDict.first));
                        }
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(usdTokens);
                    }
                    // Leaf prims have the same specified set of property children.
                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        if (EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, path))
                        {
                            std::vector<TfToken> entityTokens = _skelEntityPropertyTokens->allTokens;
                            entityTokens.insert(entityTokens.end(), _skelEntityRelationshipTokens->allTokens.begin(), _skelEntityRelationshipTokens->allTokens.end());
                            // add pp attributes
                            for (const auto& itAttr : (*entityDataPtr)->ppAttrIndexes)
                            {
                                entityTokens.push_back(itAttr.first);
                            }
                            // add shader attributes
                            for (const auto& itAttr : (*entityDataPtr)->shaderAttrIndexes)
                            {
                                entityTokens.push_back(itAttr.first);
                            }
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(entityTokens);
                        }
                        if (TfMapLookupPtr(_skelAnimDataMap, path) != NULL)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(_skelAnimPropertyTokens->allTokens);
                        }
                    }
                    else
                    {
                        if (EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, path))
                        {
                            std::vector<TfToken> entityTokens = _skinMeshEntityPropertyTokens->allTokens;
                            // add pp attributes
                            for (const auto& itAttr : (*entityDataPtr)->ppAttrIndexes)
                            {
                                entityTokens.push_back(itAttr.first);
                            }
                            // add shader attributes
                            for (const auto& itAttr : (*entityDataPtr)->shaderAttrIndexes)
                            {
                                entityTokens.push_back(itAttr.first);
                            }
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(entityTokens);
                        }
                        if (TfMapLookupPtr(_skinMeshLodDataMap, path) != NULL)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(_skinMeshLodPropertyTokens->allTokens);
                        }
                        if (TfMapLookupPtr(_skinMeshDataMap, path) != NULL)
                        {
                            std::vector<TfToken> meshTokens = _skinMeshPropertyTokens->allTokens;
                            meshTokens.insert(meshTokens.end(), _skinMeshRelationshipTokens->allTokens.begin(), _skinMeshRelationshipTokens->allTokens.end());
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(meshTokens);
                        }
                    }
                }
            }
            return false;
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::VisitSpecs(const SdfAbstractData& data, SdfAbstractDataSpecVisitor* visitor) const
        {
            // Visit the pseudoroot.
            if (!visitor->VisitSpec(data, SdfPath::AbsoluteRootPath()))
            {
                return;
            }
            // Visit all the usd params.
            for (const auto& itDict : _usdParams)
            {
                if (!visitor->VisitSpec(data, _GetRootPrimPath().AppendProperty(TfToken(itDict.first))))
                {
                    return;
                }
            }

            // Visit all the cached prim spec paths.
            for (const auto& path : _primSpecPaths)
            {
                if (!visitor->VisitSpec(data, path))
                {
                    return;
                }
            }
            if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
            {
                // Visit the property specs which exist only on entity prims.
                for (auto& it : _entityDataMap)
                {
                    for (const TfToken& propertyName : _skelEntityPropertyTokens->allTokens)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(propertyName)))
                        {
                            return;
                        }
                    }
                    for (const TfToken& propertyName : _skelEntityRelationshipTokens->allTokens)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(propertyName)))
                        {
                            return;
                        }
                    }

                    for (const auto& itAttr : it.second->ppAttrIndexes)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(itAttr.first)))
                        {
                            return;
                        }
                    }

                    for (const auto& itAttr : it.second->shaderAttrIndexes)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(itAttr.first)))
                        {
                            return;
                        }
                    }
                }
                for (auto& it : _skelAnimDataMap)
                {
                    for (const TfToken& propertyName : _skelAnimPropertyTokens->allTokens)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(propertyName)))
                        {
                            return;
                        }
                    }
                }
            }
            else
            {
                // Visit the property specs which exist only on entity prims.
                for (auto& it : _entityDataMap)
                {
                    for (const TfToken& propertyName : _skinMeshEntityPropertyTokens->allTokens)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(propertyName)))
                        {
                            return;
                        }
                    }

                    for (const auto& itAttr : it.second->ppAttrIndexes)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(itAttr.first)))
                        {
                            return;
                        }
                    }

                    for (const auto& itAttr : it.second->shaderAttrIndexes)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(itAttr.first)))
                        {
                            return;
                        }
                    }
                }
                // Visit the property specs which exist only on lod prims.
                for (auto& it : _skinMeshLodDataMap)
                {
                    for (const TfToken& propertyName : _skinMeshLodPropertyTokens->allTokens)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(propertyName)))
                        {
                            return;
                        }
                    }
                }
                // Visit the property specs which exist only on entity mesh prims.
                for (auto& it : _skinMeshDataMap)
                {
                    for (const TfToken& propertyName : _skinMeshPropertyTokens->allTokens)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(propertyName)))
                        {
                            return;
                        }
                    }
                    for (const TfToken& propertyName : _skinMeshRelationshipTokens->allTokens)
                    {
                        if (!visitor->VisitSpec(data, it.first.AppendProperty(propertyName)))
                        {
                            return;
                        }
                    }
                }
            }
        }

        //-----------------------------------------------------------------------------
        const std::vector<TfToken>& GolaemUSD_DataImpl::List(const SdfPath& path) const
        {
            if (path.IsPropertyPath())
            {
                const TfToken& nameToken = path.GetNameToken();
                SdfPath primPath = path.GetAbsoluteRootOrPrimPath();
                // For properties, check that it's a valid leaf prim property
                static std::vector<TfToken> animPropFields(
                    {SdfFieldKeys->TypeName,
                     SdfFieldKeys->Default,
                     SdfFieldKeys->TimeSamples});
                static std::vector<TfToken> nonAnimPropFields(
                    {SdfFieldKeys->TypeName,
                     SdfFieldKeys->Default});
                static std::vector<TfToken> animInterpPropFields(
                    {SdfFieldKeys->TypeName,
                     SdfFieldKeys->Default,
                     SdfFieldKeys->TimeSamples,
                     UsdGeomTokens->interpolation});
                static std::vector<TfToken> nonAnimInterpPropFields(
                    {SdfFieldKeys->TypeName,
                     SdfFieldKeys->Default,
                     UsdGeomTokens->interpolation});
                static std::vector<TfToken> relationshipFields(
                    {SdfFieldKeys->TargetPaths});
                {
                    if (primPath == _GetRootPrimPath())
                    {
                        return nonAnimPropFields;
                    }
                    if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                    {
                        if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelEntityProperties, nameToken))
                        {
                            if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                            {
                                // Include time sample field in the property is animated.
                                if (propInfo->isAnimated)
                                {
                                    return animPropFields;
                                }
                                else
                                {
                                    return nonAnimPropFields;
                                }
                            }
                        }
                        if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelAnimProperties, nameToken))
                        {
                            if (const SkelEntityData::SP* skelEntityDataPtr = TfMapLookupPtr(_skelAnimDataMap, primPath))
                            {
                                // Include time sample field in the property is animated.
                                if (propInfo->isAnimated)
                                {
                                    if (nameToken == _skelAnimPropertyTokens->scales && !(*skelEntityDataPtr)->scalesAnimated)
                                    {
                                        // scales are not always animated
                                        return nonAnimPropFields;
                                    }
                                    return animPropFields;
                                }
                                else
                                {
                                    return nonAnimPropFields;
                                }
                            }
                        }
                        if (TfMapLookupPtr(*_skelEntityRelationships, nameToken) != NULL)
                        {
                            if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                            {
                                return relationshipFields;
                            }
                        }
                        if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                        {
                            if (TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken) != NULL ||
                                TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken) != NULL)
                            {
                                // pp or shader attributes are animated
                                return animPropFields;
                            }
                        }
                    }
                    else
                    {
                        if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshEntityProperties, nameToken))
                        {
                            if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                            {
                                // Include time sample field in the property is animated.
                                if (propInfo->isAnimated)
                                {
                                    return animPropFields;
                                }
                                else
                                {
                                    return nonAnimPropFields;
                                }
                            }
                        }
                        if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshLodProperties, nameToken))
                        {
                            if (TfMapLookupPtr(_skinMeshLodDataMap, primPath) != NULL)
                            {
                                // Include time sample field in the property is animated.
                                if (propInfo->isAnimated)
                                {
                                    return animPropFields;
                                }
                                else
                                {
                                    return nonAnimPropFields;
                                }
                            }
                        }
                        if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshProperties, nameToken))
                        {
                            if (TfMapLookupPtr(_skinMeshDataMap, primPath) != NULL)
                            {
                                // Include time sample field in the property is animated.
                                if (propInfo->isAnimated)
                                {
                                    if (propInfo->hasInterpolation)
                                    {
                                        return animInterpPropFields;
                                    }
                                    return animPropFields;
                                }
                                else
                                {
                                    if (propInfo->hasInterpolation)
                                    {
                                        return nonAnimInterpPropFields;
                                    }
                                    return nonAnimPropFields;
                                }
                            }
                        }
                        if (TfMapLookupPtr(*_skinMeshRelationships, nameToken) != NULL)
                        {
                            if (TfMapLookupPtr(_skinMeshDataMap, primPath) != NULL)
                            {
                                return relationshipFields;
                            }
                        }
                        if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                        {
                            if (TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken) != NULL ||
                                TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken) != NULL)
                            {
                                // pp or shader attributes are animated
                                return animPropFields;
                            }
                        }
                    }
                }
            }
            else if (path == SdfPath::AbsoluteRootPath())
            {
                // Pseudoroot fields.
                static std::vector<TfToken> pseudoRootFields(
                    {SdfChildrenKeys->PrimChildren,
                     SdfFieldKeys->DefaultPrim,
                     SdfFieldKeys->StartTimeCode,
                     SdfFieldKeys->EndTimeCode,
                     SdfFieldKeys->FramesPerSecond,
                     SdfFieldKeys->TimeCodesPerSecond});
                return pseudoRootFields;
            }
            else if (path == _GetRootPrimPath())
            {
                static std::vector<TfToken> rootPrimFields(
                    {SdfFieldKeys->Specifier,
                     SdfChildrenKeys->PrimChildren,
                     SdfChildrenKeys->PropertyChildren});
                return rootPrimFields;
            }
            else if (_primSpecPaths.find(path) != _primSpecPaths.end())
            {
                // Prim spec. Different fields for leaf and non-leaf prims.
                if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
                {
                    if (TfMapLookupPtr(_entityDataMap, path) != NULL)
                    {
                        static std::vector<TfToken> entityPrimFields(
                            {SdfFieldKeys->Specifier,
                             SdfFieldKeys->TypeName,
                             SdfFieldKeys->Active,
                             SdfFieldKeys->References,
                             SdfFieldKeys->VariantSelection,
                             SdfChildrenKeys->PrimChildren,
                             SdfChildrenKeys->PropertyChildren});
                        return entityPrimFields;
                    }
                    else if (TfMapLookupPtr(_skelAnimDataMap, path) != NULL)
                    {
                        static std::vector<TfToken> skelAnimPrimFields(
                            {SdfFieldKeys->Specifier,
                             SdfFieldKeys->TypeName,
                             SdfChildrenKeys->PropertyChildren});
                        return skelAnimPrimFields;
                    }
                    else
                    {
                        static std::vector<TfToken> nonLeafPrimFields(
                            {SdfFieldKeys->Specifier,
                             SdfChildrenKeys->PrimChildren});
                        return nonLeafPrimFields;
                    }
                }
                else
                {
                    // Prim spec. Different fields for leaf and non-leaf prims.
                    if (TfMapLookupPtr(_entityDataMap, path) != NULL)
                    {
                        static std::vector<TfToken> entityPrimFields(
                            {SdfFieldKeys->Specifier,
                             SdfFieldKeys->TypeName,
                             SdfFieldKeys->Active,
                             SdfChildrenKeys->PrimChildren,
                             SdfChildrenKeys->PropertyChildren});
                        return entityPrimFields;
                    }
                    else if (TfMapLookupPtr(_skinMeshLodDataMap, path) != NULL)
                    {
                        static std::vector<TfToken> lodPrimFields(
                            {SdfFieldKeys->Specifier,
                             SdfFieldKeys->TypeName,
                             SdfFieldKeys->Active,
                             SdfChildrenKeys->PrimChildren,
                             SdfChildrenKeys->PropertyChildren});
                        return lodPrimFields;
                    }
                    else if (TfMapLookupPtr(_skinMeshDataMap, path) != NULL)
                    {
                        static std::vector<TfToken> meshPrimFields(
                            {SdfFieldKeys->Specifier,
                             SdfFieldKeys->TypeName,
                             UsdTokens->apiSchemas,
                             SdfChildrenKeys->PropertyChildren});
                        return meshPrimFields;
                    }
                    else
                    {
                        static std::vector<TfToken> nonLeafPrimFields(
                            {SdfFieldKeys->Specifier,
                             SdfChildrenKeys->PrimChildren});
                        return nonLeafPrimFields;
                    }
                }
            }

            static std::vector<TfToken> empty;
            return empty;
        }

        //-----------------------------------------------------------------------------
        const std::set<double>& GolaemUSD_DataImpl::ListAllTimeSamples() const
        {
            // The set of all time sample times is cached.
            return _animTimeSampleTimes;
        }

        //-----------------------------------------------------------------------------
        const std::set<double>& GolaemUSD_DataImpl::ListTimeSamplesForPath(const SdfPath& path) const
        {
            // All animated properties use the same set of time samples; all other
            // specs return empty.
            if (_IsAnimatedProperty(path))
            {
                return ListAllTimeSamples();
            }
            static std::set<double> empty;
            return empty;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::GetBracketingTimeSamples(double time, double* tLower, double* tUpper) const
        {
            // A time sample time will exist at each discrete integer frame for the
            // duration of the generated animation and will already be cached.
            if (_animTimeSampleTimes.empty())
            {
                return false;
            }

            // First time sample is always _startFrame.
            if (time <= _startFrame)
            {
                *tLower = *tUpper = _startFrame;
                return true;
            }
            // Last time sample will always be _endFrame.
            if (time >= _endFrame)
            {
                *tLower = *tUpper = _endFrame;
                return true;
            }
            // set the lower and upper time to the same value
            *tLower = *tUpper = time;
            return true;
        }

        //-----------------------------------------------------------------------------
        size_t GolaemUSD_DataImpl::GetNumTimeSamplesForPath(const SdfPath& path) const
        {
            // All animated properties use the same set of time samples; all other specs
            // have no time samples.
            if (_IsAnimatedProperty(path))
            {
                return _animTimeSampleTimes.size();
            }
            return 0;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::GetBracketingTimeSamplesForPath(const SdfPath& path, double time, double* tLower, double* tUpper) const
        {
            // All animated properties use the same set of time samples.
            if (_IsAnimatedProperty(path))
            {
                return GetBracketingTimeSamples(time, tLower, tUpper);
            }

            return false;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::_QueryEntityAttributes(EntityFrameData::SP entityFrameData, const TfToken& nameToken, VtValue* value)
        {
            if (entityFrameData->enabled)
            {
                if (const size_t* ppAttrIdx = TfMapLookupPtr(entityFrameData->entityData->ppAttrIndexes, nameToken))
                {
                    if (value)
                    {
                        if (*ppAttrIdx < entityFrameData->floatPPAttrValues.size())
                        {
                            // this is a float PP attribute
                            size_t floatAttrIdx = *ppAttrIdx;
                            *value = VtValue(entityFrameData->floatPPAttrValues[floatAttrIdx]);
                        }
                        else
                        {
                            // this is a vector PP attribute
                            size_t vectAttrIdx = *ppAttrIdx - entityFrameData->floatPPAttrValues.size();
                            *value = VtValue(entityFrameData->vectorPPAttrValues[vectAttrIdx]);
                        }
                    }
                    return true;
                }
                if (const size_t* shaderAttrIdx = TfMapLookupPtr(entityFrameData->entityData->shaderAttrIndexes, nameToken))
                {
                    if (value)
                    {
                        const glm::ShaderAttribute& shaderAttr = entityFrameData->entityData->inputGeoData._character->_shaderAttributes[*shaderAttrIdx];
                        size_t specificAttrIdx = _globalToSpecificShaderAttrIdxPerChar[entityFrameData->entityData->inputGeoData._characterIdx][*shaderAttrIdx]; // no bounds check needed, characters are consistent across crowd fields
                        switch (shaderAttr._type)
                        {
                        case glm::ShaderAttributeType::INT:
                        {
                            glm::GlmString attrName, subAttrName;
                            glm::crowdio::RendererAttributeType::Value overrideType(glm::crowdio::RendererAttributeType::END);
                            glm::crowdio::parseRendererAttribute("arnold", shaderAttr._name, attrName, subAttrName, overrideType);
                            if (overrideType == glm::crowdio::RendererAttributeType::BOOL)
                            {
                                *value = VtValue(entityFrameData->intShaderAttrValues[specificAttrIdx] != 0);
                            }
                            else
                            {
                                *value = VtValue(entityFrameData->intShaderAttrValues[specificAttrIdx]);
                            }
                        }
                        break;
                        case glm::ShaderAttributeType::FLOAT:
                        {
                            *value = VtValue(entityFrameData->floatShaderAttrValues[specificAttrIdx]);
                        }
                        break;
                        case glm::ShaderAttributeType::STRING:
                        {
                            *value = VtValue(entityFrameData->stringShaderAttrValues[specificAttrIdx]);
                        }
                        break;
                        case glm::ShaderAttributeType::VECTOR:
                        {
                            *value = VtValue(entityFrameData->vectorShaderAttrValues[specificAttrIdx]);
                        }
                        break;
                        default:
                            break;
                        }
                    }
                    return true;
                }
            }
            return false;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::QueryTimeSample(const SdfPath& path, double frame, VtValue* value)
        {
            SdfPath primPath = path.GetAbsoluteRootOrPrimPath();
            const TfToken& nameToken = path.GetNameToken();

            // Only leaf prim properties have time samples
            EntityData::SP entityData = nullptr;
            if (EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
            {
                entityData = *entityDataPtr;
            }
            bool isEntityPath = entityData != nullptr;
            if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
            {
                if (entityData == nullptr)
                {
                    if (SkelEntityData::SP* skelEntityDataPtr = TfMapLookupPtr(_skelAnimDataMap, primPath))
                    {
                        entityData = *skelEntityDataPtr;
                    }
                }
                if (entityData == nullptr || entityData->excluded)
                {
                    return false;
                }

                // need to lock the wrapper until all the data is retrieved
                glm::ScopedLockActivable<glm::Mutex> wrapperLock(_usdWrapper._updateLock);
                _usdWrapper.update(frame, wrapperLock);

                // need to lock the entity until all the data is retrieved
                glm::ScopedLock<glm::Mutex> entityComputeLock(*entityData->entityComputeLock);
                SkelEntityFrameData::SP skelEntityFrameData = _ComputeSkelEntity(entityData, frame);

                if (isEntityPath)
                {
                    // this is an entity node
                    if (nameToken == _skelEntityPropertyTokens->visibility)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(skelEntityFrameData->enabled ? UsdGeomTokens->inherited : UsdGeomTokens->invisible);
                    }
                    if (nameToken == _skelEntityPropertyTokens->extent)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(VtVec3fArray({skelEntityFrameData->pos - entityData->extent, skelEntityFrameData->pos + entityData->extent}));
                    }

                    return _QueryEntityAttributes(skelEntityFrameData, nameToken, value);
                }
                else
                {
                    // this is a skel anim node
                    if (nameToken == _skelAnimPropertyTokens->rotations)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(skelEntityFrameData->rotations);
                    }
                    if (nameToken == _skelAnimPropertyTokens->scales)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(skelEntityFrameData->scales);
                    }
                    if (nameToken == _skelAnimPropertyTokens->translations)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(skelEntityFrameData->translations);
                    }
                }
            }
            else
            {
                bool isMeshLodPath = false;
                bool isMeshPath = false;
                size_t lodIndex = 0;
                int gchaMeshId = 0;
                int meshMaterialIndex = 0;
                if (entityData == nullptr)
                {
                    const SkinMeshMapData* meshMapData = TfMapLookupPtr(_skinMeshDataMap, primPath);
                    if (meshMapData != NULL)
                    {
                        entityData = meshMapData->entityData;
                        lodIndex = meshMapData->lodIndex;
                        gchaMeshId = meshMapData->gchaMeshId;
                        meshMaterialIndex = meshMapData->meshMaterialIndex;
                        isMeshPath = true;
                    }
                    else
                    {
                        const SkinMeshLodMapData* lodMapData = TfMapLookupPtr(_skinMeshLodDataMap, primPath);
                        if (lodMapData != NULL)
                        {
                            entityData = lodMapData->entityData;
                            lodIndex = lodMapData->lodIndex;
                            isMeshLodPath = true;
                        }
                    }
                }
                if (entityData == nullptr || entityData->excluded)
                {
                    return false;
                }

                // need to lock the wrapper until all the data is retrieved
                glm::ScopedLockActivable<glm::Mutex> wrapperLock(_usdWrapper._updateLock);
                _usdWrapper.update(frame, wrapperLock);

                // need to lock the entity until all the data is retrieved
                glm::ScopedLock<glm::Mutex> entityComputeLock(*entityData->entityComputeLock);
                SkinMeshEntityFrameData::SP prevFrameData;

                SkinMeshEntityData::SP skinMeshEntityData = glm::staticCast<SkinMeshEntityData>(entityData);

                if (skinMeshEntityData->computeVelocities && frame >= _startFrame + 1)
                {
                    prevFrameData = _ComputeSkinMeshEntity(entityData, frame - 1.0);
                }
                SkinMeshEntityFrameData::SP entityFrameData = _ComputeSkinMeshEntity(entityData, frame);
                _ComputeEntityVelocities(entityFrameData, prevFrameData);

                if (isEntityPath)
                {
                    // this is an entity node
                    if (nameToken == _skinMeshEntityPropertyTokens->xformOpTranslate)
                    {
                        // Animated position, anchored at the prim's layout position.
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(entityFrameData->pos);
                    }
                    if (nameToken == _skinMeshEntityPropertyTokens->visibility)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(entityFrameData->enabled ? UsdGeomTokens->inherited : UsdGeomTokens->invisible);
                    }
                    if (nameToken == _skinMeshEntityPropertyTokens->geometryFileId)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(entityFrameData->geometryFileIdx);
                    }
                    if (nameToken == _skinMeshEntityPropertyTokens->lodName)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(entityFrameData->lodName);
                    }
                    return _QueryEntityAttributes(entityFrameData, nameToken, value);
                }
                else if (isMeshLodPath)
                {
                    SkinMeshLodData::SP meshLodData = entityFrameData->meshLodData[lodIndex];
                    if (nameToken == _skinMeshLodPropertyTokens->visibility)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(_params.glmLodMode == 1 || meshLodData->enabled ? UsdGeomTokens->inherited : UsdGeomTokens->invisible);
                    }
                }
                else if (isMeshPath)
                {
                    // this is a mesh node

                    bool useTemplateData = false;
                    if (!entityFrameData->enabled)
                    {
                        // entity is disabled, use the mesh template data
                        useTemplateData = true;
                    }
                    else
                    {
                        SkinMeshLodData::SP meshLodData = entityFrameData->meshLodData[lodIndex];
                        if (!meshLodData->enabled)
                        {
                            // this is a mesh from an inactive lod, use the mesh template data
                            useTemplateData = true;
                        }
                        else
                        {
                            SkinMeshData::SP meshData = meshLodData->meshData.at({gchaMeshId, meshMaterialIndex});
                            if (meshData != NULL)
                            {
                                if (nameToken == _skinMeshPropertyTokens->points)
                                {
                                    RETURN_TRUE_WITH_OPTIONAL_VALUE(meshData->points);
                                }
                                if (nameToken == _skinMeshPropertyTokens->normals)
                                {
                                    RETURN_TRUE_WITH_OPTIONAL_VALUE(meshData->normals);
                                }
                                if (nameToken == _skinMeshPropertyTokens->velocities)
                                {
                                    if (!skinMeshEntityData->computeVelocities || meshData->velocities.empty())
                                    {
                                        return false;
                                    }
                                    RETURN_TRUE_WITH_OPTIONAL_VALUE(meshData->velocities);
                                }
                            }
                        }
                    }

                    if (useTemplateData)
                    {
                        auto& characterTemplateData = _skinMeshTemplateDataPerCharPerGeomFile[entityData->inputGeoData._characterIdx];
                        const auto& lodTemplateData = characterTemplateData[lodIndex];
                        SkinMeshTemplateData::SP meshTemplateData = lodTemplateData.at({gchaMeshId, meshMaterialIndex});
                        if (nameToken == _skinMeshPropertyTokens->points)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(meshTemplateData->defaultPoints);
                        }
                        if (nameToken == _skinMeshPropertyTokens->normals)
                        {
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(meshTemplateData->defaultNormals);
                        }
                        if (nameToken == _skinMeshPropertyTokens->velocities)
                        {
                            if (!skinMeshEntityData->computeVelocities || meshTemplateData->defaultVelocities.empty())
                            {
                                return false;
                            }
                            RETURN_TRUE_WITH_OPTIONAL_VALUE(meshTemplateData->defaultVelocities);
                        }
                    }
                }
            }

            return false;
        }

        //-----------------------------------------------------------------------------
        void loadSimulationCacheLib(glm::crowdio::SimulationCacheLibrary& simuCacheLibrary, const glm::GlmString& cacheLibPath)
        {
            if (!cacheLibPath.empty() && glm::FileDir::exist(cacheLibPath.c_str()))
            {
                std::ifstream inFile(cacheLibPath.c_str());
                if (inFile.is_open())
                {
                    inFile.seekg(0, std::ios::end);
                    size_t fileSize = inFile.tellg();
                    inFile.seekg(0);

                    std::string fileContents(fileSize + 1, '\0');
                    inFile.read(&fileContents[0], fileSize);
                    simuCacheLibrary.loadLibrary(fileContents.c_str(), fileContents.size(), false);
                }
                else
                {
                    GLM_CROWD_TRACE_ERROR("Failed to open Golaem simulation cache library file '" << cacheLibPath << "'");
                }
            }
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_InitFromParams()
        {
#ifdef TRACY_ENABLE
            ZoneScopedNC("InitFromParams", GLM_COLOR_CACHE);
#endif

            _startFrame = INT_MAX;
            _endFrame = INT_MIN;
            _fps = -1;

            glm::GlmString correctedFilePath;
            glm::Array<glm::GlmString> dirmapRules = glm::stringToStringArray(_params.glmDirmap.GetText(), ";");

            glm::crowdio::SimulationCacheLibrary simuCacheLibrary;
            findDirmappedFile(correctedFilePath, _params.glmCacheLibFile.GetText(), dirmapRules);
            loadSimulationCacheLib(simuCacheLibrary, correctedFilePath);

            glm::GlmString cfNames;
            glm::GlmString cacheName;
            glm::GlmString cacheDir;
            glm::GlmString characterFiles;
            glm::GlmString srcTerrainFile;
            glm::GlmString dstTerrainFile;
            bool enableLayout = false;
            glm::GlmString layoutFiles;

            glm::GlmString usdCharacterFiles;

            glm::crowdio::SimulationCacheInformation* cacheInfo = simuCacheLibrary.getCacheInformationByItemName(_params.glmCacheLibItem.GetText());
            if (cacheInfo == NULL && simuCacheLibrary.getCacheInformationCount() > 0)
            {
                GLM_CROWD_TRACE_WARNING("Could not find simulation cache item '" << _params.glmCacheLibItem.GetText() << "' in library file '" << _params.glmCacheLibFile.GetText() << "'");
                cacheInfo = &simuCacheLibrary.getCacheInformation(0);
            }

            if (cacheInfo != NULL)
            {
                cfNames = cacheInfo->_crowdFields;
                cacheName = cacheInfo->_cacheName;
                cacheDir = cacheInfo->_cacheDir;
                characterFiles = cacheInfo->_characterFiles;
                dstTerrainFile = cacheInfo->_destTerrain;
                enableLayout = cacheInfo->_enableLayout;
                layoutFiles = cacheInfo->_layoutFile;
                layoutFiles.trim(";");
            }
            // override cacheInfo params if neeeded
            if (!_params.glmCrowdFields.IsEmpty())
            {
                cfNames = _params.glmCrowdFields.GetText();
            }
            if (!_params.glmCacheName.IsEmpty())
            {
                cacheName = _params.glmCacheName.GetText();
            }
            if (!_params.glmCacheDir.IsEmpty())
            {
                cacheDir = _params.glmCacheDir.GetText();
            }
            if (!_params.glmCharacterFiles.IsEmpty())
            {
                characterFiles = _params.glmCharacterFiles.GetText();
            }
            if (!_params.glmTerrainFile.IsEmpty())
            {
                dstTerrainFile = _params.glmTerrainFile.GetText();
            }
            enableLayout = _params.glmEnableLayout;
            if (!_params.glmLayoutFiles.IsEmpty())
            {
                layoutFiles = _params.glmLayoutFiles.GetText();
            }

            if (!_params.glmUsdCharacterFiles.IsEmpty())
            {
                usdCharacterFiles = _params.glmUsdCharacterFiles.GetText();
            }

            float renderPercent = _params.glmRenderPercent * 0.01f;

            // terrain file
            glm::Array<glm::GlmString> crowdFieldNames = glm::stringToStringArray(cfNames.c_str(), ";");
            if (crowdFieldNames.size())
                srcTerrainFile = cacheDir + "/" + cacheName + "." + crowdFieldNames[0] + ".gtg";

            GolaemDisplayMode::Value displayMode = (GolaemDisplayMode::Value)_params.glmDisplayMode;

            GlmString attributeNamespace = _params.glmAttributeNamespace.GetText();
            attributeNamespace.rtrim(":");

            // dirmap character files
            {
                glm::Array<glm::GlmString> characterFilesList;
                split(characterFiles, ";", characterFilesList);
                for (size_t iCharFile = 0, charFileSize = characterFilesList.size(); iCharFile < charFileSize; ++iCharFile)
                {
                    const glm::GlmString& characterFile = characterFilesList[iCharFile];
                    findDirmappedFile(correctedFilePath, characterFile, dirmapRules);
                    characterFilesList[iCharFile] = correctedFilePath;
                }
                characterFiles = glm::stringArrayToString(characterFilesList, ";");
            }

            glm::Array<glm::GlmString> usdCharacterFilesList;
            split(usdCharacterFiles, ";", usdCharacterFilesList);
            for (size_t iCharFile = 0, charFileSize = usdCharacterFilesList.size(); iCharFile < charFileSize; ++iCharFile)
            {
                const glm::GlmString& usdCharacterFile = usdCharacterFilesList[iCharFile];
                findDirmappedFile(correctedFilePath, usdCharacterFile, dirmapRules);
                usdCharacterFilesList[iCharFile] = correctedFilePath;
            }

            _factory->loadGolaemCharacters(characterFiles.c_str());

            glm::Array<glm::GlmString> layoutFilesArray = glm::stringToStringArray(layoutFiles, ";");
            size_t layoutCount = layoutFilesArray.size();
            if (enableLayout && layoutCount > 0)
            {
                for (size_t iLayout = 0; iLayout < layoutCount; ++iLayout)
                {
                    const glm::GlmString& layoutFile = layoutFilesArray[iLayout];
                    // dirmap layout file
                    findDirmappedFile(correctedFilePath, layoutFile, dirmapRules);
                    if (correctedFilePath.length() > 0)
                    {
                        _factory->loadLayoutHistoryFile(_factory->getLayoutHistoryCount(), correctedFilePath.c_str());
                    }
                }
            }

            glm::crowdio::crowdTerrain::TerrainMesh* sourceTerrain = NULL;
            glm::crowdio::crowdTerrain::TerrainMesh* destTerrain = NULL;
            if (!srcTerrainFile.empty())
            {
                // dirmap terrain file
                findDirmappedFile(correctedFilePath, srcTerrainFile, dirmapRules);
                sourceTerrain = glm::crowdio::crowdTerrain::loadTerrainAsset(correctedFilePath.c_str());
            }
            if (!dstTerrainFile.empty())
            {
                // dirmap terrain file
                findDirmappedFile(correctedFilePath, dstTerrainFile, dirmapRules);
                destTerrain = glm::crowdio::crowdTerrain::loadTerrainAsset(correctedFilePath.c_str());
            }
            if (destTerrain == NULL)
            {
                destTerrain = sourceTerrain;
            }
            _factory->setTerrainMeshes(sourceTerrain, destTerrain);

            // dirmap cache dir
            findDirmappedFile(correctedFilePath, cacheDir, dirmapRules);
            cacheDir = correctedFilePath;

            glm::Array<std::pair<int, int>> frameRangesPerCrowdField;

            // force creating the simulation data (might change golaem characters if there is a CreateEntity node)
            for (size_t iCf = 0, cfCount = crowdFieldNames.size(); iCf < cfCount; ++iCf)
            {
                const glm::GlmString& glmCfName = crowdFieldNames[iCf];
                if (glmCfName.empty())
                {
                    continue;
                }

                glm::crowdio::CachedSimulation& cachedSimulation = _factory->getCachedSimulation(cacheDir.c_str(), cacheName.c_str(), glmCfName.c_str());
                const glm::crowdio::GlmSimulationData* simuData = cachedSimulation.getFinalSimulationData();
                if (simuData == nullptr)
                {
                    GLM_CROWD_TRACE_ERROR("Could not read simulation data for cache '" << cacheName << "' and Crowd Field '" << glmCfName << "' in cache directory '" << cacheDir << "': " << glm::crowdio::glmConvertSimulationCacheStatus(cachedSimulation.getFinalSimulationStatus()));
                }

                int firstFrameInCache = 0, lastFrameInCache = 0;
                cachedSimulation.getSrcFrameRangeAvailableOnDisk(firstFrameInCache, lastFrameInCache);
                frameRangesPerCrowdField.push_back({firstFrameInCache, lastFrameInCache});

                // Initialize the global-to-specific shader attribute indices once, from the first valid crowd field.
                if (simuData != nullptr && _globalToSpecificShaderAttrIdxPerChar.empty())
                {
                    const glm::ShaderAssetDataContainer* shaderDataContainer = cachedSimulation.getFinalShaderData(firstFrameInCache, UINT32_MAX, true);
                    if (shaderDataContainer != nullptr)
                    {
                        _globalToSpecificShaderAttrIdxPerChar = shaderDataContainer->globalToSpecificShaderAttrIdxPerChar;
                    }
                }
            }

            // Layer always has a root spec that is the default prim of the layer.
            _primSpecPaths.insert(_GetRootPrimPath());
            std::vector<TfToken>& rootChildNames = _primChildNames[_GetRootPrimPath()];

            _sgToSsPerChar.resize(_factory->getGolaemCharacters().size());
            _snsIndicesPerChar.resize(_factory->getGolaemCharacters().size());
            _jointsPerChar.resize(_factory->getGolaemCharacters().size());
            for (int iChar = 0, charCount = _factory->getGolaemCharacters().sizeInt(); iChar < charCount; ++iChar)
            {
                const glm::GolaemCharacter* character = _factory->getGolaemCharacter(iChar);
                if (character == NULL)
                {
                    continue;
                }

                glm::PODArray<int>& shadingGroupToSurfaceShader = _sgToSsPerChar[iChar];
                shadingGroupToSurfaceShader.resize(character->_shadingGroups.size(), -1);
                for (size_t iSg = 0, sgCount = character->_shadingGroups.size(); iSg < sgCount; ++iSg)
                {
                    const glm::ShadingGroup& shadingGroup = character->_shadingGroups[iSg];
                    int shaderAssetIdx = character->findShaderAsset(shadingGroup, "surface");
                    if (shaderAssetIdx >= 0)
                    {
                        shadingGroupToSurfaceShader[iSg] = shaderAssetIdx;
                    }
                }

                PODArray<int>& characterSnsIndices = _snsIndicesPerChar[iChar];
                VtTokenArray& characterJoints = _jointsPerChar[iChar];
                characterJoints.resize(character->_converterMapping._skeletonDescription->getBones().size());
                GlmString boneNameWithHierarchy;
                for (int iBone = 0, boneCount = character->_converterMapping._skeletonDescription->getBones().sizeInt(); iBone < boneCount; ++iBone)
                {
                    if (character->_converterMapping.isBoneUsingSnSScale(iBone))
                    {
                        characterSnsIndices.push_back(iBone);
                    }

                    const HierarchicalBone* bone = character->_converterMapping._skeletonDescription->getBones()[iBone];

                    boneNameWithHierarchy = TfMakeValidIdentifier(bone->getName().c_str());

                    for (const HierarchicalBone* parentBone = bone->getFather(); parentBone != NULL; parentBone = parentBone->getFather())
                    {
                        boneNameWithHierarchy = TfMakeValidIdentifier(parentBone->getName().c_str()) + "/" + boneNameWithHierarchy;
                    }
                    characterJoints[iBone] = TfToken(boneNameWithHierarchy.c_str());
                }
            }

            if (displayMode == GolaemDisplayMode::SKINMESH)
            {
                _skinMeshTemplateDataPerCharPerGeomFile.resize(_factory->getGolaemCharacters().size());

                PODArray<int> meshAssets;

                for (int iChar = 0, charCount = _factory->getGolaemCharacters().sizeInt(); iChar < charCount; ++iChar)
                {
                    const glm::GolaemCharacter* character = _factory->getGolaemCharacter(iChar);
                    if (character == NULL)
                    {
                        continue;
                    }
                    auto& characterTemplateData = _skinMeshTemplateDataPerCharPerGeomFile[iChar];

                    glm::crowdio::InputEntityGeoData inputGeoData;
                    inputGeoData._fbxStorage = &getFbxStorage();
                    inputGeoData._fbxBaker = &getFbxBaker();
                    inputGeoData._geometryTag = _params.glmGeometryTag;

                    inputGeoData._dirMapRules = dirmapRules;
                    inputGeoData._entityId = -1;
                    inputGeoData._simuData = NULL;
                    inputGeoData._entityToBakeIndex = -1;
                    inputGeoData._character = character;
                    inputGeoData._characterIdx = iChar;

                    size_t geoCount = character->getGeometryAssetsCount(inputGeoData._geometryTag);
                    characterTemplateData.resize(geoCount);

                    // add all assets
                    meshAssets.resize(character->_meshAssets.size());
                    for (int iMeshAsset = 0, meshAssetCount = character->_meshAssets.sizeInt(); iMeshAsset < meshAssetCount; ++iMeshAsset)
                    {
                        meshAssets[iMeshAsset] = iMeshAsset;
                    }
                    inputGeoData._assets = &meshAssets;

                    for (size_t iGeo = 0; iGeo < geoCount; ++iGeo)
                    {
                        inputGeoData._geoFileIndex = static_cast<int32_t>(iGeo);
                        glm::crowdio::OutputEntityGeoData outputData; // TODO: see if storage is better
                        glm::crowdio::GlmGeometryGenerationStatus geoStatus = glm::crowdio::glmPrepareEntityGeometry(&inputGeoData, &outputData);
                        if (geoStatus == glm::crowdio::GIO_SUCCESS)
                        {
                            _ComputeSkinMeshTemplateData(characterTemplateData[iGeo], inputGeoData, outputData);
                        }
                    }
                }
            }
            else if (displayMode == GolaemDisplayMode::BOUNDING_BOX)
            {
                _params.glmLodMode = 0; // no lod in bounding box mode
                _skinMeshTemplateDataPerCharPerGeomFile.resize(1);
                auto& characterTemplateData = _skinMeshTemplateDataPerCharPerGeomFile[0];
                characterTemplateData.resize(1);
                auto& lodTemplateData = characterTemplateData[0];
                SkinMeshTemplateData::SP templateData = new SkinMeshTemplateData();
                lodTemplateData[{0, 0}] = templateData;
                templateData->faceVertexCounts.resize(6);
                for (size_t iFace = 0; iFace < 6; ++iFace)
                {
                    templateData->faceVertexCounts[iFace] = 4;
                }

                // face 0
                templateData->faceVertexIndices.push_back(3);
                templateData->faceVertexIndices.push_back(2);
                templateData->faceVertexIndices.push_back(1);
                templateData->faceVertexIndices.push_back(0);

                // face 1
                templateData->faceVertexIndices.push_back(2);
                templateData->faceVertexIndices.push_back(6);
                templateData->faceVertexIndices.push_back(5);
                templateData->faceVertexIndices.push_back(1);

                // face 2
                templateData->faceVertexIndices.push_back(3);
                templateData->faceVertexIndices.push_back(7);
                templateData->faceVertexIndices.push_back(6);
                templateData->faceVertexIndices.push_back(2);

                // face 3
                templateData->faceVertexIndices.push_back(0);
                templateData->faceVertexIndices.push_back(4);
                templateData->faceVertexIndices.push_back(7);
                templateData->faceVertexIndices.push_back(3);

                // face 4
                templateData->faceVertexIndices.push_back(1);
                templateData->faceVertexIndices.push_back(5);
                templateData->faceVertexIndices.push_back(4);
                templateData->faceVertexIndices.push_back(0);

                // face 5
                templateData->faceVertexIndices.push_back(5);
                templateData->faceVertexIndices.push_back(6);
                templateData->faceVertexIndices.push_back(7);
                templateData->faceVertexIndices.push_back(4);
            }

            glm::IdsFilter entityIdsFilter(_params.glmEntityIds.GetText());

            TfToken skelAnimName("SkelAnim");
            TfToken animationsGroupName("Animations");
            GlmString meshVariantEnable("Enable");
            GlmString meshVariantDisable("Disable");
            GlmString lodVariantSetName = "LevelOfDetail";
            GlmString lodVariantName;
            glm::Array<glm::GlmString> entityMeshNames;
            SdfPath animationsGroupPath;
            std::vector<TfToken>* animationsChildNames = NULL;
            _cachedSimulationLocks.resize(crowdFieldNames.size(), nullptr);
            for (size_t iCf = 0, cfCount = crowdFieldNames.size(); iCf < cfCount; ++iCf)
            {
                const glm::GlmString& glmCfName = crowdFieldNames[iCf];
                if (glmCfName.empty())
                {
                    continue;
                }

                TfToken cfName(TfMakeValidIdentifier(glmCfName.c_str()));
                SdfPath cfPath = _GetRootPrimPath().AppendChild(cfName);

                _primSpecPaths.insert(cfPath);
                rootChildNames.push_back(cfName);
                std::vector<TfToken>& cfChildNames = _primChildNames[cfPath];

                if (displayMode == GolaemDisplayMode::SKELETON)
                {
                    animationsGroupPath = cfPath.AppendChild(animationsGroupName);
                    _primSpecPaths.insert(animationsGroupPath);
                    cfChildNames.push_back(animationsGroupName);
                    animationsChildNames = &_primChildNames[animationsGroupPath];
                }

                glm::crowdio::CachedSimulation& cachedSimulation = _factory->getCachedSimulation(cacheDir.c_str(), cacheName.c_str(), glmCfName.c_str());
                const crowdio::glmHistoryRuntimeStructure* historyRuntime = nullptr;
                if (enableLayout && _factory->getLayoutHistoryCount() > 0)
                {
                    historyRuntime = cachedSimulation.getHistoryRuntimeStructure(_factory->getLayoutHistoryCount() - 1);
                }

                GlmSet<int64_t> emptySet;
                const GlmSet<int64_t>* entitiesAffectedByPermanentKill = &emptySet;

                if (historyRuntime != nullptr)
                {
                    entitiesAffectedByPermanentKill = &historyRuntime->_entitiesAffectedByPermanentKill;
                }

                const std::pair<int, int>& frameRange = frameRangesPerCrowdField[iCf];

                _startFrame = min(_startFrame, frameRange.first);
                _endFrame = max(_endFrame, frameRange.second);

                const glm::crowdio::GlmSimulationData* simuData = cachedSimulation.getFinalSimulationData();
                if (simuData == NULL)
                {
                    continue;
                }

                if (_fps < 0)
                {
                    _fps = simuData->_framerate;
                }

                if (glm::approxDiff(_fps, simuData->_framerate, GLM_NUMERICAL_PRECISION))
                {
                    GLM_CROWD_TRACE_WARNING("Found inconsistent frame rates between '" << crowdFieldNames[0] << "' and '" << glmCfName << "'. This might lead to inconsistent renders.");
                }

                // compute assets if needed
                const glm::Array<glm::PODArray<int>>& entityAssets = cachedSimulation.getFinalEntityAssets(frameRange.first);

                // create lock for cached simulation
                glm::Mutex* cachedSimulationLock = new glm::Mutex();
                _cachedSimulationLocks[iCf] = cachedSimulationLock;

                size_t maxEntities = (size_t)floorf(simuData->_entityCount * renderPercent);
                for (uint32_t iEntity = 0; iEntity < simuData->_entityCount; ++iEntity)
                {
                    int64_t entityId = simuData->_entityIds[iEntity];
                    if (entityId < 0)
                    {
                        // entity was probably killed
                        continue;
                    }

                    if (!entityIdsFilter(entityId))
                    {
                        // entity is filtered out
                        continue;
                    }

                    const glm::crowdio::GlmFrameData* firstFrameData = cachedSimulation.getFinalFrameData(frameRange.first, UINT32_MAX, true);

                    int32_t entityToBakeIndex = simuData->_entityToBakeIndex[iEntity];
                    GLM_DEBUG_ASSERT(entityToBakeIndex >= 0);

                    // filter permanently killed entities
                    if (firstFrameData->_entityEnabled[entityToBakeIndex] == 0 &&
                        entitiesAffectedByPermanentKill->find(entityId) != entitiesAffectedByPermanentKill->end())
                    {
                        continue;
                    }

                    glm::GlmString entityName = "Entity_" + glm::toString(entityId);
                    TfToken entityNameToken = TfToken(entityName.c_str());
                    SdfPath entityPath = cfPath.AppendChild(entityNameToken);
                    _primSpecPaths.insert(entityPath);
                    cfChildNames.push_back(entityNameToken);

                    EntityData::SP entityData = NULL;
                    SkelEntityData::SP skelEntityData = NULL;
                    SkinMeshEntityData::SP skinMeshEntityData = NULL;
                    if (displayMode == GolaemDisplayMode::SKELETON)
                    {
                        skelEntityData = new SkelEntityData();
                        entityData = skelEntityData;
                    }
                    else
                    {
                        skinMeshEntityData = new SkinMeshEntityData();
                        entityData = skinMeshEntityData;

                        entityData->inputGeoData._fbxStorage = &getFbxStorage();
                        entityData->inputGeoData._fbxBaker = &getFbxBaker();
                        entityData->inputGeoData._enableLOD = _params.glmLodMode != 0 ? 1 : 0;
                    }
                    _entityDataMap[entityPath] = entityData;
                    entityData->cfIdx = iCf;
                    entityData->initEntityLock();
                    entityData->inputGeoData._dirMapRules = dirmapRules;
                    entityData->inputGeoData._entityId = entityId;
                    entityData->inputGeoData._geometryTag = _params.glmGeometryTag;
                    entityData->inputGeoData._entityIndex = iEntity;
                    entityData->inputGeoData._simuData = simuData;
                    entityData->inputGeoData._entityToBakeIndex = entityToBakeIndex;

                    entityData->inputGeoData._frames.resize(1);
                    entityData->inputGeoData._frames[0] = frameRange.first;
                    entityData->inputGeoData._frameDatas.resize(1);
                    entityData->inputGeoData._frameDatas[0] = firstFrameData;

                    entityData->cachedSimulationLock = cachedSimulationLock;

                    entityData->cachedSimulation = &cachedSimulation;

                    entityData->excluded = iEntity >= maxEntities;
                    entityData->entityPath = entityPath;

                    if (entityData->excluded)
                    {
                        continue;
                    }

                    int32_t characterIdx = simuData->_characterIdx[iEntity];
                    const glm::GolaemCharacter* character = _factory->getGolaemCharacter(characterIdx);
                    if (character == NULL)
                    {
                        GLM_CROWD_TRACE_ERROR_LIMIT("The entity '" << entityId << "' has an invalid character index: '" << characterIdx << "'. Skipping it. Please assign a Rendering Type from the Rendering Attributes panel");
                        entityData->excluded = true;
                        continue;
                    }

                    // add pp attributes
                    size_t ppAttrIdx = 0;
                    for (uint8_t iFloatPPAttr = 0; iFloatPPAttr < simuData->_ppFloatAttributeCount; ++iFloatPPAttr, ++ppAttrIdx)
                    {
                        GlmString attrName = TfMakeValidIdentifier(simuData->_ppFloatAttributeNames[iFloatPPAttr]);
                        if (!attributeNamespace.empty())
                        {
                            attrName = attributeNamespace + ":" + attrName;
                        }
                        TfToken attrNameToken(attrName.c_str());
                        entityData->ppAttrIndexes[attrNameToken] = ppAttrIdx;
                    }
                    for (uint8_t iVectPPAttr = 0; iVectPPAttr < simuData->_ppVectorAttributeCount; ++iVectPPAttr, ++ppAttrIdx)
                    {
                        GlmString attrName = TfMakeValidIdentifier(simuData->_ppVectorAttributeNames[iVectPPAttr]);
                        if (!attributeNamespace.empty())
                        {
                            attrName = attributeNamespace + ":" + attrName;
                        }
                        TfToken attrNameToken(attrName.c_str());
                        entityData->ppAttrIndexes[attrNameToken] = ppAttrIdx;
                    }

                    // add shader attributes
                    glm::GlmString attrName, subAttrName;
                    glm::crowdio::RendererAttributeType::Value overrideType(glm::crowdio::RendererAttributeType::END);
                    for (size_t iShAttr = 0, shAttrCount = character->_shaderAttributes.size(); iShAttr < shAttrCount; ++iShAttr)
                    {
                        const glm::ShaderAttribute& shAttr = character->_shaderAttributes[iShAttr];
                        attrName = shAttr._name.c_str();
                        if (glm::crowdio::parseRendererAttribute("arnold", shAttr._name, attrName, subAttrName, overrideType))
                        {
                            attrName = "arnold:" + PXR_NS::TfMakeValidIdentifier(attrName.c_str());
                        }
                        else
                        {
                            attrName = PXR_NS::TfMakeValidIdentifier(attrName.c_str());
                        }
                        if (!attributeNamespace.empty())
                        {
                            attrName = attributeNamespace + ":" + attrName;
                        }
                        TfToken attrNameToken(attrName.c_str());
                        entityData->shaderAttrIndexes[attrNameToken] = iShAttr;
                    }

                    entityData->inputGeoData._character = character;
                    entityData->inputGeoData._characterIdx = characterIdx;

                    entityData->inputGeoData._assets = &entityAssets[entityData->inputGeoData._entityIndex];

                    uint16_t entityType = simuData->_entityTypes[entityData->inputGeoData._entityIndex];

                    uint16_t boneCount = simuData->_boneCount[entityType];
                    entityData->bonePositionOffset = simuData->_iBoneOffsetPerEntityType[entityType] + simuData->_indexInEntityType[entityData->inputGeoData._entityIndex] * boneCount;

                    if (displayMode == GolaemDisplayMode::SKELETON)
                    {
                        if (characterIdx < usdCharacterFilesList.sizeInt())
                        {
                            const glm::GlmString& usdCharacterFile = usdCharacterFilesList[characterIdx];
                            skelEntityData->referencedUsdCharacter.SetAppendedItems({SdfReference(usdCharacterFile.c_str())});
                        }

                        SdfPath animationSourcePath = animationsGroupPath.AppendChild(entityNameToken);
                        skelEntityData->animationSourcePath = SdfPathListOp::CreateExplicit({animationSourcePath});
                        _primSpecPaths.insert(animationSourcePath);
                        animationsChildNames->push_back(entityNameToken);

                        SdfPath skeletonPath = entityPath.AppendChild(TfToken("Rig")).AppendChild(TfToken("Skel"));
                        skelEntityData->skeletonPath = SdfPathListOp::CreateExplicit({skeletonPath});

                        {
                            // compute mesh names
                            glm::PODArray<int> furAssetIds;
                            glm::PODArray<int> dummyDeepAssets;
                            glm::PODArray<size_t> meshAssetNameIndices;
                            glm::PODArray<int> meshAssetMaterialIndices;
                            glm::Array<glm::GlmString> meshAliases;
                            glm::crowdio::computeMeshNames(
                                skelEntityData->inputGeoData._character,
                                skelEntityData->inputGeoData._entityId,
                                *skelEntityData->inputGeoData._assets,
                                dummyDeepAssets,
                                entityMeshNames,
                                meshAliases,
                                furAssetIds,
                                meshAssetNameIndices,
                                meshAssetMaterialIndices);
                        }

                        // fill skel animation data

                        _skelAnimDataMap[animationSourcePath] = skelEntityData;

                        PODArray<int>& characterSnsIndices = _snsIndicesPerChar[characterIdx];
                        skelEntityData->scalesAnimated = characterSnsIndices.size() > 0 && simuData->_snsCountPerEntityType[entityType] == characterSnsIndices.size();
                        if (skelEntityData->scalesAnimated)
                        {
                            skelEntityData->boneSnsOffset = simuData->_snsOffsetPerEntityType[entityType] + simuData->_indexInEntityType[entityData->inputGeoData._entityIndex] * simuData->_snsCountPerEntityType[entityType];
                        }

                        for (size_t iMesh = 0, meshCount = character->_meshAssets.size(); iMesh < meshCount; ++iMesh)
                        {
                            std::string meshName = TfMakeValidIdentifier(character->_meshAssets[iMesh]._name.c_str());
                            skelEntityData->geoVariants[meshName] = meshVariantDisable.c_str();
                        }
                        for (size_t iMesh = 0, meshCount = entityMeshNames.size(); iMesh < meshCount; ++iMesh)
                        {
                            std::string meshName = TfMakeValidIdentifier(entityMeshNames[iMesh].c_str());
                            skelEntityData->geoVariants[meshName] = meshVariantEnable.c_str();
                        }
                    }
                    else if (displayMode == GolaemDisplayMode::BOUNDING_BOX)
                    {
                        _ComputeBboxData(skinMeshEntityData);
                    }
                    else if (displayMode == GolaemDisplayMode::SKINMESH)
                    {
                        auto& characterTemplateData = _skinMeshTemplateDataPerCharPerGeomFile[entityData->inputGeoData._characterIdx];

                        glm::PODArray<int> gchaMeshIds;
                        glm::PODArray<int> meshAssetMaterialIndices;
                        {
                            // compute mesh names
                            glm::PODArray<int> furAssetIds;
                            glm::PODArray<int> dummyDeepAssets;
                            glm::PODArray<size_t> meshAssetNameIndices;
                            glm::Array<glm::GlmString> meshAliases;
                            glm::crowdio::computeMeshNames(
                                entityData->inputGeoData._character,
                                entityData->inputGeoData._entityId,
                                *entityData->inputGeoData._assets,
                                dummyDeepAssets,
                                entityMeshNames,
                                meshAliases,
                                furAssetIds,
                                meshAssetNameIndices,
                                meshAssetMaterialIndices,
                                &gchaMeshIds);
                        }

                        if (_params.glmLodMode == 0)
                        {
                            // no lod path
                            int geoDataIndex = entityData->inputGeoData._simuData->_iGeoBehaviorOffsetPerEntityType[entityType] + entityData->inputGeoData._simuData->_indexInEntityType[entityData->inputGeoData._entityIndex];
                            int geometryFileIdx = 0;
                            if (entityData->inputGeoData._frameDatas[0] != NULL)
                            {
                                uint16_t cacheGeoIdx = entityData->inputGeoData._frameDatas[0]->_geoBehaviorGeometryIds[geoDataIndex];
                                if (cacheGeoIdx != UINT16_MAX)
                                {
                                    geometryFileIdx = cacheGeoIdx;
                                }
                            }
                            const auto& lodTemplateData = characterTemplateData[geometryFileIdx];

                            skinMeshEntityData->lodEnabled.resize(1, 1);

                            _InitSkinMeshData(entityData->entityPath, skinMeshEntityData, 0, lodTemplateData, gchaMeshIds, meshAssetMaterialIndices);
                        }
                        else
                        {
                            skinMeshEntityData->lodEnabled.resize(characterTemplateData.size(), 0);
                            for (size_t iLod = 0, lodCount = characterTemplateData.size(); iLod < lodCount; ++iLod)
                            {
                                lodVariantName = "lod";
                                lodVariantName += glm::toString(iLod);
                                TfToken lodToken(lodVariantName.c_str());
                                SdfPath lodPath = entityData->entityPath.AppendChild(lodToken);
                                _primSpecPaths.insert(lodPath);
                                _primChildNames[entityData->entityPath].push_back(lodToken);
                                SkinMeshLodMapData& lodMapData = _skinMeshLodDataMap[lodPath];
                                lodMapData.entityData = skinMeshEntityData;
                                lodMapData.lodIndex = iLod;

                                const auto& lodTemplateData = characterTemplateData[iLod];
                                _InitSkinMeshData(lodPath, skinMeshEntityData, iLod, lodTemplateData, gchaMeshIds, meshAssetMaterialIndices);
                            }
                        }
                    }

                    float entityScale = simuData->_scales[entityData->inputGeoData._entityIndex];

                    entityData->defaultGeometryFileIdx = 0;

                    PODArray<float> overrideMinLodDistances;
                    PODArray<float> overrideMaxLodDistances;
                    float distanceToCamera = -1.f;

                    uint32_t geoDataIndex = entityData->inputGeoData._simuData->_iGeoBehaviorOffsetPerEntityType[entityType] + entityData->inputGeoData._simuData->_indexInEntityType[entityData->inputGeoData._entityIndex];
                    bool geoFileIdxSet = false;
                    if (entityData->inputGeoData._frameDatas[0] != NULL)
                    {
                        uint16_t cacheGeoIdx = entityData->inputGeoData._frameDatas[0]->_geoBehaviorGeometryIds[geoDataIndex];
                        if (cacheGeoIdx != UINT16_MAX)
                        {
                            entityData->defaultGeometryFileIdx = cacheGeoIdx;
                            geoFileIdxSet = true;
                        }
                    }
                    if (!geoFileIdxSet)
                    {
                        if (_params.glmLodMode > 0)
                        {
                            float* rootPos = entityData->inputGeoData._frameDatas[0]->_bonePositions[entityData->bonePositionOffset];
                            Vector3 entityPos(rootPos);
                            Vector3 cameraPos;

                            // update LOD data
                            if (_params.glmLodMode == 1)
                            {
                                // in static lod mode get the camera pos directly from the params
                                cameraPos.setValues(_params.glmCameraPos.data());
                            }
                            else if (_params.glmLodMode == 2)
                            {
                                // in dynamic lod mode get the camera pos from the node attributes (it may be connected to another attribute - usdWrapper will do the update)
                                const VtValue* cameraPosValue = TfMapLookupPtr(_usdParams, _golaemTokens->glmCameraPos);
                                if (cameraPosValue != NULL)
                                {
                                    if (cameraPosValue->IsHolding<GfVec3f>())
                                    {
                                        const GfVec3f& usdValue = cameraPosValue->UncheckedGet<GfVec3f>();
                                        cameraPos.setValues(usdValue.data());
                                    }
                                }
                            }

                            distanceToCamera = crowdio::computeDistanceToCamera(cameraPos, entityPos, *character, entityScale, entityData->inputGeoData._geometryTag);
                            crowdio::getLodOverridesFromCache(overrideMinLodDistances, overrideMaxLodDistances, &entityData->inputGeoData);
                        }
                    }

                    const GeometryAsset* geometryAsset = character->getGeometryAsset(entityData->inputGeoData._geometryTag, entityData->defaultGeometryFileIdx, distanceToCamera, &overrideMinLodDistances, &overrideMaxLodDistances);
                    if (geometryAsset)
                    {
                        GlmString lodLevelString;
                        getStringFromLODLevel(static_cast<LODLevelFlags::Value>(geometryAsset->_lodLevel), lodLevelString);
                        entityData->defaultLodName = TfToken(lodLevelString.c_str());
                    }

                    if (displayMode == GolaemDisplayMode::SKINMESH)
                    {
                        skinMeshEntityData->lodEnabled[entityData->defaultGeometryFileIdx] = 1;
                        if (_params.glmLodMode == 1)
                        {
                            entityData->inputGeoData._enableLOD = 0; // disable LOD switching at frame time
                            entityData->inputGeoData._geoFileIndex = static_cast<int32_t>(entityData->defaultGeometryFileIdx);
                        }
                    }
                    else if (displayMode == GolaemDisplayMode::SKELETON)
                    {
                        // set the lod variant
                        lodVariantName = "lod";
                        lodVariantName += glm::toString(entityData->defaultGeometryFileIdx);
                        skelEntityData->geoVariants[lodVariantSetName.c_str()] = lodVariantName.c_str();
                    }
                    _getCharacterExtent(entityData, entityData->extent);
                }
            }

            if (_startFrame <= _endFrame)
            {
                for (int currentFrame = _startFrame; currentFrame <= _endFrame; ++currentFrame)
                {
                    _animTimeSampleTimes.insert(currentFrame);
                }
            }
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_InitSkinMeshData(
            const SdfPath& parentPath,
            SkinMeshEntityData::SP entityData,
            size_t lodIndex,
            const std::map<std::pair<int, int>, SkinMeshTemplateData::SP>& templateDataPerMesh,
            const glm::PODArray<int>& gchaMeshIds,
            const glm::PODArray<int>& meshAssetMaterialIndices)
        {
            for (size_t iMesh = 0, meshCount = gchaMeshIds.size(); iMesh < meshCount; ++iMesh)
            {
                const auto& itMesh = templateDataPerMesh.find({gchaMeshIds[iMesh], meshAssetMaterialIndices[iMesh]});
                if (itMesh == templateDataPerMesh.end())
                {
                    continue;
                }
                SkinMeshTemplateData::SP meshTemplateData = itMesh->second;
                entityData->computeVelocities = entityData->computeVelocities || meshTemplateData->velocitiesIntShaderAttributeIndex >= 0;

                GlmMap<GlmString, SdfPath> meshTreePaths;
                SdfPath lastMeshTransformPath = _CreateHierarchyFor(meshTemplateData->meshAlias, parentPath, meshTreePaths);

                SkinMeshMapData& meshMapData = _skinMeshDataMap[lastMeshTransformPath];
                meshMapData.lodIndex = lodIndex;
                meshMapData.entityData = entityData;
                meshMapData.templateData = meshTemplateData;
                meshMapData.gchaMeshId = gchaMeshIds[iMesh];
                meshMapData.meshMaterialIndex = meshAssetMaterialIndices[iMesh];
            }
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::_IsAnimatedProperty(const SdfPath& path) const
        {
            // Check that it is a property id.
            if (!path.IsPrimPropertyPath())
            {
                return false;
            }
            const TfToken& nameToken = path.GetNameToken();
            SdfPath primPath = path.GetAbsoluteRootOrPrimPath();
            if (primPath == _GetRootPrimPath())
            {
                return false;
            }

            if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
            {
                // Check that it's one of our animated property names.
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelEntityProperties, nameToken))
                {
                    if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                    {
                        return propInfo->isAnimated;
                    }
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelAnimProperties, nameToken))
                {
                    if (const SkelEntityData::SP* skelEntityDataPtr = TfMapLookupPtr(_skelAnimDataMap, primPath))
                    {
                        if (propInfo->isAnimated)
                        {
                            if (nameToken == _skelAnimPropertyTokens->scales)
                            {
                                // scales are not always animated
                                return (*skelEntityDataPtr)->scalesAnimated;
                            }
                            return true;
                        }
                    }
                }
                if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                {
                    if (TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken) != NULL ||
                        TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken) != NULL)
                    {
                        return true;
                    }
                }
            }
            else
            {
                // Check that it's one of our animated property names.
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshEntityProperties, nameToken))
                {
                    if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                    {
                        return propInfo->isAnimated;
                    }
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshLodProperties, nameToken))
                {
                    if (TfMapLookupPtr(_skinMeshLodDataMap, primPath) != NULL)
                    {
                        return propInfo->isAnimated;
                    }
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshProperties, nameToken))
                {
                    if (TfMapLookupPtr(_skinMeshDataMap, primPath) != NULL)
                    {
                        return propInfo->isAnimated;
                    }
                }
                if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                {
                    if (TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken) != NULL ||
                        TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken) != NULL)
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::_HasPropertyDefaultValue(const SdfPath& path, VtValue* value) const
        {
            // Check that it is a property id.
            if (!path.IsPrimPropertyPath())
            {
                return false;
            }

            // Check that it is one of our property names.
            const TfToken& nameToken = path.GetNameToken();
            SdfPath primPath = path.GetAbsoluteRootOrPrimPath();

            if (primPath == _GetRootPrimPath())
            {
                if (const VtValue* usdValue = TfMapLookupPtr(_usdParams, nameToken))
                {
                    if (value)
                    {
                        *value = *usdValue;
                    }
                    return true;
                }
            }

            // Check that it belongs to a leaf prim before getting the default value
            if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
            {
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelEntityProperties, nameToken))
                {
                    if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                    {
                        if (value)
                        {
                            if (nameToken == _skelEntityPropertyTokens->entityId)
                            {
                                *value = VtValue((*entityDataPtr)->inputGeoData._entityId);
                            }
                            else if (nameToken == _skelEntityPropertyTokens->extent)
                            {
                                *value = VtValue(VtVec3fArray({-(*entityDataPtr)->extent, (*entityDataPtr)->extent}));
                            }
                            else if (nameToken == _skelEntityPropertyTokens->geometryTagId)
                            {
                                *value = VtValue(int32_t((*entityDataPtr)->inputGeoData._geometryTag));
                            }
                            else if (nameToken == _skelEntityPropertyTokens->geometryFileId)
                            {
                                *value = VtValue(int32_t((*entityDataPtr)->defaultGeometryFileIdx));
                            }
                            else if (nameToken == _skelEntityPropertyTokens->lodName)
                            {
                                *value = VtValue((*entityDataPtr)->defaultLodName);
                            }
                            else
                            {
                                *value = propInfo->defaultValue;
                            }
                        }
                        return true;
                    }
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelAnimProperties, nameToken))
                {
                    if (const SkelEntityData::SP* skelEntityDataPtr = TfMapLookupPtr(_skelAnimDataMap, primPath))
                    {
                        if (value)
                        {
                            if (nameToken == _skelAnimPropertyTokens->joints)
                            {
                                *value = VtValue(_jointsPerChar[(*skelEntityDataPtr)->inputGeoData._characterIdx]);
                            }
                            else
                            {
                                *value = propInfo->defaultValue;
                            }
                        }
                        return true;
                    }
                }
                if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                {
                    if (const size_t* ppAttrIdx = TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken))
                    {
                        if (value)
                        {
                            if (*ppAttrIdx < static_cast<size_t>((*entityDataPtr)->inputGeoData._simuData->_ppFloatAttributeCount))
                            {
                                // this is a float PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_FLOAT - 1; // enum starts at 1
                                *value = _ppAttrDefaultValues[attrTypeIdx];
                            }
                            else
                            {
                                // this is a vector PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_VECTOR - 1; // enum starts at 1
                                *value = _ppAttrDefaultValues[attrTypeIdx];
                            }
                        }
                        return true;
                    }
                    if (const size_t* shaderAttrIdx = TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken))
                    {
                        if (value)
                        {
                            const glm::ShaderAttribute& shaderAttr = (*entityDataPtr)->inputGeoData._character->_shaderAttributes[*shaderAttrIdx];
                            *value = _shaderAttrDefaultValues[shaderAttr._type];
                        }
                        return true;
                    }
                }
            }
            else
            {
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshEntityProperties, nameToken))
                {
                    if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                    {
                        if (value)
                        {
                            // Special case for translate property. Each leaf prim has its own
                            // default position.
                            if (nameToken == _skinMeshEntityPropertyTokens->entityId)
                            {
                                *value = VtValue((*entityDataPtr)->inputGeoData._entityId);
                            }
                            else if (nameToken == _skinMeshEntityPropertyTokens->geometryTagId)
                            {
                                *value = VtValue(int32_t((*entityDataPtr)->inputGeoData._geometryTag));
                            }
                            else if (nameToken == _skinMeshEntityPropertyTokens->extentsHint)
                            {
                                *value = VtValue(VtVec3fArray({-(*entityDataPtr)->extent, (*entityDataPtr)->extent}));
                            }
                            else
                            {
                                *value = propInfo->defaultValue;
                            }
                        }
                        return true;
                    }

                    return false;
                }
                if (TfMapLookupPtr(*_skinMeshLodProperties, nameToken))
                {
                    if (const SkinMeshLodMapData* lodMapData = TfMapLookupPtr(_skinMeshLodDataMap, primPath))
                    {
                        if (value)
                        {
                            if (nameToken == _skinMeshLodPropertyTokens->visibility)
                            {
                                *value = VtValue(_params.glmLodMode == 1 || lodMapData->entityData->lodEnabled[lodMapData->lodIndex] > 0 ? UsdGeomTokens->inherited : UsdGeomTokens->invisible);
                            }
                        }
                        return true;
                    }

                    return false;
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshProperties, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the default value
                    if (const SkinMeshMapData* meshMapData = TfMapLookupPtr(_skinMeshDataMap, primPath))
                    {
                        if (value)
                        {
                            if (nameToken == _skinMeshPropertyTokens->points)
                            {
                                *value = VtValue(meshMapData->templateData->defaultPoints);
                            }
                            else if (nameToken == _skinMeshPropertyTokens->normals)
                            {
                                *value = VtValue(meshMapData->templateData->defaultNormals);
                            }
                            else if (nameToken == _skinMeshPropertyTokens->faceVertexCounts)
                            {
                                *value = VtValue(meshMapData->templateData->faceVertexCounts);
                            }
                            else if (nameToken == _skinMeshPropertyTokens->faceVertexIndices)
                            {
                                *value = VtValue(meshMapData->templateData->faceVertexIndices);
                            }
                            else if (nameToken == _skinMeshPropertyTokens->uvs)
                            {
                                if (meshMapData->templateData->uvSets.empty())
                                {
                                    return false;
                                }
                                *value = VtValue(meshMapData->templateData->uvSets.front());
                            }
                            else if (nameToken == _skinMeshPropertyTokens->velocities)
                            {
                                if (!meshMapData->entityData->computeVelocities || meshMapData->templateData->defaultVelocities.empty())
                                {
                                    return false;
                                }
                                *value = VtValue(meshMapData->templateData->defaultVelocities);
                            }
                            else
                            {
                                *value = propInfo->defaultValue;
                            }
                        }
                        return true;
                    }
                }
                if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                {
                    if (const size_t* ppAttrIdx = TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken))
                    {
                        if (value)
                        {
                            if (*ppAttrIdx < static_cast<size_t>((*entityDataPtr)->inputGeoData._simuData->_ppFloatAttributeCount))
                            {
                                // this is a float PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_FLOAT - 1; // enum starts at 1
                                *value = _ppAttrDefaultValues[attrTypeIdx];
                            }
                            else
                            {
                                // this is a vector PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_VECTOR - 1; // enum starts at 1
                                *value = _ppAttrDefaultValues[attrTypeIdx];
                            }
                        }
                        return true;
                    }
                    if (const size_t* shaderAttrIdx = TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken))
                    {
                        if (value)
                        {
                            const glm::ShaderAttribute& shaderAttr = (*entityDataPtr)->inputGeoData._character->_shaderAttributes[*shaderAttrIdx];
                            *value = _shaderAttrDefaultValues[shaderAttr._type];
                        }
                        return true;
                    }
                }
            }
            return false;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::_HasTargetPathValue(const SdfPath& path, VtValue* value) const
        {
            // Check that it is a relationship id.
            if (!path.IsPropertyPath())
            {
                return false;
            }

            // Check that it is one of our property names.
            const TfToken& nameToken = path.GetNameToken();
            SdfPath primPath = path.GetAbsoluteRootOrPrimPath();

            if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
            {
                if (const _PrimRelationshipInfo* relInfo = TfMapLookupPtr(*_skelEntityRelationships, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the default value
                    if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                    {
                        if (value)
                        {
                            SkelEntityData* skelEntityData = static_cast<SkelEntityData*>(entityDataPtr->getImpl());
                            if (nameToken == _skelEntityRelationshipTokens->animationSource)
                            {
                                *value = VtValue(skelEntityData->animationSourcePath);
                            }
                            else if (nameToken == _skelEntityRelationshipTokens->skeleton)
                            {
                                *value = VtValue(skelEntityData->skeletonPath);
                            }
                            else
                            {
                                *value = VtValue(relInfo->defaultTargetPath);
                            }
                        }
                        return true;
                    }
                }
            }
            else
            {
                if (const _PrimRelationshipInfo* relInfo = TfMapLookupPtr(*_skinMeshRelationships, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the default value
                    if (const SkinMeshMapData* meshMapData = TfMapLookupPtr(_skinMeshDataMap, primPath))
                    {
                        if (value)
                        {
                            if (nameToken == _skinMeshRelationshipTokens->materialBinding)
                            {
                                *value = VtValue(meshMapData->templateData->materialPath);
                            }
                            else
                            {
                                *value = VtValue(relInfo->defaultTargetPath);
                            }
                        }
                        return true;
                    }
                    return false;
                }
            }

            return false;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::_HasPropertyInterpolation(const SdfPath& path, VtValue* value) const
        {
            // Check that it is a property id.
            if (!path.IsPrimPropertyPath())
            {
                return false;
            }

            // Check that it is one of our property names.
            const TfToken& nameToken = path.GetNameToken();
            SdfPath primPath = path.GetAbsoluteRootOrPrimPath();
            if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
            {
                return false;
            }
            else
            {
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshProperties, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the interpolation value
                    if (TfMapLookupPtr(_skinMeshDataMap, primPath) != NULL)
                    {
                        if (value)
                        {
                            if (propInfo->hasInterpolation)
                            {
                                *value = VtValue(propInfo->interpolation);
                            }
                        }
                        return propInfo->hasInterpolation;
                    }
                    return false;
                }
            }

            return false;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_DataImpl::_HasPropertyTypeNameValue(const SdfPath& path, VtValue* value) const
        {
            // Check that it is a property id.
            if (!path.IsPrimPropertyPath())
            {
                return false;
            }

            // Check that it is one of our property names.
            const TfToken& nameToken = path.GetNameToken();
            SdfPath primPath = path.GetAbsoluteRootOrPrimPath();

            if (primPath == _GetRootPrimPath())
            {
                if (const VtValue* usdValue = TfMapLookupPtr(_usdParams, nameToken))
                {
                    RETURN_TRUE_WITH_OPTIONAL_VALUE(SdfSchema::GetInstance().FindType(*usdValue).GetAsToken());
                }
            }

            if (_params.glmDisplayMode == GolaemDisplayMode::SKELETON)
            {
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelEntityProperties, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the type name value
                    if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(propInfo->typeName);
                    }

                    return false;
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skelAnimProperties, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the type name value
                    if (TfMapLookupPtr(_skelAnimDataMap, primPath) != NULL)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(propInfo->typeName);
                    }

                    return false;
                }
                if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                {
                    if (const size_t* ppAttrIdx = TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken))
                    {
                        if (value)
                        {
                            if (*ppAttrIdx < static_cast<size_t>((*entityDataPtr)->inputGeoData._simuData->_ppFloatAttributeCount))
                            {
                                // this is a float PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_FLOAT - 1; // enum starts at 1
                                *value = TfToken(_ppAttrTypes[attrTypeIdx].c_str());
                            }
                            else
                            {
                                // this is a vector PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_VECTOR - 1; // enum starts at 1
                                *value = TfToken(_ppAttrTypes[attrTypeIdx].c_str());
                            }
                        }
                        return true;
                    }
                    if (const size_t* shaderAttrIdx = TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken))
                    {
                        const glm::ShaderAttribute& shaderAttr = (*entityDataPtr)->inputGeoData._character->_shaderAttributes[*shaderAttrIdx];
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken(_shaderAttrTypes[shaderAttr._type].c_str()));
                    }
                }
            }
            else
            {
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshEntityProperties, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the type name value
                    if (TfMapLookupPtr(_entityDataMap, primPath) != NULL)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(propInfo->typeName);
                    }

                    return false;
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshLodProperties, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the type name value
                    if (TfMapLookupPtr(_skinMeshLodDataMap, primPath) != NULL)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(propInfo->typeName);
                    }

                    return false;
                }
                if (const _PrimPropertyInfo* propInfo = TfMapLookupPtr(*_skinMeshProperties, nameToken))
                {
                    // Check that it belongs to a leaf prim before getting the type name value
                    if (TfMapLookupPtr(_skinMeshDataMap, primPath) != NULL)
                    {
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(propInfo->typeName);
                    }

                    return false;
                }
                if (const EntityData::SP* entityDataPtr = TfMapLookupPtr(_entityDataMap, primPath))
                {
                    if (const size_t* ppAttrIdx = TfMapLookupPtr((*entityDataPtr)->ppAttrIndexes, nameToken))
                    {
                        if (value)
                        {
                            if (*ppAttrIdx < static_cast<size_t>((*entityDataPtr)->inputGeoData._simuData->_ppFloatAttributeCount))
                            {
                                // this is a float PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_FLOAT - 1; // enum starts at 1
                                *value = TfToken(_ppAttrTypes[attrTypeIdx].c_str());
                            }
                            else
                            {
                                // this is a vector PP attribute
                                int attrTypeIdx = crowdio::GSC_PP_VECTOR - 1; // enum starts at 1
                                *value = TfToken(_ppAttrTypes[attrTypeIdx].c_str());
                            }
                        }
                        return true;
                    }
                    if (const size_t* shaderAttrIdx = TfMapLookupPtr((*entityDataPtr)->shaderAttrIndexes, nameToken))
                    {
                        const glm::ShaderAttribute& shaderAttr = (*entityDataPtr)->inputGeoData._character->_shaderAttributes[*shaderAttrIdx];
                        RETURN_TRUE_WITH_OPTIONAL_VALUE(TfToken(_shaderAttrTypes[shaderAttr._type].c_str()));
                    }
                }
            }

            return false;
        }

        //-----------------------------------------------------------------------------
        SdfPath GolaemUSD_DataImpl::_CreateHierarchyFor(const glm::GlmString& hierarchy, const SdfPath& parentPath, GlmMap<GlmString, SdfPath>& existingPaths)
        {
            if (hierarchy.empty())
                return parentPath;

            // split last Group, find its parent and add this asset group xform
            size_t firstSlash = hierarchy.find_first_of('|');
            GlmString thisGroup = hierarchy.substr(0, firstSlash);
            GlmString childrenGroupsHierarchy("");
            if (firstSlash != GlmString::npos)
                childrenGroupsHierarchy = hierarchy.substr(firstSlash + 1);

            // create this group path
            SdfPath thisGroupPath = parentPath;
            if (!thisGroup.empty())
            {
                GlmMap<GlmString, SdfPath>::iterator foundThisGroupPath = existingPaths.find(thisGroup);
                if (foundThisGroupPath == existingPaths.end())
                {
                    // group does not exist, create it
                    TfToken thisGroupToken(TfMakeValidIdentifier(thisGroup.c_str()).c_str());
                    thisGroupPath = parentPath.AppendChild(thisGroupToken);
                    _primSpecPaths.insert(thisGroupPath);
                    _primChildNames[parentPath].push_back(thisGroupToken);
                    existingPaths[thisGroup] = thisGroupPath;
                }
                else
                {
                    thisGroupPath = foundThisGroupPath.getValue();
                }
            }
            else
            {
                thisGroupPath = parentPath;
            }

            return _CreateHierarchyFor(childrenGroupsHierarchy, thisGroupPath, existingPaths);
        }

        //-----------------------------------------------------------------------------
        GolaemUSD_DataImpl::SkelEntityFrameData::SP GolaemUSD_DataImpl::_ComputeSkelEntity(EntityData::SP entityData, double frame)
        {
            SkelEntityFrameData::SP skelEntityFrameData = entityData->getFrameData<SkelEntityFrameData>(frame, static_cast<size_t>(_params.glmCachedFramesCount));

            if (skelEntityFrameData->entityData != nullptr)
            {
                // getFrameData returned an existing SkelEntityFrameData
                return skelEntityFrameData;
            }

            // getFrameData returned a new SkelEntityFrameData, set entityData to mark it as computed
            skelEntityFrameData->entityData = entityData;

#ifdef TRACY_ENABLE
            ZoneScopedNC("ComputeSkelEntity", GLM_COLOR_CACHE);
            glm::GlmString entityIdStr = "EntityId=" + glm::toString(entityData->inputGeoData._entityId);
            ZoneText(entityIdStr.c_str(), entityIdStr.size());
            glm::GlmString frameStr = "Frame=" + glm::toString(frame);
            ZoneText(frameStr.c_str(), frameStr.size());
#endif
            _ComputeEntity(skelEntityFrameData, frame);
            if (!skelEntityFrameData->enabled)
            {
                return skelEntityFrameData;
            }

            SkelEntityData* skelEntityData = static_cast<SkelEntityData*>(entityData.getImpl());

            const glm::crowdio::GlmFrameData* frameData = entityData->inputGeoData._frameDatas[0];
            const glm::crowdio::GlmSimulationData* simuData = entityData->inputGeoData._simuData;

            const PODArray<int>& characterSnsIndices = _snsIndicesPerChar[entityData->inputGeoData._characterIdx];

            float entityScale = simuData->_scales[entityData->inputGeoData._entityIndex];
            uint16_t entityType = simuData->_entityTypes[entityData->inputGeoData._entityIndex];

            uint16_t boneCount = simuData->_boneCount[entityType];

            skelEntityFrameData->scales.assign(boneCount, GfVec3h(1, 1, 1));
            skelEntityFrameData->scales.front().Set(entityScale, entityScale, entityScale); // root bone gets entity scale
            skelEntityFrameData->rotations.resize(boneCount);
            skelEntityFrameData->translations.resize(boneCount);

            const glm::PODArray<size_t>& specificToCacheBoneIndices = entityData->inputGeoData._character->_converterMapping._skeletonDescription->getSpecificToCacheBoneIndices();

            Array<Vector3> specificBonesWorldScales(boneCount, Vector3(1, 1, 1)); // used to fix mesh translations by reverting local scale
            if (skelEntityData->scalesAnimated)
            {
                for (size_t iSnS = 0, snsCount = characterSnsIndices.size(); iSnS < snsCount; ++iSnS)
                {
                    int specificBoneIndex = characterSnsIndices[iSnS];
                    if (specificBoneIndex == 0)
                    {
                        // skip root, always gets entity scale
                        continue;
                    }

                    GfVec3h& scaleValue = skelEntityFrameData->scales[specificBoneIndex];

                    float (&snsCacheValues)[4] = frameData->_snsValues[skelEntityData->boneSnsOffset + iSnS];

                    scaleValue[0] = snsCacheValues[0];
                    scaleValue[1] = snsCacheValues[1];
                    scaleValue[2] = snsCacheValues[2];

                    specificBonesWorldScales[specificBoneIndex].setValues(snsCacheValues);
                }

                // here all scales are WORLD scales. Need to patch back local scales from there :
                for (uint16_t iBone = 0; iBone < boneCount; ++iBone)
                {
                    GfVec3h& scaleValue = skelEntityFrameData->scales[iBone];

                    const HierarchicalBone* currentBone = entityData->inputGeoData._character->_converterMapping._skeletonDescription->getBones()[iBone];
                    const HierarchicalBone* fatherBone = currentBone->getFather();
                    // skip scales parented to root, root holds the entityScale and cannot be SnS'ed
                    if (fatherBone != NULL)
                    {
                        const Vector3& fatherScale = specificBonesWorldScales[fatherBone->getSpecificBoneIndex()];

                        scaleValue[0] /= fatherScale[0];
                        scaleValue[1] /= fatherScale[1];
                        scaleValue[2] /= fatherScale[2];
                    }
                }
            }

            for (size_t iBone = 0; iBone < boneCount; ++iBone)
            {
                size_t boneIndexInCache = specificToCacheBoneIndices[iBone];

                const HierarchicalBone* currentBone = entityData->inputGeoData._character->_converterMapping._skeletonDescription->getBones()[iBone];
                const HierarchicalBone* fatherBone = currentBone->getFather();

                // get translation/rotation values as 3 float

                Vector3 currentPosValues(frameData->_bonePositions[entityData->bonePositionOffset + boneIndexInCache]); // default
                float (&quatValue)[4] = frameData->_boneOrientations[entityData->bonePositionOffset + boneIndexInCache];

                Quaternion boneWOri(quatValue);
                Quaternion fatherBoneWOri(0, 0, 0, 1);
                // in joint reference
                if (fatherBone != NULL)
                {
                    int fatherBoneSpecificIndex = fatherBone->getSpecificBoneIndex();
                    size_t fatherBoneIndexInCache = specificToCacheBoneIndices[fatherBoneSpecificIndex];

                    float (&fatherQuatValue)[4] = frameData->_boneOrientations[entityData->bonePositionOffset + fatherBoneIndexInCache];
                    Vector3 fatherBoneWPos(frameData->_bonePositions[entityData->bonePositionOffset + fatherBoneIndexInCache]);

                    fatherBoneWOri.setValues(fatherQuatValue);

                    // in local coordinates
                    currentPosValues = fatherBoneWOri.computeInverse() * (currentPosValues - fatherBoneWPos);
                    currentPosValues /= entityScale;

                    // also need to take back parent scale value
                    if (skelEntityData->scalesAnimated && fatherBoneSpecificIndex < specificBonesWorldScales.sizeInt())
                    {
                        const Vector3& parentScale = specificBonesWorldScales[fatherBoneSpecificIndex];
                        currentPosValues[0] /= parentScale.x;
                        currentPosValues[1] /= parentScale.y;
                        currentPosValues[2] /= parentScale.z;
                    }
                }

                Quaternion boneLOri = fatherBoneWOri.computeInverse() * boneWOri;

                skelEntityFrameData->translations[iBone] = GfVec3f(currentPosValues.getFloatValues());
                skelEntityFrameData->rotations[iBone] = GfQuatf(boneLOri.w, boneLOri.x, boneLOri.y, boneLOri.z);
            }
            return skelEntityFrameData;
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_ComputeEntity(EntityFrameData::SP entityFrameData, double frame)
        {
            EntityData::SP entityData = entityFrameData->entityData;
            const glm::crowdio::GlmSimulationData* simuData = entityData->inputGeoData._simuData;
            const glm::crowdio::GlmFrameData* frameData = NULL;
            const glm::ShaderAssetDataContainer* shaderDataContainer = NULL;
            {
                glm::ScopedLock<glm::Mutex> cachedSimuLock(*entityData->cachedSimulationLock);
                frameData = entityData->cachedSimulation->getFinalFrameData(frame, UINT32_MAX, true);
                shaderDataContainer = entityData->cachedSimulation->getFinalShaderData(frame, UINT32_MAX, true);
            }
            if (simuData == NULL || frameData == NULL)
            {
                _InvalidateEntity(entityFrameData);
                return;
            }

            entityFrameData->enabled = frameData->_entityEnabled[entityData->inputGeoData._entityToBakeIndex] == 1;
            if (!entityFrameData->enabled)
            {
                _InvalidateEntity(entityFrameData);
                return;
            }

            const glm::PODArray<int>& entityIntShaderData = shaderDataContainer->intData[entityData->inputGeoData._entityIndex];
            const glm::PODArray<float>& entityFloatShaderData = shaderDataContainer->floatData[entityData->inputGeoData._entityIndex];
            const glm::Array<glm::Vector3>& entityVectorShaderData = shaderDataContainer->vectorData[entityData->inputGeoData._entityIndex];
            const glm::Array<glm::GlmString>& entityStringShaderData = shaderDataContainer->stringData[entityData->inputGeoData._entityIndex];

            const PODArray<size_t>& globalToSpecificShaderAttrIdx = _globalToSpecificShaderAttrIdxPerChar[entityData->inputGeoData._characterIdx]; // no bounds check needed, characters are consistent across crowd fields

            const PODArray<size_t>& characterSpecificShaderAttrCounters = shaderDataContainer->specificShaderAttrCountersPerChar[entityData->inputGeoData._characterIdx];

            entityFrameData->intShaderAttrValues.resize(characterSpecificShaderAttrCounters[glm::ShaderAttributeType::INT], 0);
            entityFrameData->floatShaderAttrValues.resize(characterSpecificShaderAttrCounters[glm::ShaderAttributeType::FLOAT], 0);
            entityFrameData->stringShaderAttrValues.resize(characterSpecificShaderAttrCounters[glm::ShaderAttributeType::STRING]);
            entityFrameData->vectorShaderAttrValues.resize(characterSpecificShaderAttrCounters[glm::ShaderAttributeType::VECTOR], GfVec3f(0));

            // compute shader data
            glm::Vector3 vectValue;
            for (size_t iShaderAttr = 0, shaderAttrCount = entityData->inputGeoData._character->_shaderAttributes.size(); iShaderAttr < shaderAttrCount; ++iShaderAttr)
            {
                const glm::ShaderAttribute& shaderAttribute = entityData->inputGeoData._character->_shaderAttributes[iShaderAttr];
                size_t specificAttrIdx = globalToSpecificShaderAttrIdx[iShaderAttr];
                switch (shaderAttribute._type)
                {
                case glm::ShaderAttributeType::INT:
                {
                    entityFrameData->intShaderAttrValues[specificAttrIdx] = entityIntShaderData[specificAttrIdx];
                }
                break;
                case glm::ShaderAttributeType::FLOAT:
                {
                    entityFrameData->floatShaderAttrValues[specificAttrIdx] = entityFloatShaderData[specificAttrIdx];
                }
                break;
                case glm::ShaderAttributeType::STRING:
                {
                    entityFrameData->stringShaderAttrValues[specificAttrIdx] = TfToken(entityStringShaderData[specificAttrIdx].c_str());
                }
                break;
                case glm::ShaderAttributeType::VECTOR:
                {
                    entityFrameData->vectorShaderAttrValues[specificAttrIdx].Set(entityVectorShaderData[specificAttrIdx].getFloatValues());
                }
                break;
                default:
                    break;
                }
            }

            entityFrameData->floatPPAttrValues.resize(simuData->_ppFloatAttributeCount, 0);
            entityFrameData->vectorPPAttrValues.resize(simuData->_ppVectorAttributeCount, GfVec3f(0));

            // update pp attributes
            for (uint8_t iFloatPPAttr = 0; iFloatPPAttr < simuData->_ppFloatAttributeCount; ++iFloatPPAttr)
            {
                entityFrameData->floatPPAttrValues[iFloatPPAttr] = frameData->_ppFloatAttributeData[iFloatPPAttr][entityData->inputGeoData._entityToBakeIndex];
            }
            for (uint8_t iVectPPAttr = 0; iVectPPAttr < simuData->_ppVectorAttributeCount; ++iVectPPAttr)
            {
                entityFrameData->vectorPPAttrValues[iVectPPAttr].Set(frameData->_ppVectorAttributeData[iVectPPAttr][entityData->inputGeoData._entityToBakeIndex]);
            }

            // update frame before computing geometry
            entityData->inputGeoData._frames.resize(1);
            entityData->inputGeoData._frames[0] = frame;
            entityData->inputGeoData._frameDatas.resize(1);
            entityData->inputGeoData._frameDatas[0] = frameData;

            float* rootPos = frameData->_bonePositions[entityData->bonePositionOffset];
            entityFrameData->pos.Set(rootPos);
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_ComputeEntityVelocities(SkinMeshEntityFrameData::SP currentFrameData, SkinMeshEntityFrameData::SP prevFrameData)
        {
            SkinMeshEntityData::SP skinMeshEntityData = glm::staticCast<SkinMeshEntityData>(currentFrameData->entityData);
            if (!skinMeshEntityData->computeVelocities || currentFrameData->velocitiesComputed)
            {
                return;
            }
            if (!prevFrameData)
            {
                return;
            }

            // In dynamic LOD mode, only compute velocities when the same geometry/LOD
            // index is used in both the previous and current frames.
            if (_params.glmLodMode != 0 && currentFrameData->geometryFileIdx != prevFrameData->geometryFileIdx)
            {
                return;
            }
            size_t lodLevel = _params.glmLodMode == 0 ? 0 : currentFrameData->geometryFileIdx;
            if (lodLevel >= currentFrameData->meshLodData.size() || lodLevel >= prevFrameData->meshLodData.size())
            {
                return;
            }
            SkinMeshLodData::SP prevMeshLodData = prevFrameData->meshLodData[lodLevel];
            if (prevMeshLodData == nullptr)
            {
                return;
            }

            for (const auto& meshDataIt : prevMeshLodData->meshData)
            {
                const SkinMeshData::SP& prevMeshData = meshDataIt.second;
                if (prevMeshData == nullptr)
                {
                    continue;
                }
                const VtArray<GfVec3f>& prevPoints = prevMeshData->points;
                if (prevPoints.empty())
                {
                    continue;
                }

                const auto& currentMeshDataIt = currentFrameData->meshLodData[lodLevel]->meshData.find(meshDataIt.first);
                if (currentMeshDataIt == currentFrameData->meshLodData[lodLevel]->meshData.end())
                {
                    continue;
                }
                const SkinMeshData::SP& currentMeshData = currentMeshDataIt->second;
                if (currentMeshData == nullptr)
                {
                    continue;
                }
                const VtArray<GfVec3f>& currentPoints = currentMeshData->points;
                if (currentPoints.empty() || currentPoints.size() != prevPoints.size())
                {
                    continue;
                }

                bool velocitiesEnabled = currentMeshData->templateData->velocitiesIntShaderAttributeIndex >= 0 &&
                                         static_cast<size_t>(currentMeshData->templateData->velocitiesIntShaderAttributeIndex) < currentFrameData->intShaderAttrValues.size() &&
                                         currentFrameData->intShaderAttrValues[currentMeshData->templateData->velocitiesIntShaderAttributeIndex] == 1;
                if (velocitiesEnabled)
                {
                    currentMeshData->velocities.resize(currentPoints.size());
                    for (size_t iPoint = 0; iPoint < currentPoints.size(); ++iPoint)
                    {
                        currentMeshData->velocities[iPoint] = (currentPoints[iPoint] - prevPoints[iPoint]) * _fps;
                    }
                    currentFrameData->velocitiesComputed = true;
                }
            }
        }

        //-----------------------------------------------------------------------------
        GolaemUSD_DataImpl::SkinMeshEntityFrameData::SP GolaemUSD_DataImpl::_ComputeSkinMeshEntity(EntityData::SP entityData, double frame)
        {
            SkinMeshEntityFrameData::SP skinMeshEntityFrameData = entityData->getFrameData<SkinMeshEntityFrameData>(frame, static_cast<size_t>(_params.glmCachedFramesCount));

            if (skinMeshEntityFrameData->entityData != nullptr)
            {
                // getFrameData returned an existing SkelEntityFrameData
                return skinMeshEntityFrameData;
            }

            // getFrameData returned a new SkinMeshEntityFrameData, set entityData to mark it as computed
            skinMeshEntityFrameData->entityData = entityData;

#ifdef TRACY_ENABLE
            ZoneScopedNC("ComputeSkinMeshEntity", GLM_COLOR_CACHE);
            glm::GlmString entityIdStr = "EntityId=" + glm::toString(entityData->inputGeoData._entityId);
            ZoneText(entityIdStr.c_str(), entityIdStr.size());
            glm::GlmString frameStr = "Frame=" + glm::toString(frame);
            ZoneText(frameStr.c_str(), frameStr.size());
#endif

            _ComputeEntity(skinMeshEntityFrameData, frame);
            if (!skinMeshEntityFrameData->enabled)
            {
                return skinMeshEntityFrameData;
            }

            SkinMeshEntityData::SP skinMeshEntityData = glm::staticCast<SkinMeshEntityData>(entityData);

            const glm::crowdio::GlmFrameData* frameData = entityData->inputGeoData._frameDatas[0];

            GolaemDisplayMode::Value displayMode = (GolaemDisplayMode::Value)_params.glmDisplayMode;

            auto& characterTemplateData = _skinMeshTemplateDataPerCharPerGeomFile[entityData->inputGeoData._characterIdx];

            if (displayMode == GolaemDisplayMode::BOUNDING_BOX)
            {
                skinMeshEntityFrameData->meshLodData.resize(1);
                SkinMeshLodData::SP skinMeshLodData = new SkinMeshLodData();
                skinMeshLodData->enabled = true;
                skinMeshLodData->entityData = entityData;
                skinMeshEntityFrameData->meshLodData[0] = skinMeshLodData;

                SkinMeshData::SP meshData = new SkinMeshData();
                skinMeshLodData->meshData[{0, 0}] = meshData;

                auto& lodTemplateData = characterTemplateData[0];
                meshData->templateData = lodTemplateData.at({0, 0});
                meshData->points = meshData->templateData->defaultPoints;
                meshData->normals = meshData->templateData->defaultNormals;
            }
            else if (displayMode == GolaemDisplayMode::SKINMESH)
            {
                // these variables must be available when glmPrepareEntityGeometry is called below
                float entityPos[3] = {0, 0, 0};
                float cameraPos[3] = {0, 0, 0};
                glm::crowdio::OutputEntityGeoData outputData; // TODO: see if storage is better

                if (entityData->inputGeoData._enableLOD)
                {
                    // update LOD data
                    memcpy(entityPos, skinMeshEntityFrameData->pos.data(), sizeof(float[3]));
                    if (_params.glmLodMode == 1)
                    {
                        // in static lod mode get the camera pos directly from the params
                        memcpy(cameraPos, _params.glmCameraPos.data(), sizeof(float[3]));
                    }
                    else if (_params.glmLodMode == 2)
                    {
                        // in dynamic lod mode get the camera pos from the node attributes (it may be connected to another attribute - usdWrapper will do the update)
                        const VtValue* cameraPosValue = TfMapLookupPtr(_usdParams, _golaemTokens->glmCameraPos);
                        if (cameraPosValue != NULL)
                        {
                            if (cameraPosValue->IsHolding<GfVec3f>())
                            {
                                const GfVec3f& usdValue = cameraPosValue->UncheckedGet<GfVec3f>();
                                memcpy(cameraPos, usdValue.data(), sizeof(float[3]));
                            }
                        }
                    }

                    entityData->inputGeoData._entityPos = entityPos;
                    entityData->inputGeoData._cameraWorldPosition = cameraPos;
                }

                skinMeshEntityFrameData->meshLodData.resize(characterTemplateData.size());
                for (size_t iLod = 0; iLod < characterTemplateData.size(); ++iLod)
                {
                    SkinMeshLodData::SP skinMeshLodData = new SkinMeshLodData();
                    skinMeshLodData->enabled = false;
                    skinMeshLodData->entityData = entityData;
                    skinMeshEntityFrameData->meshLodData[iLod] = skinMeshLodData;
                }

                glm::crowdio::GlmGeometryGenerationStatus geoStatus = glm::crowdio::glmPrepareEntityGeometry(&entityData->inputGeoData, &outputData);
                if (geoStatus == glm::crowdio::GIO_SUCCESS)
                {
                    skinMeshEntityFrameData->geometryFileIdx = outputData._geometryFileIndexes[0];

                    const GeometryAsset* geometryAsset = entityData->inputGeoData._character->getGeometryAsset(entityData->inputGeoData._geometryTag, skinMeshEntityFrameData->geometryFileIdx);
                    if (geometryAsset)
                    {
                        GlmString lodLevelString;
                        getStringFromLODLevel(static_cast<LODLevelFlags::Value>(geometryAsset->_lodLevel), lodLevelString);
                        skinMeshEntityFrameData->lodName = TfToken(lodLevelString.c_str());
                    }

                    size_t lodLevel = _params.glmLodMode == 0 ? 0 : skinMeshEntityFrameData->geometryFileIdx;
                    SkinMeshLodData::SP lodData = skinMeshEntityFrameData->meshLodData[lodLevel];

                    auto& lodTemplateData = characterTemplateData[skinMeshEntityFrameData->geometryFileIdx];

                    // update lod visibility
                    lodData->enabled = true;
                    glm::Array<glm::Array<glm::Vector3>>& frameDeformedVertices = outputData._deformedVertices[0];
                    glm::Array<glm::Array<glm::Vector3>>& frameDeformedNormals = outputData._deformedNormals[0];

                    if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                    {
                        crowdio::CrowdFBXCharacter* fbxCharacter = outputData._fbxCharacters[0];
                        // ----- FBX specific data
                        FbxAMatrix nodeTransform;
                        FbxAMatrix geomTransform;
                        FbxAMatrix identityMatrix;
                        identityMatrix.SetIdentity();
                        FbxTime fbxTime;
                        FbxVector4 fbxVect;
                        // ----- end FBX specific data

                        // Extract frame
                        if (outputData._geoBeInfo._idGeometryFileIdx != -1)
                        {
                            float (&geometryFrameCacheData)[3] = frameData->_geoBehaviorAnimFrameInfo[outputData._geoBeInfo._geoDataIndex];
                            double frameRate(FbxTime::GetFrameRate(fbxCharacter->touchFBXScene()->GetGlobalSettings().GetTimeMode()));
                            fbxTime.SetGlobalTimeMode(FbxTime::eCustom, frameRate);
                            fbxTime.SetMilliSeconds(long((double)geometryFrameCacheData[0] / frameRate * 1000.0));
                        }
                        else
                        {
                            fbxTime = 0;
                        }

                        for (size_t iRenderMesh = 0, meshCount = outputData._meshAssetNameIndices.size(); iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            size_t iGeoFileMesh = outputData._meshAssetNameIndices[iRenderMesh];

                            // meshDeformedVertices contains all fbx points, not just the ones that were filtered by vertexMasks
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iGeoFileMesh];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                            FbxNode* fbxNode = fbxCharacter->getCharacterFBXMeshes()[iGeoFileMesh];
                            FbxMesh* fbxMesh = fbxCharacter->getCharacterFBXMesh(iGeoFileMesh);

                            // for each mesh, get the transform in case of its position in not relative to the center of the world
                            fbxCharacter->getMeshGlobalTransform(nodeTransform, fbxNode, fbxTime);
                            glm::crowdio::CrowdFBXBaker::getGeomTransform(geomTransform, fbxNode);
                            nodeTransform *= geomTransform;

                            FbxLayer* fbxLayer0 = fbxMesh->GetLayer(0);
                            bool hasNormals = false;
                            bool hasMaterials = false;
                            FbxLayerElementMaterial* materialElement = NULL;
                            if (fbxLayer0 != NULL)
                            {
                                hasNormals = fbxLayer0->GetNormals() != NULL;
                                materialElement = fbxLayer0->GetMaterials();
                                hasMaterials = materialElement != NULL;
                            }

                            bool hasTransform = !(nodeTransform == identityMatrix);

                            unsigned int fbxVertexCount = fbxMesh->GetControlPointsCount();
                            unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();

                            glm::PODArray<int> vertexMasks;
                            glm::PODArray<int> polygonMasks;

                            vertexMasks.assign(fbxVertexCount, -1);
                            polygonMasks.assign(fbxPolyCount, 0);

                            int gchaMeshId = outputData._gchaMeshIds[iRenderMesh];
                            int meshMaterialIndex = outputData._meshAssetMaterialIndices[iRenderMesh];
                            std::pair<int, int> meshKey = {gchaMeshId, meshMaterialIndex};

                            SkinMeshData::SP meshData = new SkinMeshData();
                            lodData->meshData[meshKey] = meshData;
                            meshData->templateData = lodTemplateData.at(meshKey);

                            meshData->points.resize(meshData->templateData->defaultPoints.size());
                            meshData->normals.resize(meshData->templateData->defaultNormals.size());

                            // check material id and reconstruct data
                            for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                            {
                                int currentMtlIdx = 0;
                                if (hasMaterials)
                                {
                                    currentMtlIdx = materialElement->GetIndexArray().GetAt(iFbxPoly);
                                }
                                if (currentMtlIdx == meshMaterialIndex)
                                {
                                    polygonMasks[iFbxPoly] = 1;
                                    for (int iPolyVertex = 0, polyVertexCount = fbxMesh->GetPolygonSize(iFbxPoly); iPolyVertex < polyVertexCount; ++iPolyVertex)
                                    {
                                        int iFbxVertex = fbxMesh->GetPolygonVertex(iFbxPoly, iPolyVertex);
                                        int& vertexMask = vertexMasks[iFbxVertex];
                                        if (vertexMask >= 0)
                                        {
                                            continue;
                                        }
                                        vertexMask = 0;
                                    }
                                }
                            }

                            for (unsigned int iFbxVertex = 0, iActualVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                            {
                                int& vertexMask = vertexMasks[iFbxVertex];
                                if (vertexMask >= 0)
                                {
                                    vertexMask = iActualVertex;
                                    ++iActualVertex;
                                }
                            }

                            unsigned int iActualVertex = 0;
                            for (unsigned int iFbxVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                            {
                                int& vertexMask = vertexMasks[iFbxVertex];
                                if (vertexMask >= 0)
                                {
                                    // meshDeformedVertices contains all fbx points, not just the ones that were filtered by vertexMasks
                                    GfVec3f& point = meshData->points[iActualVertex];
                                    // vertices
                                    if (hasTransform)
                                    {
                                        const Vector3& glmVect = meshDeformedVertices[iFbxVertex];
                                        fbxVect.Set(glmVect.x, glmVect.y, glmVect.z);
                                        // transform vertex in case of local transformation
                                        fbxVect = nodeTransform.MultT(fbxVect);
                                        point.Set((float)fbxVect[0], (float)fbxVect[1], (float)fbxVect[2]);
                                    }
                                    else
                                    {
                                        const Vector3& meshVertex = meshDeformedVertices[iFbxVertex];
                                        point.Set(meshVertex.getFloatValues());
                                    }

                                    point -= skinMeshEntityFrameData->pos;

                                    ++iActualVertex;
                                }
                            }

                            if (hasNormals)
                            {
                                FbxAMatrix globalRotate(identityMatrix);
                                globalRotate.SetR(nodeTransform.GetR());
                                bool hasRotate = globalRotate != identityMatrix;

                                const glm::Array<glm::Vector3>& meshDeformedNormals = frameDeformedNormals[iGeoFileMesh];

                                // normals are always stored per polygon vertex
                                for (unsigned int iFbxPoly = 0, iFbxNormal = 0, iActualPolyVertex = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                                {
                                    int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                    if (polygonMasks[iFbxPoly])
                                    {
                                        for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++iFbxNormal, ++iActualPolyVertex)
                                        {
                                            // meshDeformedNormals contains all fbx normals, not just the ones that were filtered by polygonMasks
                                            // do not reverse polygon order
                                            if (hasRotate)
                                            {
                                                const Vector3& glmVect = meshDeformedNormals[iFbxNormal];
                                                fbxVect.Set(glmVect.x, glmVect.y, glmVect.z);
                                                fbxVect = globalRotate.MultT(fbxVect);
                                                meshData->normals[iActualPolyVertex].Set(
                                                    (float)fbxVect[0], (float)fbxVect[1], (float)fbxVect[2]);
                                            }
                                            else
                                            {
                                                const glm::Vector3& deformedNormal = meshDeformedNormals[iFbxNormal];
                                                meshData->normals[iActualPolyVertex].Set(
                                                    deformedNormal.getFloatValues());
                                            }
                                        }
                                    }
                                    else
                                    {
                                        iFbxNormal += polySize;
                                    }
                                }
                            }
                        }
                    }
                    else if (outputData._geoType == glm::crowdio::GeometryType::GCG)
                    {
                        crowdio::CrowdGcgCharacter* gcgCharacter = outputData._gcgCharacters[0];
                        for (size_t iRenderMesh = 0, meshCount = outputData._meshAssetNameIndices.size(); iRenderMesh < meshCount; ++iRenderMesh)
                        {
                            const glm::Array<glm::Vector3>& meshDeformedVertices = frameDeformedVertices[iRenderMesh];
                            size_t vertexCount = meshDeformedVertices.size();
                            if (vertexCount == 0)
                            {
                                continue;
                            }

                            int gchaMeshId = outputData._gchaMeshIds[iRenderMesh];
                            int meshMaterialIndex = outputData._meshAssetMaterialIndices[iRenderMesh];
                            std::pair<int, int> meshKey = {gchaMeshId, meshMaterialIndex};

                            SkinMeshData::SP meshData = new SkinMeshData();
                            lodData->meshData[meshKey] = meshData;
                            meshData->templateData = lodTemplateData.at(meshKey);

                            meshData->points.resize(meshData->templateData->defaultPoints.size());
                            meshData->normals.resize(meshData->templateData->defaultNormals.size());

                            for (size_t iVertex = 0; iVertex < vertexCount; ++iVertex)
                            {
                                const glm::Vector3& meshVertex = meshDeformedVertices[iVertex];
                                GfVec3f& point = meshData->points[iVertex];
                                point.Set(meshVertex.getFloatValues());
                                point -= skinMeshEntityFrameData->pos;
                            }

                            const glm::Array<glm::Vector3>& meshDeformedNormals = frameDeformedNormals[iRenderMesh];

                            glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iRenderMesh]];
                            glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

                            // add normals
                            if (assetFileMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_POLYGON_VERTEX)
                            {
                                for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                {
                                    uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                    for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iVertex)
                                    {
                                        // do not reverse polygon order
                                        const glm::Vector3& vtxNormal = meshDeformedNormals[iVertex];
                                        meshData->normals[iVertex].Set(vtxNormal.getFloatValues());
                                    }
                                }
                            }
                            else
                            {
                                uint32_t* polygonNormalIndices = assetFileMesh._normalMode == glm::crowdio::GLM_NORMAL_PER_CONTROL_POINT ? assetFileMesh._polygonsVertexIndices : assetFileMesh._polygonsNormalIndices;
                                for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                                {
                                    uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                    for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iVertex)
                                    {
                                        // do not reverse polygon order
                                        uint32_t normalIdx = polygonNormalIndices[iVertex];
                                        const glm::Vector3& vtxNormal = meshDeformedNormals[normalIdx];
                                        meshData->normals[iVertex].Set(vtxNormal.getFloatValues());
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return skinMeshEntityFrameData;
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_InvalidateEntity(EntityFrameData::SP entityFrameData)
        {
            EntityData::SP entityData = entityFrameData->entityData;
            entityFrameData->enabled = false;
            entityData->inputGeoData._frames.clear();
            entityData->inputGeoData._frameDatas.clear();
            entityFrameData->intShaderAttrValues.clear();
            entityFrameData->floatShaderAttrValues.clear();
            entityFrameData->stringShaderAttrValues.clear();
            entityFrameData->vectorShaderAttrValues.clear();
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_getCharacterExtent(EntityData::SP entityData, GfVec3f& extent) const
        {
            glm::Vector3 halfExtents(1, 1, 1);
            size_t geoIdx = 0;
            const glm::GeometryAsset* geoAsset = entityData->inputGeoData._character->getGeometryAsset(entityData->inputGeoData._geometryTag, geoIdx); // any LOD should have same extents !
            if (geoAsset != NULL)
            {
                halfExtents = geoAsset->_halfExtentsYUp;
            }
            float characterScale = entityData->inputGeoData._simuData->_scales[entityData->inputGeoData._entityIndex];
            halfExtents *= characterScale;
            extent.Set(halfExtents[0], halfExtents[1], halfExtents[2]);
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_ComputeBboxData(SkinMeshEntityData::SP entityData)
        {
            glm::GlmString meshName = "BBOX";

            GlmMap<GlmString, SdfPath> meshTreePaths;
            SdfPath lastMeshTransformPath = _CreateHierarchyFor(meshName, entityData->entityPath, meshTreePaths);

            SkinMeshMapData& meshMapData = _skinMeshDataMap[lastMeshTransformPath];
            meshMapData.lodIndex = 0;
            meshMapData.gchaMeshId = 0;
            meshMapData.meshMaterialIndex = 0;
            meshMapData.entityData = entityData;
            meshMapData.templateData = _skinMeshTemplateDataPerCharPerGeomFile[0][0].at({0, 0});

            // compute the bounding box of the current entity
            GfVec3f halfExtents;
            _getCharacterExtent(entityData, halfExtents);

            // create the shape of the bounding box
            VtVec3fArray& points = meshMapData.templateData->defaultPoints;
            points.resize(8);

            points[0].Set(
                -halfExtents[0],
                -halfExtents[1],
                +halfExtents[2]);

            points[1].Set(
                +halfExtents[0],
                -halfExtents[1],
                +halfExtents[2]);

            points[2].Set(
                +halfExtents[0],
                -halfExtents[1],
                -halfExtents[2]);

            points[3].Set(
                -halfExtents[0],
                -halfExtents[1],
                -halfExtents[2]);

            points[4].Set(
                -halfExtents[0],
                +halfExtents[1],
                +halfExtents[2]);

            points[5].Set(
                +halfExtents[0],
                +halfExtents[1],
                +halfExtents[2]);

            points[6].Set(
                +halfExtents[0],
                +halfExtents[1],
                -halfExtents[2]);

            points[7].Set(
                -halfExtents[0],
                +halfExtents[1],
                -halfExtents[2]);

            VtVec3fArray& vertexNormals = meshMapData.templateData->defaultNormals;
            vertexNormals.resize(24);

            int vertexIdx = 0;

            // face 0
            for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
            {
                vertexNormals[vertexIdx].Set(0, -1, 0);
            }

            // face 1
            for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
            {
                vertexNormals[vertexIdx].Set(1, 0, 0);
            }

            // face 2
            for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
            {
                vertexNormals[vertexIdx].Set(0, 0, -1);
            }

            // face 3
            for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
            {
                vertexNormals[vertexIdx].Set(-1, 0, 0);
            }

            // face 4
            for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
            {
                vertexNormals[vertexIdx].Set(0, 0, 1);
            }

            // face 5
            for (int iVtx = 0; iVtx < 4; ++iVtx, ++vertexIdx)
            {
                vertexNormals[vertexIdx].Set(0, 1, 0);
            }
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::_ComputeSkinMeshTemplateData(std::map<std::pair<int, int>, SkinMeshTemplateData::SP>& lodTemplateData, const glm::crowdio::InputEntityGeoData& inputGeoData, const glm::crowdio::OutputEntityGeoData& outputData)
        {
            glm::GlmString materialPath = _params.glmMaterialPath.GetText();
            GolaemMaterialAssignMode::Value materialAssignMode = (GolaemMaterialAssignMode::Value)_params.glmMaterialAssignMode;

            int velocitiesShaderAttributeIndex = inputGeoData._character->findShaderAttributeIdx("glmEnableUsdVelocities");
            int velocitiesIntShaderAttributeIndex = -1;
            if (velocitiesShaderAttributeIndex >= 0 && inputGeoData._characterIdx < _globalToSpecificShaderAttrIdxPerChar.size())
            {
                const glm::ShaderAttribute& shaderAttribute = inputGeoData._character->_shaderAttributes[velocitiesShaderAttributeIndex];
                if (shaderAttribute._type == ShaderAttributeType::INT)
                {
                    const PODArray<size_t>& globalToSpecificShaderAttrIdx = _globalToSpecificShaderAttrIdxPerChar[inputGeoData._characterIdx]; // no bounds check needed, characters are consistent across crowd fields
                    if (static_cast<size_t>(velocitiesShaderAttributeIndex) < globalToSpecificShaderAttrIdx.size())
                    {
                        velocitiesIntShaderAttributeIndex = static_cast<int>(globalToSpecificShaderAttrIdx[velocitiesShaderAttributeIndex]);
                    }
                    else
                    {
                        GLM_CROWD_TRACE_WARNING("Shader attribute " << shaderAttribute._name << " index is out of bounds in globalToSpecificShaderAttrIdx, cannot be used to enable USD velocities.");
                    }
                }
                else
                {
                    GLM_CROWD_TRACE_WARNING("Shader attribute " << shaderAttribute._name << " is not of type int, cannot be used to enable USD velocities.");
                }
            }

            const glm::PODArray<int>& shadingGroupToSurfaceShader = _sgToSsPerChar[inputGeoData._characterIdx];

            glm::GlmString meshName, meshAlias, materialSuffix;

            size_t meshCount = outputData._meshAssetNameIndices.size();
            for (size_t iRenderMesh = 0; iRenderMesh < meshCount; ++iRenderMesh)
            {
                meshName = outputData._meshAssetNames[outputData._meshAssetNameIndices[iRenderMesh]];
                meshAlias = outputData._meshAssetAliases[outputData._meshAssetNameIndices[iRenderMesh]];
                int gchaMeshId = outputData._gchaMeshIds[iRenderMesh];
                int meshMaterialIndex = outputData._meshAssetMaterialIndices[iRenderMesh];
                if (meshMaterialIndex != 0)
                {
                    materialSuffix = glm::toString(meshMaterialIndex);
                    meshName += materialSuffix;
                    meshAlias += materialSuffix;
                }

                // create USD hierarchy based on alias export per mesh data
                meshAlias.trim("|");
                if (meshAlias.empty())
                {
                    meshAlias = meshName;
                }

                SkinMeshTemplateData::SP meshTemplateData = new SkinMeshTemplateData();
                lodTemplateData[{gchaMeshId, meshMaterialIndex}] = meshTemplateData;
                meshTemplateData->meshAlias = meshAlias;

                if (outputData._geoType == glm::crowdio::GeometryType::FBX)
                {
                    crowdio::CrowdFBXCharacter* fbxCharacter = outputData._fbxCharacters[0];
                    size_t iGeoFileMesh = outputData._meshAssetNameIndices[iRenderMesh];
                    // when fbxMesh == NULL, vertexCount == 0, so no need to check fbxMesh != NULL
                    FbxMesh* fbxMesh = fbxCharacter->getCharacterFBXMesh(iGeoFileMesh);

                    FbxLayer* fbxLayer0 = fbxMesh->GetLayer(0);
                    bool hasMaterials = false;
                    FbxLayerElementMaterial* materialElement = NULL;
                    if (fbxLayer0 != NULL)
                    {
                        materialElement = fbxLayer0->GetMaterials();
                        hasMaterials = materialElement != NULL;
                    }

                    unsigned int fbxVertexCount = fbxMesh->GetControlPointsCount();
                    unsigned int fbxPolyCount = fbxMesh->GetPolygonCount();
                    glm::PODArray<int> vertexMasks;
                    glm::PODArray<int> polygonMasks;

                    vertexMasks.assign(fbxVertexCount, -1);
                    polygonMasks.assign(fbxPolyCount, 0);

                    int meshMtlIdx = outputData._meshAssetMaterialIndices[iRenderMesh];

                    // check material id and reconstruct data
                    for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                    {
                        int currentMtlIdx = 0;
                        if (hasMaterials)
                        {
                            currentMtlIdx = materialElement->GetIndexArray().GetAt(iFbxPoly);
                        }
                        if (currentMtlIdx == meshMtlIdx)
                        {
                            polygonMasks[iFbxPoly] = 1;
                            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                            for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                            {
                                int iFbxVertex = fbxMesh->GetPolygonVertex(iFbxPoly, iPolyVertex);
                                int& vertexMask = vertexMasks[iFbxVertex];
                                if (vertexMask >= 0)
                                {
                                    continue;
                                }
                                vertexMask = 0;
                            }
                        }
                    }

                    int iActualVertex = 0;
                    for (unsigned int iFbxVertex = 0; iFbxVertex < fbxVertexCount; ++iFbxVertex)
                    {
                        int& vertexMask = vertexMasks[iFbxVertex];
                        if (vertexMask >= 0)
                        {
                            vertexMask = iActualVertex;
                            ++iActualVertex;
                        }
                    }

                    meshTemplateData->defaultPoints.assign(iActualVertex, GfVec3f(0.0f, 0.0f, 0.0f));

                    for (unsigned int iFbxPoly = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                    {
                        if (polygonMasks[iFbxPoly])
                        {
                            int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                            meshTemplateData->faceVertexCounts.push_back(polySize);
                            for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex)
                            {
                                // do not reverse polygon order
                                int iFbxVertex = fbxMesh->GetPolygonVertex(iFbxPoly, iPolyVertex);
                                int vertexId = vertexMasks[iFbxVertex];
                                meshTemplateData->faceVertexIndices.push_back(vertexId);
                            } // iPolyVertex
                        }
                    }

                    meshTemplateData->defaultNormals.assign(meshTemplateData->faceVertexIndices.size(), GfVec3f(0.0f, 0.0f, 0.0f));

                    // find how many uv layers are available
                    int uvSetCount = fbxMesh->GetLayerCount(FbxLayerElement::eUV);
                    meshTemplateData->uvSets.resize(uvSetCount);
                    FbxLayerElementUV* uvElement = NULL;
                    for (int iUVSet = 0; iUVSet < uvSetCount; ++iUVSet)
                    {
                        VtVec2fArray& uvs = meshTemplateData->uvSets[iUVSet];
                        uvs.resize(meshTemplateData->faceVertexIndices.size());
                        FbxLayer* layer = fbxMesh->GetLayer(fbxMesh->GetLayerTypedIndex((int)iUVSet, FbxLayerElement::eUV));
                        uvElement = layer->GetUVs();
                        bool uvsByControlPoint = uvElement->GetMappingMode() == FbxLayerElement::eByControlPoint;
                        bool uvReferenceDirect = uvElement->GetReferenceMode() == FbxLayerElement::eDirect;

                        if (uvsByControlPoint)
                        {
                            int uvIndex = 0;
                            for (unsigned int iFbxPoly = 0, actualIndexByPolyVertex = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                            {
                                int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                if (polygonMasks[iFbxPoly])
                                {
                                    for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++actualIndexByPolyVertex)
                                    {
                                        // do not reverse polygon order
                                        uvIndex = vertexMasks[fbxMesh->GetPolygonVertex(iFbxPoly, iPolyVertex)];
                                        if (!uvReferenceDirect)
                                        {
                                            uvIndex = uvElement->GetIndexArray().GetAt(uvIndex);
                                        }
                                        FbxVector2 tempUV(uvElement->GetDirectArray().GetAt(uvIndex));
                                        uvs[actualIndexByPolyVertex].Set((float)tempUV[0], (float)tempUV[1]);
                                    }
                                }
                            }
                        }
                        else
                        {
                            int uvIndex = 0;
                            for (unsigned int iFbxPoly = 0, actualIndexByPolyVertex = 0, fbxIndexByPolyVertex = 0; iFbxPoly < fbxPolyCount; ++iFbxPoly)
                            {
                                int polySize = fbxMesh->GetPolygonSize(iFbxPoly);
                                if (polygonMasks[iFbxPoly])
                                {
                                    for (int iPolyVertex = 0; iPolyVertex < polySize; ++iPolyVertex, ++fbxIndexByPolyVertex, ++actualIndexByPolyVertex)
                                    {
                                        // do not reverse polygon order
                                        uvIndex = fbxIndexByPolyVertex;
                                        if (!uvReferenceDirect)
                                        {
                                            uvIndex = uvElement->GetIndexArray().GetAt(uvIndex);
                                        }

                                        FbxVector2 tempUV(uvElement->GetDirectArray().GetAt(uvIndex));
                                        uvs[actualIndexByPolyVertex].Set((float)tempUV[0], (float)tempUV[1]);
                                    } // iPolyVertex
                                }
                                else
                                {
                                    fbxIndexByPolyVertex += polySize;
                                }
                            } // iPoly
                        }
                    }
                }
                else if (outputData._geoType == glm::crowdio::GeometryType::GCG)
                {
                    crowdio::CrowdGcgCharacter* gcgCharacter = outputData._gcgCharacters[0];

                    glm::crowdio::GlmFileMeshTransform& assetFileMeshTransform = gcgCharacter->getGeometry()._transforms[outputData._transformIndicesInGcgFile[iRenderMesh]];
                    glm::crowdio::GlmFileMesh& assetFileMesh = gcgCharacter->getGeometry()._meshes[assetFileMeshTransform._meshIndex];

                    meshTemplateData->defaultPoints.assign(assetFileMesh._vertexCount, GfVec3f(0.0f, 0.0f, 0.0f));

                    for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                    {
                        uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                        meshTemplateData->faceVertexCounts.push_back(polySize);
                        for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iVertex)
                        {
                            // do not reverse polygon order
                            int vertexId = assetFileMesh._polygonsVertexIndices[iVertex];
                            meshTemplateData->faceVertexIndices.push_back(vertexId);
                        }
                    }

                    meshTemplateData->defaultNormals.assign(meshTemplateData->faceVertexIndices.size(), GfVec3f(0.0f, 0.0f, 0.0f));

                    meshTemplateData->uvSets.resize(assetFileMesh._uvSetCount);
                    for (size_t iUVSet = 0; iUVSet < assetFileMesh._uvSetCount; ++iUVSet)
                    {
                        VtVec2fArray& uvs = meshTemplateData->uvSets[iUVSet];
                        uvs.resize(meshTemplateData->faceVertexIndices.size());

                        if (assetFileMesh._uvMode == glm::crowdio::GLM_UV_PER_CONTROL_POINT)
                        {
                            for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                            {
                                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iVertex)
                                {
                                    // do not reverse polygon order
                                    uint32_t uvIndex = assetFileMesh._polygonsVertexIndices[iVertex];
                                    uvs[iVertex].Set(assetFileMesh._us[iUVSet][uvIndex], assetFileMesh._vs[iUVSet][uvIndex]);
                                }
                            }
                        }
                        else
                        {
                            for (uint32_t iPoly = 0, iVertex = 0; iPoly < assetFileMesh._polygonCount; ++iPoly)
                            {
                                uint32_t polySize = assetFileMesh._polygonsVertexCount[iPoly];
                                for (uint32_t iPolyVtx = 0; iPolyVtx < polySize; ++iPolyVtx, ++iVertex)
                                {
                                    // do not reverse polygon order
                                    uint32_t uvIndex = assetFileMesh._polygonsUVIndices[iVertex];
                                    uvs[iVertex].Set(assetFileMesh._us[iUVSet][uvIndex], assetFileMesh._vs[iUVSet][uvIndex]);
                                }
                            }
                        }
                    }
                }
                int shadingGroupIdx = outputData._meshShadingGroups[iRenderMesh];

                glm::GlmString materialName = "";
                if (shadingGroupIdx >= 0)
                {
                    const glm::ShadingGroup& shGroup = inputGeoData._character->_shadingGroups[shadingGroupIdx];
                    materialName = materialPath;
                    materialName.rtrim("/");
                    materialName += "/";
                    switch (materialAssignMode)
                    {
                    case GolaemMaterialAssignMode::BY_SHADING_GROUP:
                    {
                        materialName += TfMakeValidIdentifier(shGroup._name.c_str());
                    }
                    break;
                    case GolaemMaterialAssignMode::BY_SURFACE_SHADER:
                    {
                        // get the surface shader
                        int shaderAssetIdx = shadingGroupToSurfaceShader[shadingGroupIdx];
                        if (shaderAssetIdx >= 0)
                        {
                            const glm::ShaderAsset& shAsset = inputGeoData._character->_shaderAssets[shaderAssetIdx];
                            materialName += TfMakeValidIdentifier(shAsset._name.c_str());
                        }
                        else
                        {
                            materialName += "DefaultGolaemMat";
                        }
                    }
                    break;
                    default:
                        break;
                    }

                    if (glm::glmFind(shGroup._shaderAttributes.begin(), shGroup._shaderAttributes.end(), velocitiesShaderAttributeIndex) != shGroup._shaderAttributes.end())
                    {
                        // if the shader attribute to enable velocities is present on the shading group, set it on the template data so it can be used later when creating the mesh data
                        meshTemplateData->velocitiesIntShaderAttributeIndex = velocitiesIntShaderAttributeIndex;
                        if (meshTemplateData->velocitiesIntShaderAttributeIndex >= 0)
                        {
                            meshTemplateData->defaultVelocities.assign(meshTemplateData->defaultPoints.size(), GfVec3f(0.0f, 0.0f, 0.0f));
                        }
                    }
                }

                if (materialAssignMode != GolaemMaterialAssignMode::NO_ASSIGNMENT)
                {
                    if (!materialName.empty())
                    {
                        meshTemplateData->materialPath = SdfPathListOp::CreateExplicit({SdfPath(materialName.c_str())});
                    }
                    else
                    {
                        meshTemplateData->materialPath = (*_skinMeshRelationships)[_skinMeshRelationshipTokens->materialBinding].defaultTargetPath;
                    }
                }
            }
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::HandleNotice(const UsdNotice::ObjectsChanged& notice)
        {
            // check if stage has changed
            RefreshUsdStage(notice.GetStage());

            // check if it's a gda property and change it
            UsdNotice::ObjectsChanged::PathRange changedPaths = notice.GetChangedInfoOnlyPaths();
            for (const SdfPath& changedPath : changedPaths)
            {
                if (changedPath.IsPropertyPath())
                {
                    SdfPath primPath = changedPath.GetAbsoluteRootOrPrimPath();
                    UsdPrim changedPrim = _usdWrapper._usdStage->GetPrimAtPath(primPath);
                    if (!changedPrim.IsValid())
                    {
                        continue;
                    }
                    if (UsdAttribute typeAttribute = changedPrim.GetAttribute(_golaemTokens->__glmNodeType__))
                    {
                        TfToken typeValue;
                        if (typeAttribute.Get(&typeValue) && typeValue == GolaemUSDFileFormatTokens->Id)
                        {
                            if (UsdAttribute nodeIdAttribute = changedPrim.GetAttribute(_golaemTokens->__glmNodeId__))
                            {
                                int nodeId = -1;
                                if (nodeIdAttribute.Get(&nodeId) && nodeId == _rootNodeIdInFinalStage)
                                {
                                    const TfToken& nameToken = changedPath.GetNameToken();
                                    if (VtValue* usdValue = TfMapLookupPtr(_usdParams, nameToken))
                                    {
                                        // get the new value
                                        if (UsdAttribute usdAttribute = changedPrim.GetAttribute(nameToken))
                                        {
                                            usdAttribute.Get(usdValue);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::RefreshUsdStage(UsdStagePtr usdStage)
        {
            if (usdStage != NULL && _usdWrapper._usdStage != usdStage)
            {
                _usdWrapper._usdStage = usdStage;

                // find the path to in the final stage
                SdfPathSet loadedPaths = usdStage->GetLoadSet();
                for (const SdfPath& loadedPath : loadedPaths)
                {
                    UsdPrim loadedPrim = usdStage->GetPrimAtPath(loadedPath);
                    if (!loadedPrim.IsValid())
                    {
                        continue;
                    }

                    if (UsdAttribute typeAttribute = loadedPrim.GetAttribute(_golaemTokens->__glmNodeType__))
                    {
                        TfToken typeValue;
                        if (typeAttribute.Get(&typeValue) && typeValue == GolaemUSDFileFormatTokens->Id)
                        {
                            if (UsdAttribute nodeIdAttribute = loadedPrim.GetAttribute(_golaemTokens->__glmNodeId__))
                            {
                                int nodeId = -1;
                                if (nodeIdAttribute.Get(&nodeId) && nodeId == _rootNodeIdInFinalStage)
                                {
                                    _rootPathInFinalStage = loadedPath;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!_rootPathInFinalStage.IsEmpty())
                {
                    _usdWrapper._connectedUsdParams.clear();
                    // refresh usd attributes
                    if (UsdPrim thisPrim = usdStage->GetPrimAtPath(_rootPathInFinalStage))
                    {
                        for (auto& itUsdParam : _usdParams)
                        {
                            if (UsdAttribute usdAttribute = thisPrim.GetAttribute(itUsdParam.first))
                            {
                                usdAttribute.Get(&itUsdParam.second);

                                // check for connections
                                SdfPathVector sourcePaths;
                                usdAttribute.GetConnections(&sourcePaths);
                                if (!sourcePaths.empty())
                                {
                                    _usdWrapper._connectedUsdParams.push_back({&itUsdParam.second, sourcePaths[0]});
                                }
                            }
                        }
                    }
                }
            }
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_DataImpl::UsdWrapper::update(const double& frame, glm::ScopedLockActivable<glm::Mutex>& scopedLock)
        {
            scopedLock.lock();
            if (glm::approxDiff(_currentFrame, frame, static_cast<double>(GLM_NUMERICAL_PRECISION)))
            {
                _currentFrame = frame;
                if (_usdStage != NULL)
                {
                    // update connected usd params
                    for (std::pair<VtValue*, SdfPath>& connectedParam : _connectedUsdParams)
                    {
                        if (connectedParam.second.IsPropertyPath())
                        {
                            SdfPath primPath = connectedParam.second.GetAbsoluteRootOrPrimPath();
                            if (UsdPrim prim = _usdStage->GetPrimAtPath(primPath))
                            {
                                const TfToken& nameToken = connectedParam.second.GetNameToken();
                                if (UsdAttribute usdAttribute = prim.GetAttribute(nameToken))
                                {
                                    VtValue attrValue;
                                    usdAttribute.Get(&attrValue, UsdTimeCode(_currentFrame));
                                    const std::type_info& currentTypeInfo = connectedParam.first->GetTypeid();
                                    if (attrValue.GetTypeid() == currentTypeInfo)
                                    {
                                        *connectedParam.first = attrValue;
                                    }
                                    else if (attrValue.CanCastToTypeid(currentTypeInfo))
                                    {
                                        *connectedParam.first = VtValue::CastToTypeid(attrValue, currentTypeInfo);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (_usdStage == NULL || _connectedUsdParams.empty())
            {
                // nothing to update, no need to keep the lock
                scopedLock.unlock();
            }
        }

    } // namespace usdplugin
} // namespace glm
