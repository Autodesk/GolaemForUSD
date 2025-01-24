/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include "glmUSD.h"

USD_INCLUDES_START
#include <pxr/pxr.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/pcp/dynamicFileFormatInterface.h>
#include <pxr/base/tf/staticTokens.h>
USD_INCLUDES_END

namespace glm
{
    namespace usdplugin
    {
        using namespace PXR_INTERNAL_NS;

#define GOLAEM_USD_FILE_FORMAT_TOKENS \
    ((Id, "glmUsdFormat"))((Version, "1.0"))((Target, "usd"))((Extension, "glmusd"))((Params, "GolaemUSD_Params"))

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4003)
#endif
        TF_DECLARE_PUBLIC_TOKENS(GolaemUSDFileFormatTokens, GOLAEM_USD_FILE_FORMAT_TOKENS);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        TF_DECLARE_WEAK_AND_REF_PTRS(GolaemUSDFileFormat);

        class GolaemUSDFileFormat : public SdfFileFormat, public PcpDynamicFileFormatInterface
        {
        public:
            /// Override this function from SdfFileFormat to provide our own procedural
            /// SdfAbstractData class.
            SdfAbstractDataRefPtr InitData(
                const FileFormatArguments& args) const override;

            /// Must implement SdfFileFormat's CanRead function. Returns true for all
            /// files as the contents of the file aren't used.
            bool CanRead(const std::string& filePath) const override;

            /// Must implement SdfFileFormat's Read function, though this implemenation
            /// doesn't do anything. There is nothing from the file that needs to be
            /// read as data will have already been initialized from file format
            /// arguments.
            bool Read(SdfLayer* layer,
                      const std::string& resolvedPath,
                      bool metadataOnly) const override;

            /// We override WriteToString and WriteToStream methods so
            /// SdfLayer::ExportToString() etc, work. Writing this layer will write out
            /// the generated layer contents.
            /// We do NOT implement WriteToFile as it doesn't make sense to write to
            /// files of this format when the contents are completely generated from the
            /// file format arguments.
            bool WriteToString(const SdfLayer& layer,
                               std::string* str,
                               const std::string& comment = std::string()) const override;
            bool WriteToStream(const SdfSpecHandle& spec,
                               std::ostream& out,
                               size_t indent) const override;

            /// A required PcpDynamicFileFormatInterface override for generating
            /// the file format arguments in context.
            void ComposeFieldsForFileFormatArguments(
                const std::string& assetPath,
                const PcpDynamicFileFormatContext& context,
                FileFormatArguments* args,
                VtValue* contextDependencyData) const override;

            /// A required PcpDynamicFileFormatInterface override for processing whether
            /// a field change may affect the file format arguments within a given
            /// context.
            bool CanFieldChangeAffectFileFormatArguments(
                const TfToken& field,
                const VtValue& oldValue,
                const VtValue& newValue,
                const VtValue& contextDependencyData) const override;

        protected:
            friend class Sdf_FileFormatFactory<GolaemUSDFileFormat>;

            virtual ~GolaemUSDFileFormat();
            GolaemUSDFileFormat();
        };
    } // namespace usdplugin
} // namespace glm
