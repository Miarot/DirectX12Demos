cmake_minimum_required( VERSION 3.25.1 )

project( DirectX12Demos LANGUAGES CXX )

set( HEADER_FILES
	inc/AppStructures.h
	inc/BaseApp.h
	inc/Camera.h
	inc/CommandQueue.h
	inc/d3dx12.h
	inc/FrameResources.h
	inc/Helpers.h
	inc/MeshGeometry.h
	inc/SimpleGeoApp.h
	inc/Timer.h
	inc/UploadBuffer.h
)

set( SRC_FILES
	src/BaseApp.cpp
	src/Camera.cpp
	src/CommandQueue.cpp
	src/FrameResources.cpp
	src/main.cpp
	src/MeshGeometry.cpp
	src/SimpleGeoApp.cpp
	src/Timer.cpp
)

set( SHADER_FILES
	shaders/GeoPixelShader.hlsl
	shaders/GeoVertexShader.hlsl
	shaders/GeoUtils.hlsl
	shaders/LightUtils.hlsl
	shaders/NomrPixelShader.hlsl
	shaders/SobelPixelShader.hlsl
	shaders/SobelVertexShader.hlsl
)

set( 3RD_PARTY_FILES
	3rd-party/DirectXTex/DDSTextureLoader/DDSTextureLoader12.cpp
	3rd-party/DirectXTex/DDSTextureLoader/DDSTextureLoader12.h
)

add_executable( ${PROJECT_NAME} WIN32
	${HEADER_FILES}
	${SRC_FILES}
	${3RD_PARTY_FILES}
)

target_include_directories( ${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/inc
	${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/DirectXTex/DDSTextureLoader
)

target_link_libraries( ${PROJECT_NAME}
	PUBLIC d3d12.lib
	PUBLIC dxgi.lib
	PUBLIC dxguid.lib
	PUBLIC d3dcompiler.lib
)