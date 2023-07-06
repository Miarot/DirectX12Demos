cmake_minimum_required( VERSION 3.25.1 )

set( TARGET_NAME AppSimpleGeometry)

set( HEADER_FILES
	inc/AppStructures.h
	inc/FrameResources.h
	inc/SimpleGeoApp.h
)

set( SRC_FILES
	src/main.cpp
	src/FrameResources.cpp
	src/SimpleGeoApp.cpp
)

set( VERTEX_SHADER_FILES
    shaders/GeoVertexShader.hlsl
	shaders/SobelVertexShader.hlsl
)

set( PIXEL_SHADER_FILES
	shaders/GeoPixelShader.hlsl
	shaders/NormPixelShader.hlsl
	shaders/SobelPixelShader.hlsl
)

set( SHADER_HEADER_FILES
	shaders/GeoUtils.hlsli
	shaders/LightUtils.hlsli
)
set( SHADER_FILES
	${VERTEX_SHADER_FILES}
	${PIXEL_SHADER_FILES}
	${SHADER_HEADER_FILES}
)

source_group( "Shaders" FILES ${SHADER_FILES} )

set_source_files_properties( ${VERTEX_SHADER_FILES} ${PIXEL_SHADER_FILES}
    PROPERTIES
        VS_SHADER_MODEL 6.0
        VS_SHADER_DISABLE_OPTIMIZATIONS $<$<CONFIG:Debug>:ON>
        VS_SHADER_ENABLE_DEBUG $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:ON>
        VS_SHADER_FLAGS $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-Qembed_debug>
)

set_source_files_properties( ${VERTEX_SHADER_FILES}
    PROPERTIES 
        VS_SHADER_TYPE Vertex
)

set_source_files_properties( ${PIXEL_SHADER_FILES}
    PROPERTIES 
        VS_SHADER_TYPE Pixel
)

add_executable( ${TARGET_NAME} WIN32
	${HEADER_FILES}
	${SRC_FILES}
	${SHADER_FILES}
)

target_include_directories( ${TARGET_NAME} 
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc
	PRIVATE ${PROJECT_SOURCE_DIR}/3rd-party
)

target_link_libraries( ${TARGET_NAME}
	PRIVATE d3d12.lib
	PRIVATE dxgi.lib
	PRIVATE dxguid.lib
	PRIVATE d3dcompiler.lib
	PRIVATE MyD3D12Lib
)