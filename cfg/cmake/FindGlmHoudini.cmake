#-----------------------------------------------------------------------------------------------------------------------------------------------------
#
#   Copyright (C) Golaem S.A.  All Rights Reserved.
#
#   dev@golaem.com
#
#-----------------------------------------------------------------------------------------------------------------------------------------------------
#
# Description :
#   This scripts is responsible for finding and configuring variables to use 'Houdini' packages (compatible with 'Houdini 17.5.x' packages).
#
# Output :
# - GLMHOUDINI_FOUND = Houdini found on this system ?

if(("${GLMHOUDINI_FOUND}" STREQUAL "") OR (NOT GLMHOUDINI_FOUND))
	set(GLMHOUDINI_FOUND OFF)
	
	find_package(Houdini REQUIRED PATHS "${HOUDINI_DIR}/toolkit/cmake")

	if(${Houdini_FOUND})
		set(GLMHOUDINI_FOUND ON)
		# update python library
		# set(hou_python_path "${HOUDINI_DIR}/python")
		# if(MSVC)
		# 	set(hou_python_path "${hou_python_path}${PYTHON_VERSION}${PYTHON_SUBVERSION}")
		# 	set(Python_EXECUTABLE "${hou_python_path}/python")
		# else()
		# 	set(Python_EXECUTABLE "${hou_python_path}/bin/python")
		# endif()
		# glm_setup_python()
		# find_package(Python COMPONENTS Development REQUIRED)
	endif()
	
	mark_as_advanced(GLMHOUDINI_FOUND)

endif()
