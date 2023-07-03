cmake_minimum_required( VERSION 3.25.1 )

set( TARGET_NAME DirectXTex)

add_library( ${TARGET_NAME} STATIC
	DDSTextureLoader/DDSTextureLoader12.h
	DDSTextureLoader/DDSTextureLoader12.cpp
)

target_include_directories( ${TARGET_NAME} PUBLIC
	${PROJECT_SOURCE_DIR}/3rd-party
	${CMAKE_CURRENT_SOURCE_DIR}/DDSTextureLoader
)