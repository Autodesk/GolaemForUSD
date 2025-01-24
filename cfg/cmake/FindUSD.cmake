#-----------------------------------------------------------------------------------------------------------------------------------------------------
#
#   Copyright (C) Golaem S.A.  All Rights Reserved.
#
#   dev@golaem.com
#
#-----------------------------------------------------------------------------------------------------------------------------------------------------
#
# Description :
#   This scripts is responsible for finding and configuring variables to use 'USD' packages (compatible with 'USD 19.x, 20.x' packages).
#
# Output :
# - USD_FOUND = USD found on this system ?
# - USD_ROOTDIR = USD root directory

if( ( "${USD_FOUND}" STREQUAL "" ) OR ( NOT USD_FOUND ) )

	if (NOT USD_VERSION)
		set (USD_VERSION "0.23.08-glm")
	endif()

	set(USD_OS_DIR "")
	if( MSVC )
		set(USD_OS_DIR "windows")
	else()
		if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0)
			set(USD_OS_DIR "linux")
		else() #GCC 11+ -> use new ABI
			set(USD_OS_DIR "linux_newABI" )
		endif()	
	endif()
	set(USD_ROOTDIR "${GLM_EXTERNALS_HOME}/USD/${USD_VERSION}/${USD_OS_DIR}" )

	set(USD_FOUND OFF)
	include("${USD_ROOTDIR}/pxrConfig.cmake")
	if( PXR_LIBRARIES)
		set(USD_FOUND ON)
		# add TBB bins
		if(MSVC)
			file(GLOB USD_TBB_BINS "${USD_ROOTDIR}/bin/tbb*.dll")
		else()
			file(GLOB USD_TBB_BINS "${USD_ROOTDIR}/lib/libtbb*.so*")
		endif()
	endif()

	mark_as_advanced(USD_FOUND)

endif()
