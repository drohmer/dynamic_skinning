# Dynamic skinning: the two applications share dynamic_skinning_source/ (the method), lib_helper/ (animated
# character, loaders), cgp/, shaders/ and assets/
#   app_complete/ : complete application (GPU, joint picking, delayed LBS, all the models; also web version)
#   app_minimal/  : minimal application (CPU only, Cylinder / Girafe / Clown, camera and f, tau, A, D)
# Each application can also be built alone from its own directory.
cmake_minimum_required(VERSION 3.16)
project(dynamic_skinning LANGUAGES CXX)

# Executables directly in the build directory
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

add_subdirectory(app_complete)
if(NOT EMSCRIPTEN)
    add_subdirectory(app_minimal)
endif()
