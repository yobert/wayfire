cmake_minimum_required(VERSION 3.1.0)
project(animate CXX)

file(GLOB SRC "animate.cpp")

if (USE_GLES32)
    list(APPEND SRC "fire.cpp" "particle.cpp")
endif (USE_GLES32)

add_library(animate SHARED ${SRC})

install(TARGETS   animate    DESTINATION lib/wayfire/)
install(DIRECTORY shaders    DESTINATION share/wayfire/animate)
