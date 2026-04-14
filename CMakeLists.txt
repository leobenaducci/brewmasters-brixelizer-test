cmake_minimum_required(VERSION 3.10.0)
project(dx12engine VERSION 0.1.0 LANGUAGES C CXX)

if(POLICY CMP0175)
  cmake_policy(SET CMP0175 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(FFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Plugins/FidelityFX")

# Assimp (minimal).
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.3.1
)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(assimp)

# ImGui.
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)

file(GLOB_RECURSE ENGINE_SOURCES Engine/*.cpp)
file(GLOB_RECURSE APP_SOURCES App/*.cpp)

set(ALL_SOURCES ${APP_SOURCES} ${ENGINE_SOURCES})

add_compile_definitions(dx12engine NOMINMAX)
add_executable(dx12engine ${ALL_SOURCES} ${IMGUI_SOURCES})

# Includes.
target_include_directories(dx12engine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${FFX_DIR}/include
)

set(FFX_LIB_SUFFIX "$<$<CONFIG:Debug>:d>")

if(MSVC)
    target_link_options(dx12engine PRIVATE /STACK:16777216) # 16MB.
endif()

# Libraries.
target_link_libraries(dx12engine 
    PRIVATE 
    assimp::assimp 
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib

    "${FFX_DIR}/lib/ffx_backend_dx12_x64${FFX_LIB_SUFFIX}.lib"
    "${FFX_DIR}/lib/ffx_brixelizer_x64${FFX_LIB_SUFFIX}.lib"
    "${FFX_DIR}/lib/ffx_brixelizergi_x64${FFX_LIB_SUFFIX}.lib"
)

# Post build (copy dll to build directory).
add_custom_command(TARGET dx12engine POST_BUILD
    # Assimp.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:assimp::assimp>" $<TARGET_FILE_DIR:dx12engine>
    
    # Fidelity FX.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFX_DIR}/bin/ffx_backend_dx12_x64${FFX_LIB_SUFFIX}.dll" $<TARGET_FILE_DIR:dx12engine>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFX_DIR}/bin/ffx_brixelizer_x64${FFX_LIB_SUFFIX}.dll" $<TARGET_FILE_DIR:dx12engine>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFX_DIR}/bin/ffx_brixelizergi_x64${FFX_LIB_SUFFIX}.dll" $<TARGET_FILE_DIR:dx12engine>
    
    # Copy debug files if debug mode.
    COMMAND ${CMAKE_COMMAND} -E $<$<CONFIG:Debug>:copy_if_different> "${FFX_DIR}/bin/ffx_brixelizer_x64${FFX_LIB_SUFFIX}.pdb" $<TARGET_FILE_DIR:dx12engine>
    COMMAND ${CMAKE_COMMAND} -E $<$<CONFIG:Debug>:copy_if_different> "${FFX_DIR}/bin/ffx_brixelizergi_x64${FFX_LIB_SUFFIX}.pdb" $<TARGET_FILE_DIR:dx12engine>
)

if(WIN32)
    set(CMAKE_SYSTEM_VERSION 10.0)
endif()