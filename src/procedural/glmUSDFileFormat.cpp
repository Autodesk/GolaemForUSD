/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "glmUSDFileFormat.h"
#include "glmUSDData.h"

USD_INCLUDES_START
#include <pxr/pxr.h>
#include <pxr/usd/pcp/dynamicFileFormatContext.h>
#include <pxr/usd/usd/usdaFileFormat.h>
USD_INCLUDES_END

#include <glmCoreDefinitions.h>

namespace glm
{
    namespace usdplugin
    {
        using namespace PXR_INTERNAL_NS;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4003)
#endif
        TF_DEFINE_PUBLIC_TOKENS(GolaemUSDFileFormatTokens, GOLAEM_USD_FILE_FORMAT_TOKENS);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        TF_REGISTRY_FUNCTION(TfType)
        {
            // don't forget to use the full type name (with namespaces, i.e. glm::usdplugin::GolaemUSDFileFormat) in
            // the plugInfo.json file
            SDF_DEFINE_FILE_FORMAT(glm::usdplugin::GolaemUSDFileFormat, SdfFileFormat);
        }

        //-----------------------------------------------------------------------------
        GolaemUSDFileFormat::GolaemUSDFileFormat()
            : SdfFileFormat(
                  GolaemUSDFileFormatTokens->Id,
                  GolaemUSDFileFormatTokens->Version,
                  GolaemUSDFileFormatTokens->Target,
                  GolaemUSDFileFormatTokens->Extension)
        {
        }

        //-----------------------------------------------------------------------------
        GolaemUSDFileFormat::~GolaemUSDFileFormat()
        {
        }

        //-----------------------------------------------------------------------------
        SdfAbstractDataRefPtr GolaemUSDFileFormat::InitData(const FileFormatArguments& args) const
        {
            // Create our special procedural abstract data with its parameters extracted
            // from the file format arguments.
            return GolaemUSD_Data::New(GolaemUSD_DataParams::FromArgs(args));
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSDFileFormat::CanRead(const std::string& filePath) const
        {
            GLM_UNREFERENCED(filePath);
            return true;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSDFileFormat::Read(SdfLayer* layer, const std::string& resolvedPath, bool metadataOnly) const
        {
            GLM_UNREFERENCED(resolvedPath);
            GLM_UNREFERENCED(metadataOnly);
            if (!TF_VERIFY(layer))
            {
                return false;
            }

            // Enforce that the layer is read only.
            layer->SetPermissionToSave(false);
            layer->SetPermissionToEdit(false);

            // We don't do anything else when we read the file as the contents aren't
            // used at all in this example. There layer's data has already been
            // initialized from file format arguments.
            return true;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSDFileFormat::WriteToString(
            const SdfLayer& layer,
            std::string* str,
            const std::string& comment) const
        {
            // Write the generated contents in usda text format.
            return SdfFileFormat::FindById(UsdUsdaFileFormatTokens->Id)->WriteToString(layer, str, comment);
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSDFileFormat::WriteToStream(
            const SdfSpecHandle& spec,
            std::ostream& out,
            size_t indent) const
        {
            // Write the generated contents in usd format.
            return SdfFileFormat::FindById(UsdUsdaFileFormatTokens->Id)->WriteToStream(spec, out, indent);
        }

        //-----------------------------------------------------------------------------
        void GolaemUSDFileFormat::ComposeFieldsForFileFormatArguments(
            const std::string& assetPath,
            const PcpDynamicFileFormatContext& context,
            FileFormatArguments* args,
            VtValue* contextDependencyData) const
        {
            GLM_UNREFERENCED(contextDependencyData);
            GolaemUSD_DataParams params;

            // There is one relevant metadata field that should be dictionary valued.
            // Compose this field's value and extract any param values from the
            // resulting dictionary.
            VtValue val;
            if (context.ComposeValue(GolaemUSDFileFormatTokens->Params, &val) && val.IsHolding<VtDictionary>())
            {
                params = GolaemUSD_DataParams::FromDict(val.UncheckedGet<VtDictionary>());
            }
            params.glmProceduralFile = TfToken(assetPath); // use this file for relative paths

            // Convert the entire params object to file format arguments. We always
            // convert all parameters even if they're default as the args are part of
            // the identity of the layer.
            *args = params.ToArgs();
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSDFileFormat::CanFieldChangeAffectFileFormatArguments(
            const TfToken& field,
            const VtValue& oldValue,
            const VtValue& newValue,
            const VtValue& contextDependencyData) const
        {
            GLM_UNREFERENCED(field);
            GLM_UNREFERENCED(contextDependencyData);
            // Theres only one relevant field and its values should hold a dictionary.
            const VtDictionary& oldDict = oldValue.IsHolding<VtDictionary>() ? oldValue.UncheckedGet<VtDictionary>() : VtGetEmptyDictionary();
            const VtDictionary& newDict = newValue.IsHolding<VtDictionary>() ? newValue.UncheckedGet<VtDictionary>() : VtGetEmptyDictionary();

            // The dictionary values for our metadata key are not restricted as to what
            // they may contain so it's possible they may have keys that are completely
            // irrelevant to generating the this file format's parameters. Here we're
            // demonstrating how we can do a more fine grained analysis based on this
            // fact. In some cases this can provide a better experience for users if
            // the extra processing in this function can prevent expensive prim
            // recompositions for changes that don't require it. But keep in mind that
            // there can easily be cases where making this function more expensive can
            // outweigh the benefits of avoiding unnecessary recompositions.

            // Compare relevant values in the old and new dictionaries.
            // If both the old and new dictionaries are empty, there's no change.
            if (oldDict.empty() && newDict.empty())
            {
                return false;
            }

            // Otherwise we iterate through each possible parameter value looking for
            // any one that has a value change between the two dictionaries.
            for (const TfToken& token : GolaemUSD_DataParamsTokens->allTokens)
            {
                auto oldIt = oldDict.find(token);
                auto newIt = newDict.find(token);
                const bool oldValExists = oldIt != oldDict.end();
                const bool newValExists = newIt != newDict.end();

                // If param value exists in one or not the other, we have change.
                if (oldValExists != newValExists)
                {
                    return true;
                }
                // Otherwise if it's both and the value differs, we also have a change.
                if (newValExists && oldIt->second != newIt->second)
                {
                    return true;
                }
            }

            // None of the relevant data params changed between the two dictionaries.
            return false;
        }

    } // namespace usdplugin
} // namespace glm