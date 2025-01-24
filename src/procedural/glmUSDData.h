/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include "glmUSD.h"

USD_INCLUDES_START
#include <pxr/pxr.h>
#include <pxr/usd/sdf/abstractData.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/usd/notice.h>
USD_INCLUDES_END

namespace glm
{
    namespace usdplugin
    {
        class GolaemUSD_DataImpl;
        using namespace PXR_INTERNAL_NS;

        TF_DECLARE_WEAK_AND_REF_PTRS(GolaemUSD_Data);

        // Macro input defining the parameter fields in the data's params struct.
        // It's easier to define the fields in the params structure via macro so that
        // we can easily add or remove parameters without having to update all the
        // functions for converting between file format arguments and dictionary values.
        // xx(TYPE, NAME, DEFAULT_VALUE)
        // clang-format off
#define GOLAEM_USD_DATA_PARAMS_X_FIELDS             \
    xx(TfToken, glmCacheLibFile, "")                \
    xx(TfToken, glmCacheLibItem, "")                \
    xx(TfToken, glmCrowdFields, "")	                \
    xx(TfToken, glmCacheName, "")                   \
    xx(TfToken, glmCacheDir, "")                    \
    xx(TfToken, glmCharacterFiles, "")              \
    xx(TfToken, glmUsdCharacterFiles, "")           \
    xx(TfToken, glmEntityIds, "*")                  \
    xx(bool, glmEnableLayout, true)                 \
    xx(TfToken, glmLayoutFiles, "")                 \
	xx(TfToken, glmTerrainFile, "")                 \
    xx(float, glmRenderPercent, 100.f)              \
    xx(short, glmDisplayMode, 2)                    \
    xx(short, glmGeometryTag, 0)                    \
    xx(TfToken, glmDirmap, "")                      \
    xx(TfToken, glmMaterialPath, "/Root/Materials") \
    xx(short, glmMaterialAssignMode, 0)             \
    xx(TfToken, glmAttributeNamespace, "")          \
    xx(short, glmLodMode, 0)                        \
    xx(GfVec3f, glmCameraPos, 0)                    \
    xx(TfToken, glmProceduralFile, "")
        // clang-format on

        // A token of the same name must be defined for each parameter in the macro
        // above so we can access these parameter values in file format arguments and
        // VtDictionary values.
        // clang-format off
#define GOLAEM_USD_DATA_PARAMS_TOKENS   \
    (glmCacheLibFile)                   \
    (glmCacheLibItem)                   \
    (glmCrowdFields)                    \
    (glmCacheName)                      \
    (glmCacheDir)                       \
    (glmCharacterFiles)                 \
    (glmUsdCharacterFiles)              \
    (glmEntityIds)                      \
    (glmEnableLayout)                   \
    (glmLayoutFiles)                    \
	(glmTerrainFile)                    \
    (glmRenderPercent)                  \
    (glmDisplayMode)                    \
    (glmGeometryTag)                    \
    (glmDirmap)                         \
    (glmMaterialPath)                   \
    (glmMaterialAssignMode)             \
    (glmAttributeNamespace)             \
    (glmLodMode)                        \
    (glmCameraPos)                      \
    (glmProceduralFile)
        // clang-format on

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4003)
#endif
        TF_DECLARE_PUBLIC_TOKENS(GolaemUSD_DataParamsTokens, GOLAEM_USD_DATA_PARAMS_TOKENS);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        // The parameter structure used as the input values to generate the data's
        // specs and fields procedurally. This is converted to and from file format
        // arguments.
        struct GolaemUSD_DataParams
        {
            // Define each parameter declared in the param field macro.
            // clang-format off
#define xx(TYPE, NAME, DEFAULT) TYPE NAME{DEFAULT};
            // clang-format on
            GOLAEM_USD_DATA_PARAMS_X_FIELDS
#undef xx

            // Creates a new params structure from the given file format arguments.
            static GolaemUSD_DataParams FromArgs(const SdfFileFormat::FileFormatArguments& args);

            // Creates a new params structure from a VtDictionary
            static GolaemUSD_DataParams FromDict(const VtDictionary& dict);

            // Converts this params structure into the file format arguments that could
            // be used to recreate these parameters.
            SdfFileFormat::FileFormatArguments ToArgs() const;
        };

        class GolaemUSD_Data : public SdfAbstractData
        {
        private:
            // Pointer to the actual implementation
            std::unique_ptr<GolaemUSD_DataImpl> _impl;

        public:
            /// Factory New. We always create this data with an explicit params object.
            static GolaemUSD_DataRefPtr New(const GolaemUSD_DataParams& params);
            virtual ~GolaemUSD_Data();

            /// SdfAbstractData overrides
            bool StreamsData() const override;

            bool IsEmpty() const override;

            void CreateSpec(const SdfPath& path, SdfSpecType specType) override;

            bool HasSpec(const SdfPath& path) const override;
            void EraseSpec(const SdfPath& path) override;
            void MoveSpec(const SdfPath& oldPath, const SdfPath& newPath) override;
            SdfSpecType GetSpecType(const SdfPath& path) const override;

            bool Has(const SdfPath& path, const TfToken& fieldName, SdfAbstractDataValue* value) const override;
            bool Has(const SdfPath& path, const TfToken& fieldName, VtValue* value = NULL) const override;
            VtValue Get(const SdfPath& path, const TfToken& fieldName) const override;
            void Set(const SdfPath& path, const TfToken& fieldName, const VtValue& value) override;
            void Set(const SdfPath& path, const TfToken& fieldName, const SdfAbstractDataConstValue& value) override;
            void Erase(const SdfPath& path, const TfToken& fieldName) override;
            std::vector<TfToken> List(const SdfPath& path) const override;

            std::set<double> ListAllTimeSamples() const override;

            std::set<double> ListTimeSamplesForPath(const SdfPath& path) const override;

            bool GetBracketingTimeSamples(double time, double* tLower, double* tUpper) const override;

            size_t GetNumTimeSamplesForPath(const SdfPath& path) const override;

            bool GetBracketingTimeSamplesForPath(
                const SdfPath& path, double time, double* tLower, double* tUpper) const override;

            bool QueryTimeSample(
                const SdfPath& path, double time, SdfAbstractDataValue* optionalValue) const override;
            bool QueryTimeSample(
                const SdfPath& path, double time, VtValue* value) const override;

            void SetTimeSample(
                const SdfPath& path, double time, const VtValue& value) override;

            void EraseTimeSample(const SdfPath& path, double time) override;

        protected:
            // SdfAbstractData overrides
            void _VisitSpecs(SdfAbstractDataSpecVisitor* visitor) const override;

            void _HandleNotice(const UsdNotice::ObjectsChanged& notice);
            void _HandleStageNotice(const UsdNotice::StageNotice& notice);

        private:
            // Private constructor for factory New
            GolaemUSD_Data(const GolaemUSD_DataParams& params);
        };
    } // namespace usdplugin
} // namespace glm
