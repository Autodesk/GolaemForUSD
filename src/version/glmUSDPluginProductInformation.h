/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

#include "glmUSDPluginAPI.h"

namespace glm
{
    class ProductInformation;

    namespace usdplugin
    {
        /** 
		* \brief Retrieve the product informations of Golaem Crowd Renderman Plugin.
		*
		* \ingroup Crowd Render
		*/
        extern GLM_USDPLUGIN_API const ProductInformation& getProductInformation();
    } // namespace usdplugin
} // namespace glm
