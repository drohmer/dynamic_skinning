# CMake setup for CGP library project with OpenGL
cmake_minimum_required(VERSION 3.16)

project(project
    DESCRIPTION "Project"
    LANGUAGES CXX
)

# Collect sources automatically (src + cgp library + shaders)
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.[ch]pp"
    "${CMAKE_CURRENT_LIST_DIR}/cgp/*.[ch]pp"
    "${CMAKE_CURRENT_LIST_DIR}/shaders/*.glsl"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/cgp"
)

# Compile definitions (macros)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SOLUTION
    CGP_OPENGL_3_3                # OpenGL version (alternatives: CGP_OPENGL_4_1, CGP_OPENGL_4_3, CGP_OPENGL_4_6)
    IMGUI_IMPL_OPENGL_LOADER_GLAD # ImGui uses GLAD for OpenGL loading
    CGP_ERROR_EXCEPTION           # Throw exceptions on errors (comment to abort instead)
    # CGP_NO_DEBUG                # Uncomment to disable assertions
)

# Compiler standard
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Option to use precompiled GLFW on macOS
option(MACOS_GLFW_PRECOMPILED "Use precompiled library for GLFW on MacOS" OFF)

# Compiler warnings
if(MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}$<0:>
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SRC_FILES})
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-
        /wd4127 /wd4267 /wd4244 /wd4996 /wd26451 /wd26495 /wd4305 /wd4458
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -pedantic
        -Wfatal-errors -Wno-pragmas -Wno-unknown-warning-option
        -Wno-sign-compare -Wno-type-limits
    )
endif()

# Link required libraries for Unix (Glad needs dl, OpenGL framework on macOS)
if(UNIX AND NOT EMSCRIPTEN)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

# GLFW library setup (windowing for OpenGL)
if(UNIX AND NOT EMSCRIPTEN)
    if(APPLE AND MACOS_GLFW_PRECOMPILED)
        # Use precompiled GLFW (option for macOS)
        message(STATUS "Using precompiled GLFW for macOS")
        target_include_directories(${PROJECT_NAME} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cgp/third_party/glfw/macos/include"
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cgp/third_party/glfw/macos/lib/libglfw.3.dylib"
        )
    else()
        # Use system GLFW (Linux/macOS via pkg-config)
        find_package(PkgConfig REQUIRED)
        pkg_search_module(GLFW REQUIRED glfw3)
        target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIRS})
        target_link_directories(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARY_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_LIBRARIES})
    endif()
elseif(WIN32)
    # Use bundled GLFW for Windows
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/cgp/third_party/glfw/windows/include"
    )
    if(MSVC)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cgp/third_party/glfw/windows/lib-visual/glfw3.lib"
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/cgp/third_party/glfw/windows/lib-mingw-w64/libglfw3.a"
        )
    endif()
endif()

# Emscripten build (usage: mkdir build_em && cd build_em && emcmake cmake .. && emmake make; emrun index.html)
if(EMSCRIPTEN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CGP_NO_DEBUG)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PRIVATE
        -sMAX_WEBGL_VERSION=2 -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1
        --preload-file "${CMAKE_SOURCE_DIR}/shaders/@shaders/"
    )
    file(GLOB ASSETS_FILES "${CMAKE_SOURCE_DIR}/assets/*")
    if(ASSETS_FILES)
        target_link_options(${PROJECT_NAME} PRIVATE
            --preload-file "${CMAKE_SOURCE_DIR}/assets/@assets/"
        )
    endif()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/scripts/emscripten/emscripten_template.html"
            "${CMAKE_BINARY_DIR}/index.html"
    )
endif()
