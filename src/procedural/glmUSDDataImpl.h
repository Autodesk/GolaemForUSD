/***************************************************************************
 *                                                                          *
 *  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
 *                                                                          *
 ***************************************************************************/

#pragma once

#include "glmUSD.h"
#include "glmUSDData.h"

#include <glmSimulationCacheFactory.h>
#include <glmSmartPointer.h>
#include <glmMap.h>

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
            struct EntityData;
            struct EntityFrameData : public glm::ReferenceCounter
            {
                typedef SmartPointer<EntityFrameData> SP;

                bool enabled = true; // can vary during simulation (kill, emit)

                GfVec3f pos{0, 0, 0};

                glm::PODArray<int> intShaderAttrValues;
                glm::PODArray<float> floatShaderAttrValues;
                glm::Array<TfToken> stringShaderAttrValues;
                glm::Array<GfVec3f> vectorShaderAttrValues;

                glm::PODArray<float> floatPPAttrValues;
                glm::Array<GfVec3f> vectorPPAttrValues;

                size_t geometryFileIdx = 0;
                TfToken lodName = TfToken("");

                SmartPointer<EntityData> entityData = NULL;
            };

            // cached data for each entity
            struct EntityData : public glm::ReferenceCounter
            {
                typedef SmartPointer<EntityData> SP;

                std::map<TfToken, size_t, TfTokenFastArbitraryLessThan> ppAttrIndexes;
                std::map<TfToken, size_t, TfTokenFastArbitraryLessThan> shaderAttrIndexes;

                SdfPath entityPath;

                bool excluded = false; // excluded by layout - the entity will always be empty
                size_t cfIdx = 0;      // index of the crowd field this entity belongs to
                uint32_t bonePositionOffset = 0;
                glm::Mutex* cachedSimulationLock = NULL;
                glm::Mutex* entityComputeLock = NULL; // do not allow simultaneous computes of the same entity

                glm::crowdio::InputEntityGeoData inputGeoData;
                glm::crowdio::CachedSimulation* cachedSimulation = NULL;

                GfVec3f extent{0, 0, 0};

                glm::GlmMap<double, EntityFrameData::SP> frameDataMap;

                size_t defaultGeometryFileIdx = 0;
                TfToken defaultLodName = TfToken("");

                ~EntityData();
                void initEntityLock();

                template <class FrameDataType>
                SmartPointer<FrameDataType> findFrameData(const double& frame) const;

                template <class FrameDataType>
                SmartPointer<FrameDataType> getFrameData(const double& frame, size_t cachedFramesCount);
            };

            struct SkinMeshTemplateData : public glm::ReferenceCounter
            {
                typedef SmartPointer<SkinMeshTemplateData> SP;

                VtIntArray faceVertexCounts;
                VtIntArray faceVertexIndices;
                glm::Array<VtVec2fArray> uvSets; // stored by polygon vertex
                GlmString meshAlias;
                VtVec3fArray defaultPoints;
                VtVec3fArray defaultNormals;
                VtVec3fArray defaultVelocities;
                // int normalsCount; // not needed, = faceVertexIndices.size();
                SdfPathListOp materialPath;
                int velocitiesIntShaderAttributeIndex = -1; // index of the enableUsdVelocities int attribute if found, -1 otherwise
            };

            struct SkinMeshData : public glm::ReferenceCounter
            {
                typedef SmartPointer<SkinMeshData> SP;

                // these parameters are animated
                VtVec3fArray points;
                VtVec3fArray normals; // stored by polygon vertex
                VtVec3fArray velocities;

                SkinMeshTemplateData::SP templateData = NULL;
            };

            struct SkinMeshLodData : public glm::ReferenceCounter
            {
                typedef SmartPointer<SkinMeshLodData> SP;

                std::map<std::pair<int, int>, SkinMeshData::SP> meshData;
                EntityData::SP entityData = NULL;
                bool enabled = false;
            };

            struct SkinMeshEntityFrameData : public EntityFrameData
            {
                typedef SmartPointer<SkinMeshEntityFrameData> SP;

                glm::Array<SkinMeshLodData::SP> meshLodData;
                bool velocitiesComputed = false;
            };

            struct SkinMeshEntityData : public EntityData
            {
                typedef SmartPointer<SkinMeshEntityData> SP;

                glm::PODArray<int> lodEnabled; // useful when using static lod
                bool computeVelocities = false;
            };

            struct SkelEntityData : public EntityData
            {
                typedef SmartPointer<SkelEntityData> SP;

                SdfReferenceListOp referencedUsdCharacter;
                SdfVariantSelectionMap geoVariants;

                SdfPathListOp animationSourcePath;
                SdfPathListOp skeletonPath;

                bool scalesAnimated = false;
                uint32_t boneSnsOffset = 0;
            };

            struct SkelEntityFrameData : public EntityFrameData
            {
                typedef SmartPointer<SkelEntityFrameData> SP;

                VtQuatfArray rotations;
                VtVec3hArray scales;

                VtVec3fArray translations;
            };

            struct SkinMeshLodMapData
            {
                SkinMeshEntityData::SP entityData;
                size_t lodIndex;
            };

            struct SkinMeshMapData
            {
                SkinMeshEntityData::SP entityData;
                size_t lodIndex;
                int gchaMeshId;
                int meshMaterialIndex;
                SkinMeshTemplateData::SP templateData;
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
            glm::Array<VtTokenArray> _jointsPerChar;
            glm::Array<glm::Array<std::map<std::pair<int, int>, SkinMeshTemplateData::SP>>> _skinMeshTemplateDataPerCharPerGeomFile;

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

            TfHashMap<SdfPath, EntityData::SP, SdfPath::Hash> _entityDataMap;

            TfHashMap<SdfPath, SkinMeshMapData, SdfPath::Hash> _skinMeshDataMap;
            TfHashMap<SdfPath, SkinMeshLodMapData, SdfPath::Hash> _skinMeshLodDataMap;

            TfHashMap<SdfPath, SkelEntityData::SP, SdfPath::Hash> _skelAnimDataMap;

            glm::PODArray<glm::Mutex*> _cachedSimulationLocks;

            glm::Array<PODArray<size_t>> _globalToSpecificShaderAttrIdxPerChar;

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
            SkelEntityFrameData::SP _ComputeSkelEntity(EntityData::SP entityData, double frame);
            SkinMeshEntityFrameData::SP _ComputeSkinMeshEntity(EntityData::SP entityData, double frame);
            void _ComputeEntityVelocities(SkinMeshEntityFrameData::SP currentFrameData, SkinMeshEntityFrameData::SP prevFrameData);
            void _ComputeEntity(EntityFrameData::SP entityFrameData, double frame);
            void _InvalidateEntity(EntityFrameData::SP entityFrameData);
            void _getCharacterExtent(EntityData::SP entityData, GfVec3f& extent) const;
            void _ComputeBboxData(SkinMeshEntityData::SP entityData);
            void _ComputeSkinMeshTemplateData(
                std::map<std::pair<int, int>, SkinMeshTemplateData::SP>& lodTemplateData,
                const glm::crowdio::InputEntityGeoData& inputGeoData,
                const glm::crowdio::OutputEntityGeoData& outputData);
            void _InitSkinMeshData(
                const SdfPath& parentPath,
                SkinMeshEntityData::SP entityData,
                size_t lodIndex,
                const std::map<std::pair<int, int>, SkinMeshTemplateData::SP>& templateDataPerMesh,
                const glm::PODArray<int>& gchaMeshIds,
                const glm::PODArray<int>& meshAssetMaterialIndices);

            bool _QueryEntityAttributes(EntityFrameData::SP entityFrameData, const TfToken& nameToken, VtValue* value);
        };

        //-----------------------------------------------------------------------------
        inline const Time& GolaemUSD_DataImpl::UsdWrapper::getCurrentFrame() const
        {
            return _currentFrame;
        }

        //-----------------------------------------------------------------------------
        template <class FrameDataType>
        SmartPointer<FrameDataType> GolaemUSD_DataImpl::EntityData::findFrameData(const double& frame) const
        {
            SmartPointer<FrameDataType> frameData;
            auto itFrameData = frameDataMap.find(frame);
            if (itFrameData != frameDataMap.end())
            {
                frameData = glm::staticCast<FrameDataType>(itFrameData.getValue());
            }
            return frameData;
        }

        //-----------------------------------------------------------------------------
        template <class FrameDataType>
        SmartPointer<FrameDataType> GolaemUSD_DataImpl::EntityData::getFrameData(const double& frame, size_t cachedFramesCount)
        {
            SmartPointer<FrameDataType> frameData = findFrameData<FrameDataType>(frame);
            if (!frameData)
            {
                frameData = new FrameDataType();

                // remove the furthest frame data if we reached the maximum number of cached frames, then add the new one
                if (frameDataMap.size() >= cachedFramesCount)
                {
                    auto itFurthestFrame = frameDataMap.begin();
                    for (auto it = frameDataMap.begin(); it != frameDataMap.end(); ++it)
                    {
                        if (std::abs(it.getKey() - frame) > std::abs(itFurthestFrame.getKey() - frame))
                        {
                            itFurthestFrame = it;
                        }
                    }
                    frameDataMap.erase(itFurthestFrame);
                }
                frameDataMap[frame] = frameData;
            }
            return frameData;
        }
    } // namespace usdplugin
} // namespace glm
