#-----------------------------------------------------------------------------------------------------------------------------------------------------
#
#   Copyright (C) Golaem S.A.  All Rights Reserved.
#
#   dev@golaem.com
#
#-----------------------------------------------------------------------------------------------------------------------------------------------------
#
# Description :
#   This scripts is responsible for finding and configuring variables to use 'FbxSdk' package.
#
# Output :
# - FBXSDK_FOUND = FbxSdk found on this system ?
# - FBXSDK_ROOTDIR = FbxSdk root directory
#-  FBXSDK_INCDIR = FbxSdk headers directory
#-  FBXSDK_BINDIR = FbxSdk binaries directory
# - FBXSDK_BINS = FbxSdk binaries
# - FBXSDK_LIBS = FbxSdk libraries
# - FBXSDK_DEFINITIONS = FbxSdk compiler definitions for all configurations
#
# Warning :
#   Calling this script first requires including macros definitions designed for Golaem projects (include file "GolaemMacros.cmake")

if( ( "${FBXSDK_FOUND}" STREQUAL "" ) OR ( NOT FBXSDK_FOUND ) )

	set( FBXSDK_VERSION "2019.0" )

	set(USE_HOUDINI_FBX FALSE)

	if(BUILD_USD_HOUDINI)
		set(USE_HOUDINI_FBX TRUE)
	endif()

	if(USE_HOUDINI_FBX)
		set(FBXSDK_DIR "${HOUDINI_DIR}")
	else()
		set(FBXSDK_DIR "${GLM_EXTERNALS_HOME}/fbxsdk/fbxsdk-${FBXSDK_VERSION}")
	endif()
	set( FBXSDK_FOUND OFF )
	if( MSVC )
		if(USE_HOUDINI_FBX)
			set(FBXSDK_LIBFILE_SUBDIR "custom/houdini/dsolib")
			set(FBXSDK_LIBFILE_RELEASE "libfbxsdk.lib")

		else()
			set(FBXSDK_LIBFILE_SUBDIR "lib")

			if(BUILD_COMPILER_VERSION STREQUAL "vc140")
				set(FBXSDK_LIBFILE_SUBDIR "${FBXSDK_LIBFILE_SUBDIR}/vs2015")
			elseif(BUILD_COMPILER_VERSION STREQUAL "vc120")
				set(FBXSDK_LIBFILE_SUBDIR "${FBXSDK_LIBFILE_SUBDIR}/vs2013")
			elseif(BUILD_COMPILER_VERSION STREQUAL "vc110")
				set(FBXSDK_LIBFILE_SUBDIR "${FBXSDK_LIBFILE_SUBDIR}/vs2012")
			elseif(BUILD_COMPILER_VERSION STREQUAL "vc100")
				set(FBXSDK_LIBFILE_SUBDIR "${FBXSDK_LIBFILE_SUBDIR}/vs2010")
			elseif(BUILD_COMPILER_VERSION STREQUAL "vc90")
				set(FBXSDK_LIBFILE_SUBDIR "${FBXSDK_LIBFILE_SUBDIR}/vs2008")
			endif()

			if(BUILD_ARCHITECTURE_VERSION STREQUAL "x64")
				set(FBXSDK_LIBFILE_SUBDIR "${FBXSDK_LIBFILE_SUBDIR}/x64")
			else()
				set(FBXSDK_LIBFILE_SUBDIR "${FBXSDK_LIBFILE_SUBDIR}/x86")
			endif()

			set(FBXSDK_LIBFILE_RELEASE "/release/libfbxsdk-md.lib")
		endif()
	else()
		if(USE_HOUDINI_FBX)
			set(FBXSDK_LIBFILE_SUBDIR "dsolib")
			set(FBXSDK_LIBFILE_RELEASE "libfbxsdk.so")
		else()
			set(FBXSDK_LIBFILE_SUBDIR "lib/gcc4/x64/release")
			set(FBXSDK_LIBFILE_RELEASE "libfbxsdk.a")
		endif()
	endif()
	set( FBXSDK_REQUESTEDDIR "${FBXSDK_ROOTDIR}" )
	unset( FBXSDK_ROOTDIR CACHE )
	find_path( FBXSDK_ROOTDIR "${FBXSDK_LIBFILE_SUBDIR}/${FBXSDK_LIBFILE_RELEASE}" "${FBXSDK_REQUESTEDDIR}" "${FBXSDK_DIR}" "$ENV{FBXSDK_HOME}" NO_DEFAULT_PATH )
	if( FBXSDK_ROOTDIR )
		set( FBXSDK_FOUND ON )

		if(USE_HOUDINI_FBX)
			set(FBXSDK_INCDIR "${FBXSDK_ROOTDIR}/toolkit/include/fbx")
		else()
			set(FBXSDK_INCDIR "${FBXSDK_ROOTDIR}/include")
		endif()

		set(FBXSDK_BINDIR "${FBXSDK_ROOTDIR}/${FBXSDK_LIBFILE_SUBDIR}")
		set(FBXSDK_LIBDIR "${FBXSDK_ROOTDIR}/${FBXSDK_LIBFILE_SUBDIR}")
		add_library( "fbxsdk" UNKNOWN IMPORTED )
		if( MSVC )
			set( FBXSDK_LIBS fbxsdk )

			if(USE_HOUDINI_FBX)
				set_property(TARGET fbxsdk PROPERTY IMPORTED_LOCATION_DEBUG "${FBXSDK_LIBDIR}/${FBXSDK_LIBFILE_RELEASE}")
				set_property(TARGET fbxsdk PROPERTY IMPORTED_LOCATION_RELEASE "${FBXSDK_LIBDIR}/${FBXSDK_LIBFILE_RELEASE}")
			else()
				set_property(TARGET fbxsdk PROPERTY IMPORTED_LOCATION_DEBUG "${FBXSDK_LIBDIR}/debug/libfbxsdk-md.lib")
				set_property(TARGET fbxsdk PROPERTY IMPORTED_LOCATION_RELEASE "${FBXSDK_LIBDIR}/release/libfbxsdk-md.lib")
			endif()
		else()
			# Need to do it in such a dirty way for linux libraries (even setting CMake property "IMPORTED_SONAME" or "IMPORTED_NO_SONAME" could not make it)
			# Indeed, there is no proper "SONAME" ELF property in FBX SDK libraries, so any plug-in/application depending on FBX SDK keeps a full link path to the FBX SDK libraries
			# Without this ugly trick, the generated plug-ins/applications can only find FBX SDK libraries in the same location where it was found on build host (even if setting LD_LIBRARY_PATH)
			set(FBXSDK_LIBS "${FBXSDK_LIBDIR}/${FBXSDK_LIBFILE_RELEASE}")
		endif()
		unset( FBXSDK_LIBDIR )

		if(NOT MSVC AND NOT USE_HOUDINI_FBX)
			find_library( UUID_PATH "libuuid.so.1" )
			if( UUID_PATH )
				get_filename_component( UUID_DIR "${UUID_PATH}" PATH )
				list( APPEND FBXSDK_BINDIR "${UUID_DIR}" )
				list( APPEND FBXSDK_LIBS "uuid" )
				add_library( "uuid" UNKNOWN IMPORTED )
				set_property( TARGET uuid PROPERTY IMPORTED_LOCATION "${UUID_PATH}" )
			else()
				report_message( "ERROR" "LibUUID not found while searching for dependency 'FbxSdk'" )
			endif()
		endif()

		if(USE_HOUDINI_FBX)
			set(FBXSDK_DEFINITIONS "-DFBXSDK_SHARED")
		endif()
	else()
		set( FBXSDK_FOUND OFF )
	endif()

	if(NOT MSVC AND NOT USE_HOUDINI_FBX)
		set( FBXSDK_BINS "libuuid.so.1" )
	endif()

	mark_as_advanced(FBXSDK_FOUND FBXSDK_INCDIR FBXSDK_BINDIR FBXSDK_BINS FBXSDK_LIBS FBXSDK_DEFINITIONS)

endif()
