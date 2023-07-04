cmake_minimum_required( VERSION 3.25.1 )

set( TARGET_NAME MyD3D12Lib )

set( HEADER_FILES
	inc/MyD3D12Lib/BaseApp.h
	inc/MyD3D12Lib/Camera.h
	inc/MyD3D12Lib/CommandQueue.h
	inc/MyD3D12Lib/Helpers.h
	inc/MyD3D12Lib/MeshGeometry.h
	inc/MyD3D12Lib/Timer.h
	inc/MyD3D12Lib/UploadBuffer.h
)

set( SRC_FILES
	src/BaseApp.cpp
	src/Camera.cpp
	src/CommandQueue.cpp
	src/MeshGeometry.cpp
	src/Timer.cpp
)

add_library( ${TARGET_NAME} STATIC
	${HEADER_FILES}
	${SRC_FILES}
)

target_include_directories( ${TARGET_NAME}
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc
	PRIVATE ${PROJECT_SOURCE_DIR}/3rd-party
)

target_link_libraries( ${TARGET_NAME}
	PRIVATE d3d12.lib
	PRIVATE dxgi.lib
	PRIVATE dxguid.lib
	PRIVATE d3dcompiler.lib
	PRIVATE DirectXTex
)