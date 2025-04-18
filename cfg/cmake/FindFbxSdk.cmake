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
	set(USE_HOUDINI_FBX FALSE)

	if(BUILD_HOUDINI AND NOT "${HOUDINI_VERSION}" STREQUAL "18.5")
		set(USE_HOUDINI_FBX TRUE)
		message( "Using FbxSdk from Houdini ${HOUDINI_VERSION}" )
	else()
		if("${FBXSDK_VERSION}" STREQUAL "")
			set( FBXSDK_VERSION "2020.3.7" )
			message( "Setting FbxSdk version to ${FBXSDK_VERSION} because it was not forced" )
		else()
			message( "Using FbxSdk version ${FBXSDK_VERSION}" )
		endif()
	endif()

	if(USE_HOUDINI_FBX)
		set(FBXSDK_EXTERNALS_PATH "${HOUDINI_EXTERNALS_PATH}")

		if(MSVC)
			set(FBXSDK_LIBFILE_SUBDIR "custom/houdini/dsolib")
		else()
			set(FBXSDK_LIBFILE_SUBDIR "dsolib")
		endif()
	else()
		set(FBXSDK_EXTERNALS_PATH "${GLM_EXTERNALS_HOME}/fbxsdk/fbxsdk-${FBXSDK_VERSION}")
		if(MSVC)
			set(TOOLSETDIR "vs2022")
			if( MSVC_TOOLSET_VERSION EQUAL 141)
				set(TOOLSETDIR "vs2017")
			endif()
			set(FBXSDK_EXTERNALS_PATH "${FBXSDK_EXTERNALS_PATH}/windows/${TOOLSETDIR}")
		else()
			if("${FBXSDK_VERSION}" STREQUAL "2019.0")
				set(FBXSDK_EXTERNALS_PATH "${FBXSDK_EXTERNALS_PATH}/linux/gcc48")
			else()
				set(FBXSDK_EXTERNALS_PATH "${FBXSDK_EXTERNALS_PATH}/linux")
			endif()
		endif()
	endif()

	set( FBXSDK_FOUND OFF )
	if( MSVC )
		if(USE_HOUDINI_FBX)
			set(FBXSDK_LIBFILE_SUBDIR "custom/houdini/dsolib")
			set(FBXSDK_LIBFILE_RELEASE "libfbxsdk.lib")

		else()
			set(FBXSDK_LIBFILE_SUBDIR "lib/x64")
			set(FBXSDK_LIBFILE_RELEASE "/release/libfbxsdk-md.lib")
		endif()
	else()
		if(USE_HOUDINI_FBX)
			set(FBXSDK_LIBFILE_SUBDIR "dsolib")
			set(FBXSDK_LIBFILE_RELEASE "libfbxsdk.so")
		else()
			set(FBXSDK_LIBFILE_RELEASE "libfbxsdk.a")
			if("${FBXSDK_VERSION}" STREQUAL "2019.0")
				set(FBXSDK_LIBFILE_SUBDIR "lib/gcc4/x64/release")
			elseif("${FBXSDK_VERSION}" STREQUAL "2020.3.4")
				set( FBXSDK_LIBFILE_SUBDIR "lib/gcc/x64/release" )
			else()
				set(FBXSDK_LIBFILE_SUBDIR "lib/release")
			endif()
		endif()
	endif()
	set( FBXSDK_REQUESTEDDIR "${FBXSDK_ROOTDIR}" )
	unset( FBXSDK_ROOTDIR CACHE )
	find_path( FBXSDK_ROOTDIR "${FBXSDK_LIBFILE_SUBDIR}/${FBXSDK_LIBFILE_RELEASE}" "${FBXSDK_REQUESTEDDIR}" "${FBXSDK_EXTERNALS_PATH}" "$ENV{FBXSDK_HOME}" NO_DEFAULT_PATH )
	if( FBXSDK_ROOTDIR )
		set( FBXSDK_FOUND ON )

		if(USE_HOUDINI_FBX)
			set(FBXSDK_INCDIR "${FBXSDK_ROOTDIR}/toolkit/include/fbx")
		else()
			set(FBXSDK_INCDIR "${FBXSDK_ROOTDIR}/include")
		endif()

		set(FBXSDK_BINDIR "${FBXSDK_ROOTDIR}/${FBXSDK_LIBFILE_SUBDIR}")
		set(FBXSDK_LIBDIR "${FBXSDK_ROOTDIR}/${FBXSDK_LIBFILE_SUBDIR}")
		if( MSVC )
			if(USE_HOUDINI_FBX)
				set( FBXSDK_LIBS fbxsdk )
				add_library( "fbxsdk" UNKNOWN IMPORTED )
				set_property(TARGET fbxsdk PROPERTY IMPORTED_LOCATION_DEBUG "${FBXSDK_LIBDIR}/${FBXSDK_LIBFILE_RELEASE}")
				set_property(TARGET fbxsdk PROPERTY IMPORTED_LOCATION_RELEASE "${FBXSDK_LIBDIR}/${FBXSDK_LIBFILE_RELEASE}")
			else()
				set(FBXSDK_LIBS libfbxsdk-md libxml2-md zlib-md)
				foreach( lib ${FBXSDK_LIBS} )
					add_library( "${lib}" UNKNOWN IMPORTED )
					set_property( TARGET ${lib} PROPERTY IMPORTED_LOCATION_DEBUG "${FBXSDK_LIBDIR}/debug/${lib}.lib" )
					set_property( TARGET ${lib} PROPERTY IMPORTED_LOCATION_RELEASE "${FBXSDK_LIBDIR}/release/${lib}.lib" )
				endforeach()
			endif()
		else()
			# Need to do it in such a dirty way for linux libraries (even setting CMake property "IMPORTED_SONAME" or "IMPORTED_NO_SONAME" could not make it)
			# Indeed, there is no proper "SONAME" ELF property in FBX SDK libraries, so any plug-in/application depending on FBX SDK keeps a full link path to the FBX SDK libraries
			# Without this ugly trick, the generated plug-ins/applications can only find FBX SDK libraries in the same location where it was found on build host (even if setting LD_LIBRARY_PATH)
			set(FBXSDK_LIBS "${FBXSDK_LIBDIR}/${FBXSDK_LIBFILE_RELEASE}")
		endif()
		unset( FBXSDK_LIBDIR )

		if(NOT MSVC AND NOT USE_HOUDINI_FBX)
			#link uuid lib
			find_library( UUID_PATH "libuuid.so.1" )
			if( UUID_PATH )
				get_filename_component( UUID_DIR "${UUID_PATH}" PATH )
				list( APPEND FBXSDK_BINDIR "${UUID_DIR}" )
				list( APPEND FBXSDK_LIBS "uuid" )
				add_library( "uuid" UNKNOWN IMPORTED )
				set_property( TARGET uuid PROPERTY IMPORTED_LOCATION "${UUID_PATH}" )
				message( "'FbxSdk' will link with uuid found in ${UUID_PATH}" )
			else()
				message( "libUUID not found while searching for dependency 'FbxSdk'" )
				report_message( "ERROR" "libUUID not found while searching for dependency 'FbxSdk'" )
			endif()

			#link libxml2 lib
			find_library( LIBXML2_PATH "libxml2.so.2" )
			if( LIBXML2_PATH )
				get_filename_component( LIBXML2_DIR "${LIBXML2_PATH}" PATH )
				list( APPEND FBXSDK_BINDIR "${LIBXML2_DIR}" )
				list( APPEND FBXSDK_LIBS "libxml2" )
				add_library( "libxml2" UNKNOWN IMPORTED )
				set_property( TARGET libxml2 PROPERTY IMPORTED_LOCATION "${LIBXML2_PATH}" )
				message( "'FbxSdk' will link with libxml2 found in ${LIBXML2_PATH}" )
			else()
				message( "libxml2 not found while searching for dependency 'FbxSdk'" )
				report_message( "ERROR" "libxml2 not found while searching for dependency 'FbxSdk'" )
			endif()

			#link zlib
			find_library( ZLIB_PATH "libz.so.1" )
			if( ZLIB_PATH )
				get_filename_component( ZLIB_DIR "${ZLIB_PATH}" PATH )
				list( APPEND FBXSDK_BINDIR "${ZLIB_DIR}" )
				list( APPEND FBXSDK_LIBS "zlib" )
				add_library( "zlib" UNKNOWN IMPORTED )
				set_property( TARGET zlib PROPERTY IMPORTED_LOCATION "${ZLIB_PATH}" )
				message( "'FbxSdk' will link with zlib found in ${ZLIB_PATH}" )
			else()
				message( "zlib not found while searching for dependency 'FbxSdk'" )
				report_message( "ERROR" "zlib not found while searching for dependency 'FbxSdk'" )
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
