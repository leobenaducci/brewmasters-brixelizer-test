cmake_minimum_required(VERSION 3.10.0)
project(dx12engine VERSION 0.1.0 LANGUAGES C CXX)

if(POLICY CMP0175)
  cmake_policy(SET CMP0175 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- CONFIGURACIÓN DE RUTAS ---
set(FFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Plugins/FidelityFX")

# --- ASSIMP ---
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v6.0.4
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

# --- IMGUI ---
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

# --- INCLUDES ---
target_include_directories(dx12engine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${FFX_DIR}/include        # FidelityFX Headers
)

# --- LÓGICA DE LIBRERÍAS FIDELITYFX (Debug vs Release) ---
# El SDK usa sufijos 'd' para Debug y nada para Release.
set(FFX_LIB_SUFFIX "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(FFX_LIB_SUFFIX "d")
endif()

if(MSVC)
    target_link_options(dx12engine PRIVATE /STACK:16777216)  # 16MB
endif()

# --- LIBRARIES ---
target_link_libraries(dx12engine 
    PRIVATE 
    assimp::assimp 
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
    
    # FidelityFX Libs
    ${FFX_DIR}/lib/ffx_backend_dx12_x64${FFX_LIB_SUFFIX}.lib
    ${FFX_DIR}/lib/ffx_brixelizer_x64${FFX_LIB_SUFFIX}.lib
    ${FFX_DIR}/lib/ffx_brixelizergi_x64${FFX_LIB_SUFFIX}.lib
)

# --- POST-BUILD (DLL & PDB Copy) ---
add_custom_command(TARGET dx12engine POST_BUILD
    # Copiar DLL de Assimp
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:assimp::assimp>" $<TARGET_FILE_DIR:dx12engine>
    
    # Copiar DLLs de FidelityFX
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFX_DIR}/bin/ffx_backend_dx12_x64${FFX_LIB_SUFFIX}.dll" $<TARGET_FILE_DIR:dx12engine>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFX_DIR}/bin/ffx_brixelizer_x64${FFX_LIB_SUFFIX}.dll" $<TARGET_FILE_DIR:dx12engine>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFX_DIR}/bin/ffx_brixelizergi_x64${FFX_LIB_SUFFIX}.dll" $<TARGET_FILE_DIR:dx12engine>
    
    # Copiar PDBs (Solo en Debug para facilitar el rastreo de errores en el motor)
    COMMAND ${CMAKE_COMMAND} -E $<$<CONFIG:Debug>:copy_if_different> "${FFX_DIR}/bin/ffx_brixelizer_x64${FFX_LIB_SUFFIX}.pdb" $<TARGET_FILE_DIR:dx12engine>
    COMMAND ${CMAKE_COMMAND} -E $<$<CONFIG:Debug>:copy_if_different> "${FFX_DIR}/bin/ffx_brixelizergi_x64${FFX_LIB_SUFFIX}.pdb" $<TARGET_FILE_DIR:dx12engine>
)

if(WIN32)
    set(CMAKE_SYSTEM_VERSION 10.0)
endif()