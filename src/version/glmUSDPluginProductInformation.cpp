/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#include "glmUSDPluginProductInformation.h"
#include "glmUSDPluginVersion.h"

#include <glmProductInformation.h>
#include <glmCrowdIO.h>

namespace glm
{
    namespace usdplugin
    {
        //-------------------------------------------------------------------------
        const ProductInformation& getProductInformation()
        {
            unsigned int glmAPIVersion[4];
            crowdio::getGolaemAPIVersion(glmAPIVersion);
            static ProductInformation glmProductInformation(
                GLM_USDPLUGIN_NAME,
                GLM_USDPLUGIN_DESCRIPTION,
                glmAPIVersion[crowdio::GolaemVersion::MAIN_VERSION],
                glmAPIVersion[crowdio::GolaemVersion::MAJOR_VERSION],
                glmAPIVersion[crowdio::GolaemVersion::MINOR_VERSION],
                glmAPIVersion[crowdio::GolaemVersion::PATCH_VERSION],
                crowdio::getGolaemReleaseLabel().c_str(),
                crowdio::getGolaemReleaseDate().c_str());
            return glmProductInformation;
        }
    } // namespace usdplugin
} // namespace glm
