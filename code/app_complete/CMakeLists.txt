# CMake setup for CGP library project with OpenGL
cmake_minimum_required(VERSION 3.16)

project(complete
    DESCRIPTION "Dynamic skinning: complete application"
    LANGUAGES CXX
)

# Root of the project: dynamic_skinning_source, lib_helper (animated character, loaders), cgp library, shaders and assets
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# Collect sources automatically (this application, dynamic_skinning_source, lib_helper, cgp library, shaders)
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/*.[ch]pp"
    "${ROOT}/dynamic_skinning_source/*.[ch]pp"
    "${ROOT}/lib_helper/*.[ch]pp"
    "${ROOT}/cgp/*.[ch]pp"
    "${ROOT}/shaders/*.glsl"
)
# Unit tests of the cgp library are not part of the application
list(FILTER SRC_FILES EXCLUDE REGEX "/cgp/.*/test/")
list(FILTER SRC_FILES EXCLUDE REGEX "/app_complete/build/")

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${ROOT}"
    "${ROOT}/lib_helper"
    "${ROOT}/cgp"
)

# Compile definitions (macros)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SOLUTION
    CGP_OPENGL_3_3                # OpenGL version (alternatives: CGP_OPENGL_4_1, CGP_OPENGL_4_3, CGP_OPENGL_4_6)
    IMGUI_IMPL_OPENGL_LOADER_GLAD # ImGui uses GLAD for OpenGL loading
    CGP_ERROR_EXCEPTION           # Throw exceptions on errors (comment to abort instead)
    # CGP_NO_DEBUG                # Uncomment to disable assertions
)
# Assertions (bound checks) only in Debug: they are costly on the vec3 operations of the CPU skinning
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<NOT:$<CONFIG:Debug>>:CGP_NO_DEBUG>)

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
        RUNTIME_OUTPUT_DIRECTORY ${ROOT}$<0:>
        VS_DEBUGGER_WORKING_DIRECTORY ${ROOT}
    )
    source_group(TREE ${ROOT} FILES ${SRC_FILES})
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
            "${ROOT}/cgp/third_party/glfw/macos/include"
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${ROOT}/cgp/third_party/glfw/macos/lib/libglfw.3.dylib"
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
        "${ROOT}/cgp/third_party/glfw/windows/include"
    )
    if(MSVC)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${ROOT}/cgp/third_party/glfw/windows/lib-visual/glfw3.lib"
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${ROOT}/cgp/third_party/glfw/windows/lib-mingw-w64/libglfw3.a"
        )
    endif()
endif()

# Emscripten build (usage: mkdir build_em && cd build_em && emcmake cmake .. && emmake make; emrun index.html)
if(EMSCRIPTEN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CGP_NO_DEBUG)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PRIVATE
        -sMAX_WEBGL_VERSION=2 -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=ccall  # functions of web_interface.cpp, called by the HTML panel
        "SHELL:--preload-file ${ROOT}/shaders/@shaders/"
    )
    # Models: complete.data only holds the shaders and the default model (Giraffe, 0.4 MB), so that the demo
    # starts quickly. Each other model is a separate package (model_<name>.js + model_<name>.data), downloaded
    # by the web page when the model is chosen, or in the background (see web_page_models in the page template).
    if(EXISTS "${ROOT}/assets/giraffe")
        target_link_options(${PROJECT_NAME} PRIVATE
            "SHELL:--preload-file ${ROOT}/assets/giraffe@assets/giraffe"  # SHELL: avoids the de-duplication of --preload-file
        )
    endif()
    set(MODEL_PACKAGES clown cthulhu dragon snail spot train)
    set(MODEL_PACKAGE_OUTPUTS "")
    foreach(model ${MODEL_PACKAGES})
        if(NOT EXISTS "${ROOT}/assets/${model}")
            continue()
        endif()
        file(GLOB_RECURSE MODEL_FILES "${ROOT}/assets/${model}/*")
        # High resolution Cthulhu meshes (~100 MB) are not packaged in the web version
        list(FILTER MODEL_FILES EXCLUDE REGEX "/cthulhu/(300000|500000|1500000)/")
        add_custom_command(
            OUTPUT "${CMAKE_BINARY_DIR}/model_${model}.data" "${CMAKE_BINARY_DIR}/model_${model}.js"
            COMMAND "${EMSCRIPTEN_ROOT_PATH}/tools/file_packager" "model_${model}.data"
                --preload "${ROOT}/assets/${model}@assets/${model}"
                --exclude "${ROOT}/assets/cthulhu/300000" "${ROOT}/assets/cthulhu/500000" "${ROOT}/assets/cthulhu/1500000"
                --js-output=model_${model}.js --no-node
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            DEPENDS ${MODEL_FILES}
            COMMENT "Web package of the model ${model}"
        )
        list(APPEND MODEL_PACKAGE_OUTPUTS "${CMAKE_BINARY_DIR}/model_${model}.data")
    endforeach()
    add_custom_target(model_packages ALL DEPENDS ${MODEL_PACKAGE_OUTPUTS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${ROOT}/scripts/emscripten/emscripten_template.html"
            "${CMAKE_BINARY_DIR}/index.html"
    )
endif()
