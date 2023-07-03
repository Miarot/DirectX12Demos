cmake_minimum_required( VERSION 3.25.1 )

set( TARGET_NAME SimpleGeometry)

set( HEADER_FILES
	inc/AppStructures.h
	inc/BaseApp.h
	inc/Camera.h
	inc/CommandQueue.h
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

add_executable( ${TARGET_NAME} WIN32
	${HEADER_FILES}
	${SRC_FILES}
)

target_include_directories( ${TARGET_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/inc
	${PROJECT_SOURCE_DIR}/3rd-party
)

target_link_libraries( ${TARGET_NAME}
	PUBLIC d3d12.lib
	PUBLIC dxgi.lib
	PUBLIC dxguid.lib
	PUBLIC d3dcompiler.lib
	PUBLIC DirectXTex
)