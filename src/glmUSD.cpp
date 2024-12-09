/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "glmUSD.h"
#include "glmUSDLogger.h"
#include "glmUSDPluginProductInformation.h"

#include <glmCore.h>
#include <glmStringOperators.h>
#include <glmCrowdIO.h>
#include <glmMutex.h>
#include <glmScopedLock.h>
#include <glmProductInformation.h>

USD_INCLUDES_START
#include <pxr/pxr.h>
USD_INCLUDES_END

namespace glm
{
    namespace usdplugin
    {
        static glm::Mutex s_initLock;
        static int s_initCount = 0;

        //-----------------------------------------------------------------------------
        int init()
        {
            glm::ScopedLock<glm::Mutex> lock(s_initLock);
            if (s_initCount == 0)
            {
                printf("%s\n", usdplugin::getProductInformation().getCString());

                glm::initCore(); // inits logs
                glm::getLog()->_logSeverity[glm::Log::CROWD] = glm::Log::LOG_WARNING;
                glm::getLog()->_logSeverity[glm::Log::SDK] = glm::Log::LOG_ERROR;

                glm::Singleton<USDLogger>::create();

                glm::crowdio::setupGolaemProduct("GolaemForUsd", "");
                glm::crowdio::init();
            }
            ++s_initCount;
            return s_initCount;
        }

        //-------------------------------------------------------------------------
        void finish()
        {
            glm::ScopedLock<glm::Mutex> lock(s_initLock);
            --s_initCount;
            if (s_initCount == 0)
            {
                glm::crowdio::finish();
                glm::Singleton<USDLogger>::destroy();
                glm::finishCore();
            }
        }
    } // namespace usdplugin
} // namespace glm