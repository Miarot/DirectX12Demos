#pragma once

#include <DirectXMath.h>

#include <vector>

class Shaker {
public:
	Shaker();

	void Shake(DirectX::XMMATRIX& matrix, uint32_t width, uint32_t height);

private:
	float ShakePixelAmplitude;
	std::vector<DirectX::XMVECTOR> ShakeDirections;
	size_t ShakeDirectionIndex;
};