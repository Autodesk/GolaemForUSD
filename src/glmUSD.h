/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

#pragma once

// Disable USD SDK warnings
#ifndef USD_INCLUDES_START
#ifdef _MSC_VER
#define USD_INCLUDES_START   \
    __pragma(warning(push)); \
    __pragma(warning(disable : 4003 4244 4305 4100 4275 4127 4996 4800 4201));
#else
#define USD_INCLUDES_START
#endif
#endif

#ifndef USD_INCLUDES_END
#ifdef _MSC_VER
#define USD_INCLUDES_END \
    __pragma(warning(pop));
#else
#define USD_INCLUDES_END
#endif
#endif


namespace glm
{
    namespace usdplugin
    {
        /// initialize the Golaem USD library
        extern int init();

        /// finish the Golaem USD library
        extern void finish();
    }
} // namespace glm
