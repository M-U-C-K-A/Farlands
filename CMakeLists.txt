cmake_minimum_required(VERSION 3.16)
project(farlands CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── Dependencies ────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)

# ── ImGui Sources ───────────────────────────────────────────────
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR
        "ImGui sources not found in ${IMGUI_DIR}.\n"
        "Run: git submodule update --init --recursive")
endif()

set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp"
)

# ── Project Sources ─────────────────────────────────────────────
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# ── Shader compilation ──────────────────────────────────────────
find_program(GLSLC glslc HINTS "${Vulkan_GLSLC_EXECUTABLE}" /opt/homebrew/bin $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC)
    message(FATAL_ERROR
        "glslc not found. Install the Vulkan SDK or shaderc (macOS: brew install shaderc).")
endif()

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")

file(GLOB SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag")
file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS "${SHADER_DIR}/*.glsl")
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV "${SHADER_OUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${GLSLC} -O -I${SHADER_DIR} ${SHADER} -o ${SPV}
        DEPENDS ${SHADER} ${SHADER_INCLUDES}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    list(APPEND SPV_SHADERS ${SPV})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(${PROJECT_NAME} shaders)

# Pass shader path to source code
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_DIR="${SHADER_OUT_DIR}"
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

# ── Outil de développement : aperçu du générateur de terrain ────
# cmake --build build --target terrain_preview && ./build/terrain_preview map.png
add_executable(terrain_preview EXCLUDE_FROM_ALL
    tools/terrain_preview.cpp
    src/world/terrain_generator.cpp
    src/world/structures.cpp
    src/world/biome.cpp
    src/world/noise.cpp
    src/world/chunk.cpp
    src/world/block.cpp
)
target_include_directories(terrain_preview PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(terrain_preview PRIVATE glm::glm)

# macOS / MoltenVK specifics
if(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_MACOS_MVK)
endif()
