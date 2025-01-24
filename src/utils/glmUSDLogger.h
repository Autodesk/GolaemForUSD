/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include <glmLog.h>
#include <glmSingleton.h>

namespace glm
{
    namespace usdplugin
    {
        class USDLogger : public glm::ILogger
        {
        public:
            virtual ~USDLogger();
            virtual void trace(glm::Log::Module module, glm::Log::Severity severity, const char* msg, const char* file, int line, const char* operation);

        private:
            USDLogger();
            friend class glm::Singleton<USDLogger>;
        };
    } // namespace usdplugin
} // namespace glm