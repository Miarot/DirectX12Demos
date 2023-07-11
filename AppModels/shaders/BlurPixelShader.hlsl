#include "Common.hlsli"

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

struct BlurConstants {
    int Radius;
    float Weight0;
    float Weight1;
    float Weight2;
    float Weight3;
    float Weight4;
    float Weight5;
    float Weight6;
    float Weight7;
    float Weight8;
    float Weight9;
    float Weight10;
    bool IsHorizontal;
};

ConstantBuffer<PassConstants> PassConstantsCB : register(b0);
ConstantBuffer<BlurConstants> BlurConstantsCB : register(b1);

Texture2D NormalMap : register(t0);
Texture2D DepthMap : register(t1);
Texture2D OcclusionMap : register(t3);
    
SamplerState DepthSampler : register(s0);
SamplerState LinearWrapSampler : register(s1);
SamplerState PointClampSampler : register(s2);

float4 main(VertexOut pin) : SV_Target
{
    float blurWeights[] = {
        BlurConstantsCB.Weight0, BlurConstantsCB.Weight1, BlurConstantsCB.Weight2,
        BlurConstantsCB.Weight3, BlurConstantsCB.Weight4, BlurConstantsCB.Weight5,
        BlurConstantsCB.Weight6, BlurConstantsCB.Weight7, BlurConstantsCB.Weight8,
        BlurConstantsCB.Weight9, BlurConstantsCB.Weight10
    };
    
    // get occlusion, normal and depth for central pixel
    float occlusion = blurWeights[BlurConstantsCB.Radius] * OcclusionMap.SampleLevel(LinearWrapSampler, pin.TexC, 0.0f).r;
    float3 centerNormal = NormalMap.SampleLevel(PointClampSampler, pin.TexC, 0.0f).xyz;
    float centerDepth = DepthMap.SampleLevel(DepthSampler, pin.TexC, 0.0f).r;
    centerDepth = PassConstantsCB.Proj[2][3] / (centerDepth - PassConstantsCB.Proj[2][2]);
    
    // choose tex step
    float2 texOffset;
    if (BlurConstantsCB.IsHorizontal) {
        texOffset = float2(PassConstantsCB.OcclusionMapWidthInv, 0.0f);
    } else {
        texOffset = float2(0.0f, PassConstantsCB.OcclusionMapHeightInv);
    }
    
    float addedWeightsSum = blurWeights[BlurConstantsCB.Radius];
        
    for (int i = -BlurConstantsCB.Radius; i < BlurConstantsCB.Radius; ++i) {
        if (i == 0) {
            continue;
        }
        
        // get normal and depth for nearby pixel
        float2 nearTexC = pin.TexC + i * texOffset;
        float3 nearNormal = NormalMap.SampleLevel(PointClampSampler, nearTexC, 0.0f).xyz;
        float nearDepth = DepthMap.SampleLevel(DepthSampler, nearTexC, 0.0f).r;
        nearDepth = PassConstantsCB.Proj[2][3] / (nearDepth - PassConstantsCB.Proj[2][2]);
        
        // if normals or depths differs to much then it is probably edge 
        if (dot(centerNormal, nearNormal) > 0.8f && abs(centerDepth - nearDepth) < 0.2f) {
            occlusion += blurWeights[BlurConstantsCB.Radius + i] * OcclusionMap.SampleLevel(LinearWrapSampler, nearTexC, 0.0f).r;
            addedWeightsSum += blurWeights[BlurConstantsCB.Radius + i];
        }
    }
    
    // normalize to actualy added weights
    occlusion /= addedWeightsSum;

    return occlusion;
}