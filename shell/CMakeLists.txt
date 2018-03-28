cmake_minimum_required(VERSION 3.1.0)
find_package(PkgConfig REQUIRED)


file(GLOB PANEL_SOURCES "panel.cpp" "main.cpp" "widgets.cpp" "gamma.cpp" "net.cpp")
file(GLOB BACKGROUND_SOURCES "background.cpp")
file(GLOB BACKEND_SRC "window.cpp" "shm-surface.cpp")

pkg_check_modules(REQLIBS libevdev freetype2 wayland-client cairo wayland-cursor gio-2.0)

add_definitions(${REQLIBS_CFLAGS_OTHER})
link_directories(${REQLIBS_LIBRARY_DIRS})
include_directories(${REQLIBS_INCLUDE_DIRS})

if (HAS_PIXBUF)
    add_definitions(${PIXBUF_CFLAGS_OTHER})
    link_directories(${PIXBUF_LIBRARY_DIRS})
    include_directories(${PIXBUF_INCLUDE_DIRS})
endif(HAS_PIXBUF)

add_library(wayfire-shell-window STATIC ${BACKEND_SRC})
target_link_libraries(wayfire-shell-window wayfire-shell-proto)

add_executable(wayfire-shell-panel ${PANEL_SOURCES})
add_executable(wayfire-shell-background ${BACKGROUND_SOURCES})

find_package (Threads)
target_link_libraries(wayfire-shell-panel ${CMAKE_THREAD_LIBS_INIT} ${REQLIBS_LIBRARIES}
                      wayfire-shell-proto wayfire-config wayfire-shell-window)
target_link_libraries(wayfire-shell-background ${REQLIBS_LIBRARIES}
                      wayfire-shell-proto wayfire-config wayfire-shell-window)

if (HAS_PIXBUF)
    target_link_libraries(wayfire-shell-background ${PIXBUF_LIBRARIES})
endif(HAS_PIXBUF)

add_executable(wayfire-virtual-keyboard "vkeyboard.cpp")
target_link_libraries(wayfire-virtual-keyboard wayfire-shell-proto wayfire-config wayfire-shell-window)
target_link_libraries(wayfire-virtual-keyboard ${REQLIBS_LIBRARIES})

pkg_check_modules(ALSA alsa)
if (${ALSA_FOUND})
    add_definitions(${ALSA_CFLAGS_OTHER})
    link_directories(${ALSA_LIBRARY_DIRS})
    include_directories(${ALSA_INCLUDE_DIRS})

    add_executable(wayfire-sound-popup "sound-popup.cpp")
    target_link_libraries(wayfire-sound-popup wayfire-shell-proto wayfire-shell-window)
    target_link_libraries(wayfire-sound-popup ${REQLIBS_LIBRARIES} ${ALSA_LIBRARIES})

    if (HAS_CAIRO_GL_H)
        target_link_libraries(wayfire-sound-popup ${EGL_SURF_BACKEND_LIBRARIES})
    endif (HAS_CAIRO_GL_H)
endif(${ALSA_FOUND})


install(TARGETS wayfire-shell-panel      DESTINATION lib/wayfire/)
install(TARGETS wayfire-shell-background DESTINATION lib/wayfire/)
install(TARGETS wayfire-virtual-keyboard DESTINATION lib/wayfire/)
install(TARGETS wayfire-sound-popup      DESTINATION lib/wayfire/)
install(TARGETS wayfire-shell-window     DESTINATION lib/wayfire/)
