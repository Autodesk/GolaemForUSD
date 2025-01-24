/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "glmUSDData.h"
#include "glmUSDDataImpl.h"

USD_INCLUDES_START
#include <pxr/pxr.h>
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
        TF_DEFINE_PUBLIC_TOKENS(GolaemUSD_DataParamsTokens, GOLAEM_USD_DATA_PARAMS_TOKENS);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        // Sets an arbitrary param type value from a string arg.
        //-----------------------------------------------------------------------------
        template <class T>
        static void _SetParamFromArg(T* param, const std::string& arg)
        {
            *param = TfUnstringify<T>(arg);
        }

        // Specialization for TfToken which doesn't have an istream method for
        // TfUnstringify.
        //-----------------------------------------------------------------------------
        template <>
        void _SetParamFromArg<TfToken>(TfToken* param, const std::string& arg)
        {
            *param = TfToken(arg);
        }

        // Specialization for GfVec3f which doesn't have an istream method for
        // TfUnstringify.
        //-----------------------------------------------------------------------------
        template <>
        void _SetParamFromArg<GfVec3f>(GfVec3f* param, const std::string& arg)
        {
            GlmString glmArg = arg;
            glmArg.trim();
            glmArg.trim("()");
            glmArg.replaceAll(',', ' ');
            std::istringstream stream(glmArg.c_str());
            stream >> param->data()[0];
            stream >> param->data()[1];
            stream >> param->data()[2];
        }

        // Helper for setting a parameter value from a VtValue, casting if the value type
        // is not an exact match.
        //-----------------------------------------------------------------------------
        template <class T>
        static void _SetParamFromValue(T* param, const VtValue& dictVal)
        {
            if (dictVal.IsHolding<T>())
            {
                *param = dictVal.UncheckedGet<T>();
            }
            else if (dictVal.CanCast<T>())
            {
                VtValue castVal = VtValue::Cast<T>(dictVal);
                *param = castVal.UncheckedGet<T>();
            }
        }

        /*static*/
        //-----------------------------------------------------------------------------
        GolaemUSD_DataParams GolaemUSD_DataParams::FromArgs(
            const SdfFileFormat::FileFormatArguments& args)
        {
            GolaemUSD_DataParams params;

// For each param in the struct, try to find an arg with the same name
// and convert its string value to a new value for the param. Falls back
// to leaving the param as its default value if the arg isn't there.
#define xx(UNUSED_1, NAME, UNUSED_2)                                                          \
    if (const std::string* argValue = TfMapLookupPtr(args, GolaemUSD_DataParamsTokens->NAME)) \
    {                                                                                         \
        _SetParamFromArg(&params.NAME, *argValue);                                            \
    }
            GOLAEM_USD_DATA_PARAMS_X_FIELDS
#undef xx

            return params;
        }

        /*static*/
        //-----------------------------------------------------------------------------
        GolaemUSD_DataParams GolaemUSD_DataParams::FromDict(const VtDictionary& dict)
        {
            GolaemUSD_DataParams params;

// Same as FromArgs, but values are extracted from a VtDictionary.
#define xx(UNUSED_1, NAME, UNUSED_2)                                                     \
    if (const VtValue* dictVal = TfMapLookupPtr(dict, GolaemUSD_DataParamsTokens->NAME)) \
    {                                                                                    \
        _SetParamFromValue(&params.NAME, *dictVal);                                      \
    }
            GOLAEM_USD_DATA_PARAMS_X_FIELDS
#undef xx
            return params;
        }

        //-----------------------------------------------------------------------------
        SdfFileFormat::FileFormatArguments GolaemUSD_DataParams::ToArgs() const
        {
            SdfFileFormat::FileFormatArguments args;

// Convert each param in the struct to string argument with the same name.
#define xx(UNUSED_1, NAME, UNUSED_2) \
    args[GolaemUSD_DataParamsTokens->NAME] = TfStringify(NAME);
            GOLAEM_USD_DATA_PARAMS_X_FIELDS
#undef xx
            return args;
        }

        /*static*/
        //-----------------------------------------------------------------------------
        GolaemUSD_DataRefPtr GolaemUSD_Data::New(const GolaemUSD_DataParams& params)
        {
            return TfCreateRefPtr(new GolaemUSD_Data(params));
        }

        //-----------------------------------------------------------------------------
        GolaemUSD_Data::GolaemUSD_Data(const GolaemUSD_DataParams& params)
            : _impl(new GolaemUSD_DataImpl(params))
        {
            TfWeakPtr<GolaemUSD_Data> me(this);
            TfNotice::Register(me, &GolaemUSD_Data::_HandleNotice);
            TfNotice::Register(me, &GolaemUSD_Data::_HandleStageNotice);
        }

        //-----------------------------------------------------------------------------
        GolaemUSD_Data::~GolaemUSD_Data()
        {
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::StreamsData() const
        {
            // We say this data object streams data because the implementation generates
            // most of its queries on demand.
            return true;
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::IsEmpty() const
        {
            return !_impl || _impl->IsEmpty();
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::HasSpec(const SdfPath& path) const
        {
            return GetSpecType(path) != SdfSpecTypeUnknown;
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::EraseSpec(const SdfPath& path)
        {
            GLM_UNREFERENCED(path);
            TF_RUNTIME_ERROR("GolaemUSD_Data::EraseSpec() not supported");
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::MoveSpec(const SdfPath& oldPath, const SdfPath& newPath)
        {
            GLM_UNREFERENCED(oldPath);
            GLM_UNREFERENCED(newPath);
            TF_RUNTIME_ERROR("GolaemUSD_Data::MoveSpec() not supported");
        }

        //-----------------------------------------------------------------------------
        SdfSpecType GolaemUSD_Data::GetSpecType(const SdfPath& path) const
        {
            GLM_UNREFERENCED(path);
            return _impl->GetSpecType(path);
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::CreateSpec(const SdfPath& path, SdfSpecType specType)
        {
            GLM_UNREFERENCED(path);
            GLM_UNREFERENCED(specType);
            TF_RUNTIME_ERROR("GolaemUSD_Data::CreateSpec() not supported");
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::_VisitSpecs(SdfAbstractDataSpecVisitor* visitor) const
        {
            _impl->VisitSpecs(*this, visitor);
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::Has(const SdfPath& path, const TfToken& field, SdfAbstractDataValue* value) const
        {
            if (value)
            {
                VtValue val;
                if (_impl->Has(path, field, &val))
                {
                    return value->StoreValue(val);
                }
                return false;
            }
            return _impl->Has(path, field, nullptr);
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::Has(const SdfPath& path, const TfToken& field, VtValue* value) const
        {
            return _impl->Has(path, field, value);
        }

        //-----------------------------------------------------------------------------
        VtValue GolaemUSD_Data::Get(const SdfPath& path, const TfToken& field) const
        {
            VtValue value;
            _impl->Has(path, field, &value);
            return value;
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::Set(const SdfPath& path, const TfToken& field, const VtValue& value)
        {
            GLM_UNREFERENCED(path);
            GLM_UNREFERENCED(field);
            GLM_UNREFERENCED(value);
            TF_RUNTIME_ERROR("GolaemUSD_Data::Set() not supported");
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::Set(const SdfPath& path, const TfToken& field, const SdfAbstractDataConstValue& value)
        {
            GLM_UNREFERENCED(path);
            GLM_UNREFERENCED(field);
            GLM_UNREFERENCED(value);
            TF_RUNTIME_ERROR("GolaemUSD_Data::Set() not supported");
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::Erase(const SdfPath& path, const TfToken& field)
        {
            GLM_UNREFERENCED(path);
            GLM_UNREFERENCED(field);
            TF_RUNTIME_ERROR("GolaemUSD_Data::Erase() not supported");
        }

        //-----------------------------------------------------------------------------
        std::vector<TfToken> GolaemUSD_Data::List(const SdfPath& path) const
        {
            return _impl->List(path);
        }

        //-----------------------------------------------------------------------------
        std::set<double> GolaemUSD_Data::ListAllTimeSamples() const
        {
            return _impl->ListAllTimeSamples();
        }

        //-----------------------------------------------------------------------------
        std::set<double> GolaemUSD_Data::ListTimeSamplesForPath(const SdfPath& path) const
        {
            return _impl->ListTimeSamplesForPath(path);
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::GetBracketingTimeSamples(double time, double* tLower, double* tUpper) const
        {
            return _impl->GetBracketingTimeSamples(time, tLower, tUpper);
        }

        //-----------------------------------------------------------------------------
        size_t GolaemUSD_Data::GetNumTimeSamplesForPath(const SdfPath& path) const
        {
            return _impl->GetNumTimeSamplesForPath(path);
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::GetBracketingTimeSamplesForPath(
            const SdfPath& path, double time, double* tLower, double* tUpper) const
        {
            return _impl->GetBracketingTimeSamplesForPath(path, time, tLower, tUpper);
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::QueryTimeSample(const SdfPath& path, double time, VtValue* value) const
        {
            return _impl->QueryTimeSample(path, time, value);
        }

        //-----------------------------------------------------------------------------
        bool GolaemUSD_Data::QueryTimeSample(
            const SdfPath& path, double time, SdfAbstractDataValue* value) const
        {
            if (value)
            {
                VtValue val;
                if (_impl->QueryTimeSample(path, time, &val))
                {
                    return value->StoreValue(val);
                }
                return false;
            }
            else
            {
                return _impl->QueryTimeSample(path, time, nullptr);
            }
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::SetTimeSample(const SdfPath& path, double time, const VtValue& value)
        {
            GLM_UNREFERENCED(path);
            GLM_UNREFERENCED(time);
            GLM_UNREFERENCED(value);
            TF_RUNTIME_ERROR("GolaemUSD_Data::SetTimeSample() not supported");
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::EraseTimeSample(const SdfPath& path, double time)
        {
            GLM_UNREFERENCED(path);
            GLM_UNREFERENCED(time);
            TF_RUNTIME_ERROR("GolaemUSD_Data::EraseTimeSample() not supported");
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::_HandleNotice(const UsdNotice::ObjectsChanged& notice)
        {
            _impl->HandleNotice(notice);
        }

        //-----------------------------------------------------------------------------
        void GolaemUSD_Data::_HandleStageNotice(const UsdNotice::StageNotice& notice)
        {
            _impl->RefreshUsdStage(notice.GetStage());
        }

    } // namespace usdplugin
} // namespace glm