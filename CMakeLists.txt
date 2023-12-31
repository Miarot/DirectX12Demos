cmake_minimum_required( VERSION 3.25.1 )

project( DirectX12Demos LANGUAGES CXX )

set( ASSIMP_DIR ${PROJECT_SOURCE_DIR}/3rd-party/assimp )
add_library( assimp SHARED IMPORTED )
set_target_properties(assimp PROPERTIES
    IMPORTED_CONFIGURATIONS        "DEBUG"
    IMPORTED_LOCATION_DEBUG        ${ASSIMP_DIR}/bin/Debug/assimp-vc143-mtd.dll
    IMPORTED_IMPLIB_DEBUG          ${ASSIMP_DIR}/lib/Debug/assimp-vc143-mtd.lib
    INTERFACE_INCLUDE_DIRECTORIES  ${ASSIMP_DIR}/include
)

set( DIRECTXTK12_DIR ${PROJECT_SOURCE_DIR}/3rd-party/DirectXTK12)
add_library( DirectXTK12 STATIC IMPORTED )
set_target_properties( DirectXTK12 PROPERTIES
    IMPORTED_CONFIGURATIONS        "DEBUG"
    IMPORTED_LOCATION_DEBUG        ${DIRECTXTK12_DIR}/bin/Debug/DirectXTK12.lib
    IMPORTED_IMPLIB_DEBUG          ${DIRECTXTK12_DIR}/bin/Debug/DirectXTK12.lib
    INTERFACE_INCLUDE_DIRECTORIES  ${DIRECTXTK12_DIR}/include
)

add_subdirectory( MyD3D12Lib )

add_subdirectory( AppSimpleGeometry )

add_subdirectory( AppModels )

set_directory_properties( PROPERTIES 
    VS_STARTUP_PROJECT AppModels
)