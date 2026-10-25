# Minimal application of dynamic skinning (CPU only): Cylinder, Giraffe and Clown
#   Build:  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
#   Run:    ./build/minimal   (shaders/ and assets/ are found in the root directory)
cmake_minimum_required(VERSION 3.16)
project(minimal LANGUAGES CXX)

# Root of the project: dynamic_skinning_source, lib_helper (animated character, loaders), cgp library, shaders and assets
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/*.[ch]pp"
    "${ROOT}/dynamic_skinning_source/*.[ch]pp"
    "${ROOT}/lib_helper/*.[ch]pp"
    "${ROOT}/cgp/*.[ch]pp"
)
list(FILTER SRC_FILES EXCLUDE REGEX "/cgp/.*/test/")
list(FILTER SRC_FILES EXCLUDE REGEX "/app_minimal/build/")

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}" "${ROOT}" "${ROOT}/lib_helper" "${ROOT}/cgp")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CGP_OPENGL_3_3 IMGUI_IMPL_OPENGL_LOADER_GLAD CGP_ERROR_EXCEPTION
    $<$<NOT:$<CONFIG:Debug>>:CGP_NO_DEBUG>  # bound checks only in Debug (costly in the CPU skinning)
)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unknown-warning-option)  # pragmas of the third party libraries
endif()

# GLFW
if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE "${ROOT}/cgp/third_party/glfw/windows/include")
    if(MSVC)
        target_link_libraries(${PROJECT_NAME} PRIVATE "${ROOT}/cgp/third_party/glfw/windows/lib-visual/glfw3.lib")
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE "${ROOT}/cgp/third_party/glfw/windows/lib-mingw-w64/libglfw3.a")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_search_module(GLFW REQUIRED glfw3)
    target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARIES} dl)
endif()
