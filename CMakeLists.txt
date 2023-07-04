cmake_minimum_required( VERSION 3.25.1 )

project( DirectX12Demos LANGUAGES CXX )

add_subdirectory( 3rd-party/DirectXTex )

add_subdirectory( SimpleGeometry )

set_directory_properties( PROPERTIES 
    VS_STARTUP_PROJECT SimpleGeometry
)