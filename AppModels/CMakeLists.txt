cmake_minimum_required( VERSION 3.26.3 )

set (CMAKE_CXX_STANDARD 17)

set( TARGET_NAME AppModels )

set( HEADER_FILES
	inc/AppStructures.h
	inc/FrameResources.h
	inc/ModelsApp.h
)

set( SRC_FILES
	src/main.cpp
	src/FrameResources.cpp
	src/ModelsApp.cpp
)

set( VERTEX_SHADER_FILES
    shaders/GeoVertexShader.hlsl
	shaders/SSAOVertexShader.hlsl
	shaders/FullScreenQuadVS.hlsl
)

set( PIXEL_SHADER_FILES
	shaders/GeoPixelShader.hlsl	
	shaders/SobelPixelShader.hlsl
	shaders/SSAONormVPixelShader.hlsl
	shaders/SSAOPixelShader.hlsl
	shaders/BlurPixelShader.hlsl
)

set( SHADER_HEADER_FILES
	shaders/GeoUtils.hlsli
	shaders/LightUtils.hlsli
	shaders/Common.hlsli
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
	PRIVATE assimp
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ASSIMP_DIR}/bin/Debug/assimp-vc143-mtd.dll
		${CMAKE_CURRENT_BINARY_DIR})  