/***************************************************************************
 *                                                                          *
 *  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
 *                                                                          *
 ***************************************************************************/

#pragma once

#include "glmUSD.h"
#include "glmUSDData.h"

#include <glmSimulationCacheFactory.h>

namespace glm
{
    namespace usdplugin
    {
        using namespace PXR_INTERNAL_NS;

        struct GolaemDisplayMode
        {
            enum Value
            {
                BOUNDING_BOX,
                SKELETON,
                SKINMESH,
                END
            };
        };

        struct GolaemMaterialAssignMode
        {
            enum Value
            {
                BY_SURFACE_SHADER,
                BY_SHADING_GROUP,
                NO_ASSIGNMENT,
                END
            };
        };

        class GolaemUSD_DataImpl
        {
        private:
            // cached data for each entity
            struct EntityData
            {
                std::map<TfToken, size_t, TfTokenFastArbitraryLessThan> ppAttrIndexes;
                std::map<TfToken, size_t, TfTokenFastArbitraryLessThan> shaderAttrIndexes;

                SdfPath entityPath;

                double computedTimeSample = 0;
                bool excluded = false; // excluded by layout - the entity will always be empty
                bool enabled = true;   // can vary during simulation (kill, emit)
                uint32_t bonePositionOffset = 0;
                glm::Mutex* cachedSimulationLock = NULL;
                glm::Mutex* entityComputeLock = NULL; // do not allow simultaneous computes of the same entity

                glm::PODArray<int> intShaderAttrValues;
                glm::PODArray<float> floatShaderAttrValues;
                glm::Array<TfToken> stringShaderAttrValues;
                glm::Array<GfVec3f> vectorShaderAttrValues;

                glm::PODArray<float> floatPPAttrValues;
                glm::Array<GfVec3f> vectorPPAttrValues;

                glm::crowdio::InputEntityGeoData inputGeoData;
                glm::crowdio::CachedSimulation* cachedSimulation = NULL;

                GfVec3f pos{0, 0, 0};

                ~EntityData();
                void initEntityLock();
            };

            struct SkinMeshData;
            struct SkinMeshLodData;
            struct SkinMeshEntityData : public EntityData
            {
                glm::PODArray<SkinMeshLodData*> meshLodData; // used when lod is enabled (glmLodMode > 0)
                glm::PODArray<SkinMeshData*> meshData;       // used when no lod (glmLodMode == 0)

                size_t geometryFileIdx = 0; // to check if LOD changed
            };

            struct SkelAnimData;
            struct SkelEntityData : public EntityData
            {
                SkelAnimData* animData = NULL;
                SdfReferenceListOp referencedUsdCharacter;
                SdfVariantSelectionMap geoVariants;

                SdfPathListOp animationSourcePath;
                SdfPathListOp skeletonPath;
            };

            struct SkinMeshLodData;
            struct SkinMeshTemplateData;
            struct SkinMeshData
            {
                SkinMeshLodData* lodData = NULL;       // used when lod is enabled (glmLodMode > 0)
                SkinMeshEntityData* entityData = NULL; // used when no lod (glmLodMode == 0)

                // these parameters are animated
                VtVec3fArray points;
                VtVec3fArray normals; // stored by polygon vertex

                const SkinMeshTemplateData* templateData = NULL;
                SdfPath meshPath;
            };

            struct SkinMeshTemplateData
            {
                VtIntArray faceVertexCounts;
                VtIntArray faceVertexIndices;
                glm::Array<VtVec2fArray> uvSets; // stored by polygon vertex
                GlmString meshAlias;
                int pointsCount;
                // int normalsCount; // not needed, = faceVertexIndices.size();
                SdfPathListOp materialPath;
            };

            struct SkinMeshLodData
            {
                glm::PODArray<SkinMeshData*> meshData;
                SkinMeshEntityData* entityData = NULL;
                bool enabled = false;
                SdfPath lodPath;
            };

            struct SkelAnimData
            {
                VtTokenArray joints;
                VtQuatfArray rotations;
                VtVec3hArray scales;
                bool scalesAnimated = false;
                uint32_t boneSnsOffset = 0;

                VtVec3fArray translations;
                SkelEntityData* entityData = NULL;
            };

            struct UsdWrapper
            {
            public:
                glm::Array<std::pair<VtValue*, SdfPath>> _connectedUsdParams;
                UsdStagePtr _usdStage = NULL; // from GolaemUSD_DataImpl
                glm::Mutex _updateLock;

            protected:
                double _currentFrame = -FLT_MAX;

            public:
                inline const double& getCurrentFrame() const;
                void update(const double& frame, glm::ScopedLockActivable<glm::Mutex>& scopedLock);
            };

        private:
            // The parameters use to generate specs and time samples, obtained from the
            // layer's file format arguments.
            GolaemUSD_DataParams _params;

            crowdio::SimulationCacheFactory* _factory;
            glm::Array<glm::PODArray<int>> _sgToSsPerChar;
            glm::Array<PODArray<int>> _snsIndicesPerChar;
            glm::Array<glm::Array<std::map<std::pair<int, int>, SkinMeshTemplateData>>> _skinMeshTemplateDataPerCharPerLod;

            glm::Array<GlmString> _shaderAttrTypes;
            glm::Array<VtValue> _shaderAttrDefaultValues;

            glm::Array<GlmString> _ppAttrTypes;
            glm::Array<VtValue> _ppAttrDefaultValues;

            int _startFrame;
            int _endFrame;
            float _fps = 24;

            // Cached set of generated time sample times. All of the animated property
            // time sample fields have the same time sample times.
            std::set<double> _animTimeSampleTimes;

            // Cached set of all paths with a generated prim spec.
            TfHashSet<SdfPath, SdfPath::Hash> _primSpecPaths;

            // Cached list of the names of all child prims for each generated prim spec
            // that is not a leaf. The child prim names are the same for all prims that
            // make up the cube layout hierarchy.
            TfHashMap<SdfPath, std::vector<TfToken>, SdfPath::Hash> _primChildNames;

            TfHashMap<SdfPath, SkinMeshEntityData, SdfPath::Hash> _skinMeshEntityDataMap;

            TfHashMap<SdfPath, SkelEntityData, SdfPath::Hash> _skelEntityDataMap;

            TfHashMap<SdfPath, SkinMeshData, SdfPath::Hash> _skinMeshDataMap;

            TfHashMap<SdfPath, SkinMeshLodData, SdfPath::Hash> _skinMeshLodDataMap;

            TfHashMap<SdfPath, SkelAnimData, SdfPath::Hash> _skelAnimDataMap;

            glm::PODArray<glm::Mutex*> _cachedSimulationLocks;

            UsdWrapper _usdWrapper;

            std::map<TfToken, VtValue, TfTokenFastArbitraryLessThan> _usdParams; // additional usd params and their value

            SdfPath _rootPathInFinalStage;
            int _rootNodeIdInFinalStage = -1;

        public:
            GolaemUSD_DataImpl(const GolaemUSD_DataParams& params);
            ~GolaemUSD_DataImpl();

            /// Returns true if the parameters produce no specs
            bool IsEmpty() const;

            /// Generates the spec type for the path.
            SdfSpecType GetSpecType(const SdfPath& path) const;

            /// Returns whether a value should exist for the given \a path and
            /// \a fieldName. Optionally returns the value if it exists.
            bool Has(const SdfPath& path, const TfToken& field, VtValue* value = NULL);

            /// Visits every spec generated from our params with the given
            /// \p visitor.
            void VisitSpecs(const SdfAbstractData& data, SdfAbstractDataSpecVisitor* visitor) const;

            /// Returns the list of all fields generated for spec path.
            const std::vector<TfToken>& List(const SdfPath& path) const;

            /// Returns a set that enumerates all integer frame values from 0 to the
            /// total number of animation frames specified in the params object.
            const std::set<double>& ListAllTimeSamples() const;

            /// Returns the same set as ListAllTimeSamples if the spec path is for one
            /// of the animated properties. Returns an empty set for all other spec
            /// paths.
            const std::set<double>& ListTimeSamplesForPath(const SdfPath& path) const;

            /// Returns the total number of animation frames if the spec path is for
            /// one of the animated properties. Returns 0 for all other spec paths.
            size_t GetNumTimeSamplesForPath(const SdfPath& path) const;

            /// Sets the upper and lower bound time samples of the value time and
            /// returns true as long as there are any animated frames for this data.
            bool GetBracketingTimeSamples(double time, double* tLower, double* tUpper) const;

            /// Sets the upper and lower bound time samples of the value time and
            /// returns true if the spec path is for one of the animated properties.
            /// Returns false for all other spec paths.
            bool GetBracketingTimeSamplesForPath(const SdfPath& path, double time, double* tLower, double* tUpper) const;

            /// Computes the value for the time sample if the spec path is one of the
            /// animated properties.
            bool QueryTimeSample(const SdfPath& path, double frame, VtValue* value);

            /// <summary>
            /// Notice received when an object changes in the stage
            /// </summary>
            /// <param name="notice"></param>
            void HandleNotice(const UsdNotice::ObjectsChanged& notice);

            void RefreshUsdStage(UsdStagePtr usdStage);

        private:
            // Initializes the cached data from the params object.
            void _InitFromParams();

            // Helper functions for queries about property specs.
            bool _IsAnimatedProperty(const SdfPath& path) const;
            bool _HasPropertyDefaultValue(const SdfPath& path, VtValue* value) const;
            bool _HasTargetPathValue(const SdfPath& path, VtValue* value) const;
            bool _HasPropertyTypeNameValue(const SdfPath& path, VtValue* value) const;
            bool _HasPropertyInterpolation(const SdfPath& path, VtValue* value) const;

            SdfPath _CreateHierarchyFor(const glm::GlmString& hierarchy, const SdfPath& parentPath, GlmMap<GlmString, SdfPath>& existingPaths);
            void _ComputeSkelEntity(SkelEntityData* entityData, double frame);
            void _ComputeSkinMeshEntity(SkinMeshEntityData* entityData, double frame);
            void _DoComputeSkinMeshEntity(SkinMeshEntityData* entityData);
            void _ComputeEntity(EntityData* entityData);
            void _InvalidateEntity(EntityData* entityData);
            void _ComputeBboxData(SkinMeshEntityData* entityData);
            void _ComputeSkinMeshTemplateData(
                glm::Array<std::map<std::pair<int, int>, SkinMeshTemplateData>>& characterTemplateData,
                const glm::crowdio::InputEntityGeoData& inputGeoData,
                const glm::crowdio::OutputEntityGeoData& outputData);
            void _InitSkinMeshData(
                const SdfPath& parentPath,
                SkinMeshEntityData* entityData,
                SkinMeshLodData* lodData,
                glm::PODArray<SkinMeshData*>& meshDataArray,
                const std::map<std::pair<int, int>, SkinMeshTemplateData>& templateDataPerMesh,
                const glm::PODArray<int>& gchaMeshIds,
                const glm::PODArray<int>& meshAssetMaterialIndices);

            bool _QueryEntityAttributes(const EntityData* genericEntityData, const TfToken& nameToken, const double& frame, VtValue* value);
        };

        //-----------------------------------------------------------------------------
        inline const Time& GolaemUSD_DataImpl::UsdWrapper::getCurrentFrame() const
        {
            return _currentFrame;
        }
    } // namespace usdplugin
} // namespace glm
