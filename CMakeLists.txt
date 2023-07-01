cmake_minimum_required( VERSION 3.25.1 )

project( DirectXDemos LANGUAGES CXX )

set( HEADER_FILES 
	inc/helpers.h
	inc/d3dx12.h
	inc/MeshGeometry.h
	inc/CommandQueue.h
	inc/BaseApp.h
	inc/SimpleGeoApp.h
	inc/Camera.h
	inc/Timer.h
	inc/UploadBuffer.h
	inc/FrameResources.h
	inc/RenderItem.h
	inc/Material.h
	inc/Light.h
)

set( SRC_FILES
	src/main.cpp
	src/CommandQueue.cpp
	src/BaseApp.cpp
	src/SimpleGeoApp.cpp
	src/Camera.cpp
	src/Timer.cpp
	src/FrameResources.cpp
)

set( SHADER_FILES
	shaders/VertexShader.hlsl
	shaders/PixelShader.hlsl
	shaders/SobelVertexShader.hlsl
	shaders/SobelPixelShader.hlsl
)

add_executable( ${PROJECT_NAME} WIN32
	${HEADER_FILES}
	${SRC_FILES}
)

target_include_directories( ${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries( ${PROJECT_NAME}
	PUBLIC d3d12.lib
	PUBLIC dxgi.lib
	PUBLIC dxguid.lib
	PUBLIC d3dcompiler.lib
)