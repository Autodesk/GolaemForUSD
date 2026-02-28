cmake_minimum_required(VERSION 3.25)

option (GOLAEMUSD_STANDALONE_BUILD "Standalone build: ON/OFF" ON)

############################################################
# BEGIN Project
############################################################

# Stand Alone
if(GOLAEMUSD_STANDALONE_BUILD)
    ############################################################
	# BEGIN Declare various macros
	############################################################
	macro( list_files FOLDER_NAME FILE_EXTENSIONS )
        set( SKIP_INCLUDE_DIR OFF )
        if( ( ${ARGC} GREATER 2 ) AND ( NOT "${ARGV2}" STREQUAL "" ) )
            set( DIRECTORY_REFERENCE "${ARGV2}/${FOLDER_NAME}" )
            if( ( ${ARGC} GREATER 3 ) AND ( "${ARGV3}" STREQUAL "SKIP_INCLUDE_DIR" ) )
                set( SKIP_INCLUDE_DIR ON )
            endif()
        else()
            set( DIRECTORY_REFERENCE "${CMAKE_CURRENT_SOURCE_DIR}/${FOLDER_NAME}" )
        endif()
        if( EXISTS ${DIRECTORY_REFERENCE} )
            if( NOT SKIP_INCLUDE_DIR )
                include_directories( ${DIRECTORY_REFERENCE} )
            endif()
            if( "${FOLDER_NAME}" STREQUAL "." )
                set( DIRECTORY_FILTER "Source Files" )
                set( DIRECTORY_LABEL "ROOT" )
            else()
                set( DIRECTORY_FILTER "Source Files/${FOLDER_NAME}" )
                string( REGEX REPLACE "/\\." "" DIRECTORY_FILTER ${DIRECTORY_FILTER} )
                string( REGEX REPLACE "\\./" "" DIRECTORY_FILTER ${DIRECTORY_FILTER} )
                string( REGEX REPLACE "/" "\\\\\\\\" DIRECTORY_FILTER ${DIRECTORY_FILTER} )
                set( DIRECTORY_LABEL "${FOLDER_NAME}" )
                string( REGEX REPLACE "/\\." "" DIRECTORY_LABEL ${DIRECTORY_LABEL} )
                string( REGEX REPLACE "\\./" "" DIRECTORY_LABEL ${DIRECTORY_LABEL} )
                string( REGEX REPLACE "/" "_" DIRECTORY_LABEL ${DIRECTORY_LABEL} )
                string( TOUPPER "${DIRECTORY_LABEL}" DIRECTORY_LABEL )
            endif()
            foreach( file_ext ${FILE_EXTENSIONS} )
                string( TOUPPER "${file_ext}" _FILE_EXT )
                unset( LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT} )
                file( GLOB LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT} ${DIRECTORY_REFERENCE}/*.${file_ext} )
                list( APPEND LIST_FILES "${LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT}}" )
                list( APPEND LIST_FILES_${_FILE_EXT} "${LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT}}" )
                source_group( "${DIRECTORY_FILTER}" FILES ${LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT}} )
                # unset( LIST_FILES_${DIRECTORY_LABEL}_${_FILE_EXT} )
            endforeach()
        else()
        endif()
    endmacro()

    macro(glm_setup_python)
        set(PY_COMMAND_FILE "${CMAKE_CURRENT_BINARY_DIR}/pyfile.py")
        if (NOT "${Python_EXECUTABLE}" STREQUAL "")
            message("Python_EXECUTABLE=${Python_EXECUTABLE}")
            if ("${Python_INCLUDE_DIR}" STREQUAL "")
                set(PY_COMMAND_FILE_CONTENTS "")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}import sysconfig\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}import os\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}includeDir = sysconfig.get_path(\"include\")\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}if os.path.isdir(includeDir):\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tprint(includeDir)\n")
                file(WRITE "${PY_COMMAND_FILE}" "${PY_COMMAND_FILE_CONTENTS}")
                execute_process(COMMAND ${Python_EXECUTABLE} ${PY_COMMAND_FILE} OUTPUT_VARIABLE Python_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
            endif()
        endif()
        message("Python_INCLUDE_DIR=${Python_INCLUDE_DIR}")
        if ("${Python_INCLUDE_DIR}" STREQUAL "")
            message( SEND_ERROR "Could not find Python_INCLUDE_DIR from the python interpreter ${Python_EXECUTABLE}")
        endif()

        if (NOT "${Python_EXECUTABLE}" STREQUAL "")
            if ("${Python_LIBRARY}" STREQUAL "")
                set(PY_COMMAND_FILE_CONTENTS "")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}import sysconfig\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}import os\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}import sys\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}libName=\"\"\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}if sys.platform == \"win32\":\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tlibName=\"python{}.lib\".format(sysconfig.get_config_var(\"py_version_nodot\"))\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}else:\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tlibName=sysconfig.get_config_var(\"LDLIBRARY\")\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidates = []\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidate=sysconfig.get_config_var(\"LIBPL\")\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}if dirCandidate:\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tdirCandidates += [dirCandidate]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidate=sysconfig.get_config_var(\"srcdir\")\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}if dirCandidate:\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tdirCandidates += [dirCandidate]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidate=sysconfig.get_config_var(\"LIBDIR\")\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}if dirCandidate:\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tdirCandidates += [dirCandidate]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidates += [sys.exec_prefix]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidates += [os.path.join(sys.exec_prefix, \"lib\")]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidates += [os.path.join(sys.exec_prefix, \"libs\")]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidates += [os.path.join(sys.exec_prefix, os.pardir, \"lib\")]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}dirCandidates += [os.path.join(sys.exec_prefix, os.pardir, \"libs\")]\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}for dirCandidate in dirCandidates:\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tlibCandidate=os.path.join(dirCandidate, libName)\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\tif os.path.isfile(libCandidate):\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\t\tprint(libCandidate)\n")
                set(PY_COMMAND_FILE_CONTENTS "${PY_COMMAND_FILE_CONTENTS}\t\tbreak\n")
                file(WRITE "${PY_COMMAND_FILE}" "${PY_COMMAND_FILE_CONTENTS}")
                execute_process(COMMAND ${Python_EXECUTABLE} ${PY_COMMAND_FILE} OUTPUT_VARIABLE Python_LIBRARY OUTPUT_STRIP_TRAILING_WHITESPACE)
            endif()
        endif()
        message("Python_LIBRARY=${Python_LIBRARY}")
        if ("${Python_LIBRARY}" STREQUAL "")
            message( SEND_ERROR "Could not find Python_LIBRARY from the python interpreter ${Python_EXECUTABLE}")
        endif()
    endmacro()
    ############################################################
    # END Declare various macros
    ############################################################

    project(GolaemUSD)

    if( NOT MSVC )
        # Needed to ensure that RPATH is set ASAP, even before copying installed libraries in multiple locations
        set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
        # set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    endif()

    set( BUILD_ARCHITECTURE_VERSION "x64" )
    if( MSVC )
		set( BUILD_COMPILER_VERSION "vc${MSVC_TOOLSET_VERSION}")
	elseif( CMAKE_COMPILER_IS_GNUCC )
		execute_process( COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION )
		string( REGEX MATCHALL "[0-9]+" GCC_VERSION_NUMBERS ${GCC_VERSION} )
		set( BUILD_COMPILER_VERSION "gcc" )
		foreach( number ${GCC_VERSION_NUMBERS} )
			set( BUILD_COMPILER_VERSION "${BUILD_COMPILER_VERSION}${number}" )
		endforeach()
    endif()

    set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cfg/cmake")
    
    # Add dependencies
    find_package(FbxSdk REQUIRED)
    if(BUILD_USD_HOUDINI)
        find_package(GlmHoudini REQUIRED)
    else()
        find_package(USD REQUIRED)
    endif()

    # Golaem Devkit dependency
    set( GOLAEMDEVKIT_ROOTDIR "Golaem Devkit Dir" CACHE PATH "Golaem Devkit directory" )
    set( GOLAEMDEVKIT_INCDIR "${GOLAEMDEVKIT_ROOTDIR}/include")
    set( GOLAEMDEVKIT_LIBDIR "${GOLAEMDEVKIT_ROOTDIR}/lib")
    if(MSVC)
        list(APPEND GOLAEMDEVKIT_LIBS "${GOLAEMDEVKIT_LIBDIR}/glmCore_${BUILD_COMPILER_VERSION}${BUILD_ARCHITECTURE_VERSION}.lib")
        list(APPEND GOLAEMDEVKIT_LIBS "${GOLAEMDEVKIT_LIBDIR}/glmCrowdIO_${BUILD_COMPILER_VERSION}${BUILD_ARCHITECTURE_VERSION}.lib")
        list(APPEND GOLAEMDEVKIT_LIBS "${GOLAEMDEVKIT_LIBDIR}/glmSDK_${BUILD_COMPILER_VERSION}${BUILD_ARCHITECTURE_VERSION}.lib")
        foreach(physx_bin PxPvdSDK PxFoundation PhysX3 PhysX3Common PhysX3Cooking PhysX3CharacterKinematic PhysX3Gpu)
            list(APPEND GOLAEMDEVKIT_PHYSX_INSTALL_BINS "${GOLAEMDEVKIT_ROOTDIR}/bin/glm_${physx_bin}PROFILE_${BUILD_ARCHITECTURE_VERSION}.dll")
        endforeach()
        foreach(physx_bin APEX_Clothing ApexFramework APEX_Legacy)
            list(APPEND GOLAEMDEVKIT_PHYSX_INSTALL_BINS "${GOLAEMDEVKIT_ROOTDIR}/bin/${physx_bin}PROFILE_${BUILD_ARCHITECTURE_VERSION}_glm.dll")
        endforeach()
    else()
        list(APPEND GOLAEMDEVKIT_LIBS "${GOLAEMDEVKIT_LIBDIR}/libglmCore_${BUILD_COMPILER_VERSION}${BUILD_ARCHITECTURE_VERSION}.so")
        list(APPEND GOLAEMDEVKIT_LIBS "${GOLAEMDEVKIT_LIBDIR}/libglmCrowdIO_${BUILD_COMPILER_VERSION}${BUILD_ARCHITECTURE_VERSION}.so")
        list(APPEND GOLAEMDEVKIT_LIBS "${GOLAEMDEVKIT_LIBDIR}/libglmSDK_${BUILD_COMPILER_VERSION}${BUILD_ARCHITECTURE_VERSION}.so")

        foreach(physx_bin PxPvdSDK PxFoundation PhysX3 PhysX3Common PhysX3Cooking PhysX3CharacterKinematic)
            list(APPEND GOLAEMDEVKIT_PHYSX_INSTALL_BINS "${GOLAEMDEVKIT_ROOTDIR}/lib/lib${physx_bin}PROFILE_${BUILD_ARCHITECTURE_VERSION}.so")
        endforeach()
    endif(MSVC)

    # List Files
    list_files( "src" "h;cpp" )
    list_files( "src/procedural" "h;cpp;json" )
    list_files( "src/utils" "h;cpp" )
    list_files( "src/version" "h;cpp;rc")
    list_files( "src/hdgp" "h;cpp" )

    # Include and Link Dependencies
    add_library( ${PROJECT_NAME} SHARED ${LIST_FILES} )
    target_include_directories(${PROJECT_NAME} PRIVATE ${FBXSDK_INCDIR})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GOLAEMDEVKIT_INCDIR} )
    target_include_directories(${PROJECT_NAME} PRIVATE ${PXR_INCLUDE_DIRS})
    if(MSVC)
        set_target_properties( ${PROJECT_NAME} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG ON )
    endif()
    target_link_libraries( ${PROJECT_NAME} ${FBXSDK_LIBS})
    target_link_libraries(${PROJECT_NAME} ${GOLAEMDEVKIT_LIBS} )

    if(BUILD_USD_HOUDINI)
        # from Houdini toolkit/samples/USD/CMakeLists.txt
        if(MSVC)
            target_link_libraries( ${PROJECT_NAME}
                Houdini # Standard Houdini libraries
                HoudiniThirdParty # Standard Houdini third party libraries
            )
        else()
            # Link against Houdini libraries (including USD)
            if ( DEFINED ENV{HOUDINI_HDK_LINK_GUSD} )
                target_link_libraries( ${PROJECT_NAME}
                    Houdini					# Standard Houdini libraries
                    ${_houdini_hfs_root}/dsolib/libgusd.so
                )
            else ()
                target_link_libraries( ${PROJECT_NAME}
                    Houdini			        # Standard Houdini libraries
                )
            endif ()
        endif()
    else()
        target_link_libraries( ${PROJECT_NAME} hdGp usd usdGeom)
    endif()

    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE "-DNOMINMAX" "/MP" "/nologo" "/wd4251")

        if(MSVC_VERSION GREATER_EQUAL 1920)
            # from https://github.com/PixarAnimationStudios/USD/issues/1279
            target_compile_options(${PROJECT_NAME} PRIVATE "/Zc:inline-")
        endif()
    else()
        # flags needed for some usd includes (that use hash_set)
        target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-deprecated")

        # follow the vfx reference platform: https://vfxplatform.com/
        if( CMAKE_COMPILER_IS_GNUCC )
            if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0) # gcc 5.x 6.x
                target_compile_options(${PROJECT_NAME} PUBLIC "-std=c++14" "-D_GLIBCXX_USE_CXX11_ABI=0")
            elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0) # gcc 7.x 8.x 9.x 10.x
                target_compile_options(${PROJECT_NAME} PUBLIC "-std=c++17" "-D_GLIBCXX_USE_CXX11_ABI=0") 
            else() # gcc 11.x (used versions should be 11.1 at least)
                target_compile_options(${PROJECT_NAME} PUBLIC "-std=c++17" "-D_GLIBCXX_USE_CXX11_ABI=1")
            endif()
        endif()

        set_target_properties( ${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/../../lib:$ORIGIN/../../../lib" )
    endif()


    # project label
    string(REGEX REPLACE "^glm" "USD_" CUSTOM_PROJECT_LABEL "${PROJECT_NAME}" )
    set_target_properties( ${PROJECT_NAME} PROPERTIES PROJECT_LABEL ${CUSTOM_PROJECT_LABEL} )
    set_target_properties( ${PROJECT_NAME} PROPERTIES PREFIX "" )
 
    # Update plugInfo.json
    file( READ "${CMAKE_CURRENT_SOURCE_DIR}/src/procedural/plugInfo.json" PLUGININFO_PATH_CONTENTS )
    string( REPLACE "@GOLAEMUSD_LIBRARY_PATH@" "${PROJECT_NAME}_d${CMAKE_SHARED_LIBRARY_SUFFIX}" PLUGININFO_PATH_CONTENTS_Debug ${PLUGININFO_PATH_CONTENTS} )
    string( REPLACE "@GOLAEMUSD_LIBRARY_PATH@" "${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}" PLUGININFO_PATH_CONTENTS_Release ${PLUGININFO_PATH_CONTENTS} )

    # Install path
    set( GOLAEM_INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/GolaemForUSD" )
    set( GOLAEM_INSTALL_PATH_Debug "${GOLAEM_INSTALL_PATH}DBG" )
    set( GOLAEM_INSTALL_PATH_Release "${GOLAEM_INSTALL_PATH}" )

    foreach( configuration "Debug" "Release" )
        if( MSVC )
            install (TARGETS ${PROJECT_NAME} RUNTIME CONFIGURATIONS ${configuration} DESTINATION "${GOLAEM_INSTALL_PATH_${configuration}}/procedurals/usd" )
        else()
            install (TARGETS ${PROJECT_NAME} CONFIGURATIONS ${configuration} DESTINATION "${GOLAEM_INSTALL_PATH_${configuration}}/procedurals/usd" )
        endif()
        set( PLUGININFO_PATH_${configuration} "${CMAKE_BINARY_DIR}/${configuration}/plugInfo.json" )
        file( WRITE "${PLUGININFO_PATH_${configuration}}" "${PLUGININFO_PATH_CONTENTS_${configuration}}" )
        install( FILES "${PLUGININFO_PATH_${configuration}}" CONFIGURATIONS ${configuration} DESTINATION "${GOLAEM_INSTALL_PATH_${configuration}}/procedurals/usd" )      # install plugInfo file
        foreach(GOLAEMDEVKIT_LIB ${GOLAEMDEVKIT_LIBS})
            if( MSVC )
                install( FILES "${GOLAEMDEVKIT_LIB}" CONFIGURATIONS ${configuration} DESTINATION "${GOLAEM_INSTALL_PATH_${configuration}}/bin" )
            else()
                install( FILES "${GOLAEMDEVKIT_LIB}" CONFIGURATIONS ${configuration} DESTINATION "${GOLAEM_INSTALL_PATH_${configuration}}/lib" )
            endif()
        endforeach()
        foreach(physx_bin ${GOLAEMDEVKIT_PHYSX_INSTALL_BINS})
            if( MSVC )
                install( FILES "${physx_bin}" CONFIGURATIONS ${configuration} DESTINATION "${GOLAEM_INSTALL_PATH_${configuration}}/bin" )
            else()
                install( FILES "${physx_bin}" CONFIGURATIONS ${configuration} DESTINATION "${GOLAEM_INSTALL_PATH_${configuration}}/lib" )
            endif()
        endforeach()
    endforeach()

# Included in Golaem Solution
else()

    project(${TARGET_GOLAEMUSD})

    # List Files
    list_files( "src" "h;cpp" )
    list_files( "src/procedural" "h;cpp;json" )
    list_files( "src/utils" "h;cpp" )
    list_files( "src/version" "h;cpp;rc")
    list_files( "src/hdgp" "h;cpp" )

    # Create a library
    add_library( ${PROJECT_NAME} SHARED ${LIST_FILES} )

    if(MSVC)
        if(DEFINED GLM_EXCLUDE_INVALID_DEBUG_BUILDS) # used by build servers
            set_target_properties( ${PROJECT_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE )
            set( GLM_EXCLUDED_TARGETS_FILE_PATH "${CMAKE_BINARY_DIR}/glmExcludedDebugTargets.txt" )
            file(APPEND "${GLM_EXCLUDED_TARGETS_FILE_PATH}" "${PROJECT_NAME}\n")
        endif()
        set_target_properties( ${PROJECT_NAME} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG ON )
    endif()

    if(BUILD_USD_HOUDINI)
        # from Houdini toolkit/samples/USD/CMakeLists.txt
        if(MSVC)
            set(_houdini_link_libraries_ 
                Houdini # Standard Houdini libraries
                ${_houdini_hfs_root}/custom/houdini/dsolib/libgusd.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_ar.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_arch.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_gf.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_js.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_kind.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_pcp.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_plug.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_sdf.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_tf.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_usd.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_usdGeom.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_usdRi.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_usdShade.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_usdUtils.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_vt.lib
                ${_houdini_hfs_root}/custom/houdini/dsolib/libpxr_work.lib
                Python::Module
            )

            set(_houdini_link_libraries_
                ${_houdini_link_libraries_}
                ${_houdini_hfs_root}/custom/houdini/dsolib/hboost_python${PYTHON_VERSION}${PYTHON_SUBVERSION}-mt-x64.lib)
                
            target_link_libraries( ${PROJECT_NAME} ${_houdini_link_libraries_})
        else()
            # Link against Houdini libraries (including USD)
            if ( DEFINED ENV{HOUDINI_HDK_LINK_GUSD} )
                target_link_libraries( ${PROJECT_NAME}
                    Houdini					# Standard Houdini libraries
                    ${_houdini_hfs_root}/dsolib/libgusd.so
                )
            else ()
                target_link_libraries( ${PROJECT_NAME}
                    Houdini			        # Standard Houdini libraries
                )
            endif ()
        endif()
    elseif(BUILD_USD_KATANA)
        if(KATANA_VERSION VERSION_GREATER_EQUAL "5.0")
            if (MSVC)
                set(KATANA_BOOST_PYTHON_LIB foundryboost_python${PYTHON_VERSION}${PYTHON_SUBVERSION}-${BUILD_COMPILER_VERSION_EXACT}-mt-x64-${KATANA_BOOST_VERSION})
            else()
                set(KATANA_BOOST_PYTHON_LIB foundryboost_python${PYTHON_VERSION}${PYTHON_SUBVERSION})
            endif()
            set(KATANA_USD_LIBS
                fnarch
                fngf
                fnpcp
                fnsdf
                fntf
                fnusd
                fnusdGeom
                fnvt
                tbb
                ${KATANA_BOOST_PYTHON_LIB}
            )

            if (MSVC)
                # Disallow Boost auto-linking.
                add_definitions(-DBOOST_ALL_NO_LIB)
                set(KATANA_USD_LIB_PREFIX "")
                set(KATANA_USD_LIB_EXT ".lib")
            else()
                set(KATANA_USD_LIB_PREFIX "lib")
                set(KATANA_USD_LIB_EXT ".so")
            endif()

            foreach(katana_usd_lib ${KATANA_USD_LIBS})
                target_link_libraries(${PROJECT_NAME} "${KATANA_ROOT}/bin/${KATANA_USD_LIB_PREFIX}${katana_usd_lib}${KATANA_USD_LIB_EXT}")
            endforeach()

            target_link_libraries(${PROJECT_NAME} Python::Module)

            target_include_directories(${PROJECT_NAME} PRIVATE ${KATANA_USD_INCLUDE_DIR})
            target_include_directories(${PROJECT_NAME} PRIVATE ${KATANA_BOOST_INCLUDE_DIR})
            target_include_directories(${PROJECT_NAME} PRIVATE ${KATANA_TBB_INCLUDE_DIR})
        else()
            target_link_libraries(${PROJECT_NAME} 
                arch
                gf
                pcp
                sdf
                tf
                usd
                usdGeom
                vt
            )
        endif()
    else()
        target_link_libraries( ${PROJECT_NAME} usd usdGeom hdGp)
    endif()

    # cannot compile debug - windows or linux
    set( CROWD_INSTALL_SKIP_DEBUG ON )

    string(REGEX REPLACE "^glm" "USD_" CUSTOM_PROJECT_LABEL "${PROJECT_NAME}" )

    crowd_label_project( ${PROJECT_NAME} )
    set_target_properties( ${PROJECT_NAME} PROPERTIES PROJECT_LABEL ${CUSTOM_PROJECT_LABEL} )

    crowd_load_dependencies( ${PROJECT_NAME} )

    if(MSVC)
        if(MSVC_VERSION GREATER_EQUAL 1920)
            # from https://github.com/PixarAnimationStudios/USD/issues/1279
            target_compile_options(${PROJECT_NAME} PRIVATE "/Zc:inline-")
        endif()
    else()
        # flags needed for some usd includes (that use hash_set)
        target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-deprecated")
    endif()

    set_target_rpath( ${PROJECT_NAME} "$ORIGIN/../lib:$ORIGIN/../../lib:$ORIGIN/../../../lib" )
    set_target_prefix( ${PROJECT_NAME} PLUGIN )
    set_target_postfix( ${PROJECT_NAME} PLUGIN )

    set(USD_PROC_INSTALL_DIR "procedurals/usd")

    # name the dll "GolaemUSD" instead of "glmGolaemUSD"
    CROWD_INSTALL_RUNTIME_LIBRARY( ${PROJECT_NAME} "${USD_PROC_INSTALL_DIR}" "GolaemUSD")

    crowd_install_dependencies()

    # Update plugInfo.json
    file( READ "${CMAKE_CURRENT_SOURCE_DIR}/src/procedural/plugInfo.json" PLUGININFO_PATH_CONTENTS )
    # get_target_property(_baseName ${PROJECT_NAME} LOCATION_<CONFIG>)
    string( REPLACE "@GOLAEMUSD_LIBRARY_PATH@" "GolaemUSD_d${CMAKE_SHARED_LIBRARY_SUFFIX}" PLUGININFO_PATH_CONTENTS_Debug ${PLUGININFO_PATH_CONTENTS} )
    string( REPLACE "@GOLAEMUSD_LIBRARY_PATH@" "GolaemUSD${CMAKE_SHARED_LIBRARY_SUFFIX}" PLUGININFO_PATH_CONTENTS_Release ${PLUGININFO_PATH_CONTENTS} )

    foreach( configuration "Debug" "Release" )
        set( PLUGININFO_PATH_${configuration} "${CMAKE_BINARY_DIR}/${configuration}/plugInfo.json" )
        file( WRITE "${PLUGININFO_PATH_${configuration}}" "${PLUGININFO_PATH_CONTENTS_${configuration}}" )
        install( FILES "${PLUGININFO_PATH_${configuration}}" CONFIGURATIONS ${configuration} DESTINATION "${CROWD_INSTALL_PATH_${configuration}}/${USD_PROC_INSTALL_DIR}" )      # install plugInfo file
    endforeach()
    
    unset( CROWD_INSTALL_SKIP_DEBUG )
endif()
