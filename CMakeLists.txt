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
)

set( SRC_FILES
	src/main.cpp
)

add_executable( ${TARGET_NAME} WIN32
	${HEADER_FILES}
	${SRC_FILES}
)

target_include_directories( ${TARGET_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries( ${TARGET_NAME}
	PUBLIC d3d12.lib
	PUBLIC dxgi.lib
)