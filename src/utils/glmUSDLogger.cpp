/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "glmUSDLogger.h"
#include "glmUSD.h"

USD_INCLUDES_START

#include <pxr/pxr.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/errorMark.h>

USD_INCLUDES_END

namespace glm
{
    namespace usdplugin
    {
        using namespace PXR_INTERNAL_NS;

        struct GolaemErrorCodes
        {
            enum Value
            {
                GOLAEM_ERROR,
                GOLAEM_WARNING,
                GOLAEM_INFO,
                GOLAEM_DEBUG,
                GOLAEM_SDK_ERROR,
                GOLAEM_SDK_WARNING,
                GOLAEM_SDK_INFO,
                GOLAEM_SDK_DEBUG,
                END,
            };
        };
        

        TF_REGISTRY_FUNCTION(TfEnum)
        {
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_ERROR, "[Golaem::ERROR]");
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_WARNING, "[Golaem::WARNING]");
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_INFO, "[Golaem::INFO]");
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_DEBUG, "[Golaem::DEBUG]");
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_SDK_ERROR, "[GolaemSDK::ERROR]");
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_SDK_WARNING, "[GolaemSDK::WARNING]");
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_SDK_INFO, "[GolaemSDK::INFO]");
            TF_ADD_ENUM_NAME(GolaemErrorCodes::GOLAEM_SDK_DEBUG, "[GolaemSDK::DEBUG]");
        }

        //-----------------------------------------------------------------------------
        USDLogger::USDLogger()
        {
        }

        //-----------------------------------------------------------------------------
        USDLogger::~USDLogger()
        {
        }

        //-----------------------------------------------------------------------------
        void USDLogger::trace(glm::Log::Module module, glm::Log::Severity severity, const char* msg, const char*, int, const char*)
        {
            glm::GlmString glmMessage = msg;
            glmMessage += "\n";
            GolaemErrorCodes::Value errorCodeBase = GolaemErrorCodes::END;
            if (module == Log::CROWD)
            {
                errorCodeBase = GolaemErrorCodes::GOLAEM_ERROR;
            }
            else if (module == Log::SDK)
            {
                errorCodeBase = GolaemErrorCodes::GOLAEM_SDK_ERROR;
            }
            GolaemErrorCodes::Value errorCode = (GolaemErrorCodes::Value)(errorCodeBase + severity);
            TF_STATUS(errorCode, glmMessage.c_str());
        }
    } // namespace usdplugin
} // namespace glm
