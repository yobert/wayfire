cmake_minimum_required(VERSION 3.1.0)
project(vkeyboard CXX)

find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)

pkg_check_modules(WAYLAND_PROTOCOLS wayland-protocols)
execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=pkgdatadir wayland-protocols
    OUTPUT_VARIABLE WAYLAND_PROTOCOLS_PKGDATADIR
    RESULT_VARIABLE _pkgconfig_failed)

string (STRIP ${WAYLAND_PROTOCOLS_PKGDATADIR} WAYLAND_PROTOCOLS_PKGDATADIR)

if (_pkgconfig_failed)
    message("Compiling without vkeyboard support!")
    return()
endif (_pkgconfig_failed)

# text-input protocol files
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/zwp-text-input-client-v1.h
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/text-input/text-input-unstable-v1.xml ${CMAKE_CURRENT_SOURCE_DIR}/zwp-text-input-client-v1.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/zwp-text-input-server-v1.h
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} server-header ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/text-input/text-input-unstable-v1.xml ${CMAKE_CURRENT_SOURCE_DIR}/zwp-text-input-server-v1.h)

add_custom_command(OUTPUT zwp-text-input-protocol-v1.c
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} code ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/text-input/text-input-unstable-v1.xml ${CMAKE_CURRENT_BINARY_DIR}/zwp-text-input-protocol-v1.c
    DEPENDS zwp-text-input-server-v1.h zwp-text-input-client-v1.h)

# input-method protocol files
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/zwp-input-method-client-v1.h
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/input-method/input-method-unstable-v1.xml ${CMAKE_CURRENT_SOURCE_DIR}/zwp-input-method-client-v1.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/zwp-input-method-server-v1.h
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} server-header ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/input-method/input-method-unstable-v1.xml ${CMAKE_CURRENT_SOURCE_DIR}/zwp-input-method-server-v1.h)

add_custom_command(OUTPUT zwp-input-method-protocol-v1.c
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} code ${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/input-method/input-method-unstable-v1.xml ${CMAKE_CURRENT_BINARY_DIR}/zwp-input-method-protocol-v1.c
    DEPENDS zwp-input-method-server-v1.h zwp-input-method-client-v1.h)

add_library(vkeyboard SHARED "zwp-text-input-protocol-v1.c" "zwp-input-method-protocol-v1.c")

#install(TARGETS vkeyboard DESTINATION lib/wayfire/)
