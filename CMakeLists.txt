cmake_minimum_required(VERSION 3.22)

project(
	Precision
	VERSION 2.0.4
	LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

macro(set_from_environment VARIABLE)
	if(NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif()
endmacro()

set_from_environment(CompiledPluginsPath)
if(NOT DEFINED CompiledPluginsPath)
	message(FATAL_ERROR "CompiledPluginsPath is not set")
endif()

option(COPY_OUTPUT "Copy the output of build operations to the game directory" ON)
option(BUILD_FOR_SKYRIM_SE_1597 "Build dedicated Precision - Weapon Trails Only binary for Skyrim SE 1.5.97" OFF)
option(ENABLE_SKYRIM_SE "Enable support for Skyrim SE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_AE "Enable support for Skyrim AE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_VR "Enable support for Skyrim VR in the dynamic runtime feature." OFF)
if(BUILD_FOR_SKYRIM_SE_1597)
	set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
	set(ENABLE_SKYRIM_AE OFF CACHE BOOL "" FORCE)
	set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
endif()
set(BUILD_TESTS OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

add_subdirectory(src)
include(cmake/packaging.cmake)
