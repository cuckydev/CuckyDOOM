cmake_minimum_required(VERSION 3.8)

if((${CMAKE_VERSION} VERSION_EQUAL 3.9) OR (${CMAKE_VERSION} VERSION_GREATER 3.9))
	cmake_policy(SET CMP0069 NEW)
endif()

###########
# Options #
###########
option(LTO "Enable link-time optimization" OFF)
option(PKG_CONFIG_STATIC_LIBS "On platforms with pkg-config, static-link the dependencies (good for Windows builds, so you don't need to bundle DLL files)" OFF)
option(MSVC_LINK_STATIC_RUNTIME "Link the static MSVC runtime library (Visual Studio only)" OFF)

#########
# Setup #
#########

# Define project, common source, and includes
project(CuckyDOOM LANGUAGES C)

add_executable(CuckyDOOM WIN32
	src/doomdef.c
	src/doomstat.c
	src/dstrings.c
	src/i_system.c
	src/i_sound.c
	src/i_video.c
	src/i_net.c
	src/i_main.c
	src/tables.c
	src/f_finale.c
	src/f_wipe.c
	src/d_main.c
	src/d_net.c
	src/d_items.c
	src/g_game.c
	src/m_menu.c
	src/m_misc.c
	src/m_argv.c
	src/m_bbox.c
	src/m_fixed.c
	src/m_swap.c
	src/m_cheat.c
	src/m_random.c
	src/am_map.c
	src/p_ceilng.c
	src/p_doors.c
	src/p_enemy.c
	src/p_floor.c
	src/p_inter.c
	src/p_lights.c
	src/p_map.c
	src/p_maputl.c
	src/p_plats.c
	src/p_pspr.c
	src/p_setup.c
	src/p_sight.c
	src/p_spec.c
	src/p_switch.c
	src/p_mobj.c
	src/p_telept.c
	src/p_tick.c
	src/p_saveg.c
	src/p_user.c
	src/r_bsp.c
	src/r_data.c
	src/r_draw.c
	src/r_main.c
	src/r_plane.c
	src/r_segs.c
	src/r_sky.c
	src/r_things.c
	src/w_wad.c
	src/wi_stuff.c
	src/v_video.c
	src/st_lib.c
	src/st_stuff.c
	src/hu_stuff.c
	src/hu_lib.c
	src/s_sound.c
	src/z_zone.c
	src/info.c
	src/sounds.c
)

target_include_directories(CuckyDOOM PRIVATE "src")

list(APPEND CMAKE_MODULE_PATH
	"${CMAKE_SOURCE_DIR}/cmake"
)

##################
# Compiler flags #
##################

# DOOM flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Og -ggdb3 -fsanitize=address")
target_compile_definitions(CuckyDOOM PRIVATE NORMALUNIX)
target_compile_definitions(CuckyDOOM PRIVATE LINUX)

# Strip release builds
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

# Statically link other libraries
if(PKG_CONFIG_STATIC_LIBS)
	target_link_options(CuckyDOOM PRIVATE "-static")
endif()

# Use LTO if requested
if(LTO)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT result)
	if(result)
		set_target_properties(CuckyDOOM PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
endif()

########
# MSVC #
########

# This is messy as hell, and has been replaced by CMAKE_MSVC_RUNTIME_LIBRARY,
# but that's a very recent CMake addition, so we're still doing it this way for now
if(MSVC AND MSVC_LINK_STATIC_RUNTIME)
	# Statically-link the CRT (vcpkg static libs do this)
	foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
		if(${flag_var} MATCHES "/MD")
			string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
		endif()
	endforeach()
endif()

# Do some other MSVC fixes
if(MSVC)
	# Disable warnings that normally fire up on MSVC when using "unsafe" functions instead of using MSVC's "safe" _s functions
	target_compile_definitions(CuckyDOOM PRIVATE _CRT_SECURE_NO_WARNINGS)
	
	# Make it so source files are recognized as UTF-8 by MSVC
	target_compile_options(CuckyDOOM PRIVATE "/utf-8")
	
	# Use `main` instead of `WinMain`
	set_target_properties(CuckyDOOM PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

##########################
# Further compiler setup #
##########################

# Force strict C++17
#set_target_properties(CuckyDOOM PROPERTIES
#	CXX_STANDARD 17
#	CXX_STANDARD_REQUIRED ON
#	CXX_EXTENSIONS OFF
#)

# Set debug executable name
set_target_properties(CuckyDOOM PROPERTIES DEBUG_OUTPUT_NAME "CuckyDOOM_debug")

# Set output directory
set(BUILD_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set_target_properties(CuckyDOOM PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIRECTORY}
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BUILD_DIRECTORY}
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${BUILD_DIRECTORY}
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BUILD_DIRECTORY}
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILD_DIRECTORY}
)

################
# Dependencies #
################

# Find PkgConfig for dependency linking
find_package(PkgConfig QUIET)

# Standard libraries
target_link_libraries(CuckyDOOM PRIVATE m)
target_link_libraries(CuckyDOOM PRIVATE X11)
target_link_libraries(CuckyDOOM PRIVATE Xext)

# Find SDL2
find_package(SDL2)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(sdl2 QUIET IMPORTED_TARGET sdl2)
endif()

# Link SDL2
if(TARGET PkgConfig::sdl2)
	if(PKG_CONFIG_STATIC_LIBS)
		# Pkg-Config static
		message(STATUS "Using system SDL2 (pkg-config, static)")
		list(REMOVE_ITEM sdl2_STATIC_CFLAGS "-Dmain=SDL_main")
		list(REMOVE_ITEM sdl2_STATIC_LDFLAGS "-lSDL2main")
		target_compile_options(CuckyDOOM PRIVATE ${sdl2_STATIC_CFLAGS})
		target_link_libraries(CuckyDOOM PRIVATE ${sdl2_STATIC_LDFLAGS})
	else()
		# Pkg-Config dynamic
		message(STATUS "Using system SDL2 (pkg-config, dynamic)")
		list(REMOVE_ITEM sdl2_CFLAGS "-Dmain=SDL_main")
		list(REMOVE_ITEM sdl2_LDFLAGS "-lSDL2main")
		target_compile_options(CuckyDOOM PRIVATE ${sdl2_CFLAGS})
		target_link_libraries(CuckyDOOM PRIVATE ${sdl2_LDFLAGS})
	endif()
elseif(TARGET SDL2::SDL2)
	# CMake-generated config (Arch, vcpkg, Raspbian)
	message(STATUS "Using system SDL2 (CMake, dynamic)")
	target_link_libraries(CuckyDOOM PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
	# CMake-generated config (Arch, vcpkg, Raspbian)
	message(STATUS "Using system SDL2 (CMake, static)")
	target_link_libraries(CuckyDOOM PRIVATE SDL2::SDL2-static)
elseif(SDL2_FOUND)
	# Autotools-generated config (MSYS2)
	message(STATUS "Using system SDL2 (Autotools)")
	target_include_directories(CuckyDOOM PRIVATE ${SDL2_INCLUDE_DIRS})
	target_link_libraries(CuckyDOOM PRIVATE ${SDL2_LIBRARIES})
endif()
