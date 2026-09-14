cmake_minimum_required(VERSION 3.25.1)
project(steam-publishing-tool VERSION 1.0.0 LANGUAGES CXX RC)

# App version: project(VERSION) unless CI passes -DSPT_VERSION=<tag>. Plain variable, a cache entry would go stale after a bump.
if(NOT DEFINED SPT_VERSION OR SPT_VERSION STREQUAL "")
	set(SPT_VERSION "${PROJECT_VERSION}")
endif()
if(NOT SPT_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
	message(FATAL_ERROR "SPT_VERSION must look like MAJOR.MINOR.PATCH, got '${SPT_VERSION}'")
endif()
set(SPT_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(SPT_VERSION_MINOR "${CMAKE_MATCH_2}")
set(SPT_VERSION_PATCH "${CMAKE_MATCH_3}")
set(SPT_GITHUB_REPOSITORY "sazonische/steam-publishing-tool" CACHE STRING "owner/name of the GitHub repository used for the update check")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")
set(STEAMWORKS_DIR "${THIRD_PARTY_DIR}/steamworks")
set(STEAM_API_DLL_PATH "${STEAMWORKS_DIR}/redistributable_bin/win64/steam_api64.dll")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network Svg)
find_package(glaze CONFIG REQUIRED)
# BLAKE3: CS2 verifies workshop VPK chunks with BLAKE3-128, not MD5 (see vpk_writer.cpp).
find_package(blake3 CONFIG REQUIRED)

if(QT6_IS_SHARED_LIBS_BUILD)
	set(SPT_STATIC_BUILD OFF)
	message(STATUS "Qt: shared build -> deploying Qt runtime next to the executable")
else()
	set(SPT_STATIC_BUILD ON)
	message(STATUS "Qt: static build -> single executable")
endif()

# Steamworks: official SDK 1.65. Valve ships only a DLL, no static library, so the DLL is
# embedded into the exe as a resource and imported through /DELAYLOAD (see steam_api_library.cpp).
add_library(steamworks SHARED IMPORTED)
set_target_properties(steamworks PROPERTIES
	IMPORTED_IMPLIB "${STEAMWORKS_DIR}/lib/win64/steam_api64.lib"
	IMPORTED_LOCATION "${STEAM_API_DLL_PATH}"
	INTERFACE_INCLUDE_DIRECTORIES "${STEAMWORKS_DIR}/public"
)

file(TO_NATIVE_PATH "${STEAM_API_DLL_PATH}" STEAM_API_DLL_NATIVE_PATH)
string(REPLACE "\\" "\\\\" STEAM_API_DLL_RC_PATH "${STEAM_API_DLL_NATIVE_PATH}")
configure_file(res/steam_api_resource.rc.in "${CMAKE_BINARY_DIR}/steam_api_resource.rc" @ONLY)

set_source_files_properties("${CMAKE_BINARY_DIR}/steam_api_resource.rc" PROPERTIES
	OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/app.ico;${STEAM_API_DLL_PATH}"
)

file(GLOB_RECURSE
	CODE_SOURCES
	CONFIGURE_DEPENDS
	src/*.cpp
	src/*.h
	src/*.hpp
)

qt_add_executable(${PROJECT_NAME}
	${CODE_SOURCES}
	res/resources.qrc
	res/app.manifest
	"${CMAKE_BINARY_DIR}/steam_api_resource.rc"
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
	Qt6::Core
	Qt6::Gui
	Qt6::Widgets
	Qt6::Network
	Qt6::Svg
	glaze::glaze
	BLAKE3::blake3
	steamworks
	delayimp
)
target_link_options(${PROJECT_NAME} PRIVATE "/DELAYLOAD:steam_api64.dll")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

set_target_properties(${PROJECT_NAME} PROPERTIES
	WIN32_EXECUTABLE ON
	OUTPUT_NAME "SteamPublishingTool"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	UNICODE
	_UNICODE
	NOMINMAX
	WIN32_LEAN_AND_MEAN
	QT_NO_KEYWORDS
	APP_VERSION="${SPT_VERSION}"
	APP_GITHUB_REPOSITORY="${SPT_GITHUB_REPOSITORY}"
	SPT_STATIC_BUILD=$<IF:$<BOOL:${SPT_STATIC_BUILD}>,1,0>
)

if(MSVC)
	# Warnings from Steamworks and Qt are silenced through /external — our own code builds with /W4.
	target_compile_options(${PROJECT_NAME} PRIVATE
		/utf-8
		/W4
		/permissive-
		/Zc:__cplusplus
		/Zc:preprocessor
		/EHsc
		/external:W0
		/external:anglebrackets
	)
	# The CRT must match the vcpkg triplet: the static triplet is built with /MT, the regular one with /MD.
	if(VCPKG_TARGET_TRIPLET MATCHES "static" AND NOT VCPKG_TARGET_TRIPLET MATCHES "-md")
		set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
endif()

if(NOT SPT_STATIC_BUILD)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:steamworks>"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>"
		VERBATIM
	)

	get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
	get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
	find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

	if(WINDEPLOYQT_EXECUTABLE)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND "${WINDEPLOYQT_EXECUTABLE}"
				--no-translations
				--no-compiler-runtime
				--no-system-d3d-compiler
				--no-opengl-sw
				$<IF:$<CONFIG:Debug>,--debug,--release>
				"$<TARGET_FILE:${PROJECT_NAME}>"
			COMMENT "Deploying Qt runtime next to ${PROJECT_NAME}"
			VERBATIM
		)
	endif()
endif()

if(CMAKE_EXPORT_COMPILE_COMMANDS)
	set(COMPILE_COMMANDS_SRC "${CMAKE_BINARY_DIR}/compile_commands.json")
	set(COMPILE_COMMANDS_DST "${CMAKE_SOURCE_DIR}/build/compile_commands.json")
	if(EXISTS "${COMPILE_COMMANDS_SRC}")
		file(CREATE_LINK
			"${COMPILE_COMMANDS_SRC}"
			"${COMPILE_COMMANDS_DST}"
			COPY_ON_ERROR
			SYMBOLIC
		)
		message(STATUS "Created link: ${COMPILE_COMMANDS_DST} -> ${COMPILE_COMMANDS_SRC}")
	else()
		message(STATUS "Skipping link creation - source file doesn't exist yet")
	endif()
endif()
