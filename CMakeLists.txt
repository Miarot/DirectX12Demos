cmake_minimum_required( VERSION 3.25.1 ) # Latest version of CMake when this file was created.

# Use solution folders to organize projects
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Make sure DLL and EXE targets go to the same directory.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # Output directory for static lib (.LIB)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for shared lib (.DLL)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for executables (.EXE)

project( DirectXAppFromScratch LANGUAGES CXX )

# Set the startup project.
set_directory_properties( PROPERTIES
	VS_STARTUP_PROJECT DirectXAppFromScratch
)

set( TARGET_NAME DirectXAppFromScratch )

set( HEADER_FILES 
	inc/helpers.h
	inc/d3dx12.h
	inc/MeshGeometry.h
	inc/CommandQueue.h
	inc/BaseApp.h
	inc/SimpleGeoApp.h
)

set( SHADER_FILES
	shaders/VertexShader.hlsl
	shaders/PixelShader.hlsl
)

source_group( "Resources\\Shaders" FILES ${SHADER_FILES} )

set_source_files_properties( shaders/VertexShader.hlsl PROPERTIES 
    VS_SHADER_TYPE Vertex
    VS_SHADER_MODEL 5.1
)

set_source_files_properties( shaders/PixelShader.hlsl PROPERTIES 
    VS_SHADER_TYPE Pixel
    VS_SHADER_MODEL 5.1
)

set( SRC_FILES
	src/main.cpp
	src/CommandQueue.cpp
	src/BaseApp.cpp
	src/SimpleGeoApp.cpp
)

add_executable( ${TARGET_NAME} WIN32
	${HEADER_FILES}
	${SRC_FILES}
	${SHADER_FILES}
)

target_include_directories( ${TARGET_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries( ${TARGET_NAME}
	PUBLIC d3d12.lib
	PUBLIC dxgi.lib
	PUBLIC dxguid.lib
	PUBLIC d3dcompiler.lib
)