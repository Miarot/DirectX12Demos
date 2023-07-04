#include <MyD3D12Lib/Shaker.h>

using namespace DirectX;

Shaker::Shaker() {
	ShakePixelAmplitude = 5.0f;

	ShakeDirections = {
		{ 0.0f,     1.0f,  0.0f,    0.0f },
		{ 0.0f,     -1.0f, 0.0f,    0.0f },
		{ 1.0f,     0.0f,  0.0f,    0.0f },
		{ -1.0f,    0.0f,  0.0f,    0.0f }
	};

	ShakeDirectionIndex = 0;
}

void Shaker::Shake(DirectX::XMMATRIX& matrix, uint32_t width, uint32_t height) {
	XMVECTOR pixelNorm = { 2.0f / width, 2.0f / height, 0.0f, 0.0f };
	XMVECTOR displacement = ShakeDirections[ShakeDirectionIndex] * pixelNorm;

	matrix.r[2] += displacement * ShakePixelAmplitude;
	ShakeDirectionIndex = (ShakeDirectionIndex + 1) % ShakeDirections.size();
}