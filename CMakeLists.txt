cmake_minimum_required(VERSION 3.1.0)

set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
project(wayfire)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(WAYFIRE_DEBUG_ENABLED TRUE)
else(CMAKE_BUILD_TYPE MATCHES Debug)
    set(WAYFIRE_DEBUG_ENABLED FALSE)
endif (CMAKE_BUILD_TYPE MATCHES Debug)

find_package(PkgConfig)

pkg_check_modules(IMAGEIO_LIBS libpng libjpeg)
if (${IMAGEIO_LIBS_FOUND})
    set(BUILD_WITH_IMAGEIO TRUE)
else (${IMAGEIO_LIBS_FOUND})
    set(BUILD_WITH_IMAGEIO FALSE)
endif (${IMAGEIO_LIBS_FOUND})

pkg_check_modules(PIXBUF gdk-2.0 gdk-pixbuf-2.0)
if (${PIXBUF_FOUND})
    set(HAS_PIXBUF TRUE)
else (${PIXBUF_FOUND})
    set(HAS_PIXBUF FALSE)
endif (${PIXBUF_FOUND})

if (NOT DEFINED USE_GLES32)
    set(USE_GLES32 TRUE)
endif(NOT DEFINED USE_GLES32)

if (NOT DEFINED HAS_CAIRO_GL_H)
    include(CheckIncludeFiles)
    CHECK_INCLUDE_FILES("cairo/cairo-gl.h" HAS_CAIRO_GL_H)
endif(NOT DEFINED HAS_CAIRO_GL_H)

if (HAS_CAIRO_GL_H)
    set(HAS_CAIRO_GL_H TRUE)
else (HAS_CAIRO_GL_H)
    set(HAS_CAIRO_GL_H FALSE)
endif (HAS_CAIRO_GL_H)

# TODO: check if the egl-surface backend in shell still works
set(HAS_CAIRO_GL_H FALSE)

configure_file(config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

message("\n\nConfiguration options:\n")
message("\tImageIO:          " ${BUILD_WITH_IMAGEIO})
message("\tOpenGL ES 3.2:    " ${USE_GLES32})
message("\tCario-GL:         " ${HAS_CAIRO_GL_H})
message("\tGdk-Pixbuf:       " ${HAS_PIXBUF})
message("\tDebugging output: " ${WAYFIRE_DEBUG_ENABLED})
message("\n")

include_directories(src)
include_directories(src/api)
include_directories(shared)

add_subdirectory(proto)
add_subdirectory(shared)
add_subdirectory(src)

# Installation
install(DIRECTORY shaders/ DESTINATION share/wayfire/shaders)
install(FILES wayfire.desktop DESTINATION share/wayland-sessions)

# Plugins
add_subdirectory(plugins)

# Shell
add_subdirectory(shell)
