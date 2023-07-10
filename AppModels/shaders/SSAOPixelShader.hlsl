#include "Common.hlsli"

Texture2D NormalsMap : register(t0);
Texture2D DepthMap : register(t1);
Texture2D RandomMap : register(t2);

SamplerState DepthSampler : register(s0);
SamplerState LinearWrapSampler : register(s1);
SamplerState PointClampSampler : register(s2);

ConstantBuffer<PassConstants> PassConstantsCB : register(b0);

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    // reconstruct current point's coordinates in view space
    float pz = DepthMap.SampleLevel(DepthSampler, pin.TexC, 0.0f).r;
    // z` = A + B /z => z = B / (z` - A), A = Proj[2][2], B = Proj[2][3] 
    pz = PassConstantsCB.Proj[2][3] / (pz - PassConstantsCB.Proj[2][2]);
    float3 p = pin.PosV * pz / pin.PosV.z;
    
    float3 n = NormalsMap.SampleLevel(PointClampSampler, pin.TexC, 0.0f).xyz;
    float3 randVec = 2 * RandomMap.SampleLevel(LinearWrapSampler, pin.TexC * 8.0f, 0.0f).xyz - 1.0f; 
    float sumOcclusion = 0.0f;
    
    int numSamples = 10;
    
    for (int i = 0; i < numSamples; ++i)
    {
        // get random point q near p
        float3 offset = reflect(PassConstantsCB.RandomDirections[i].xyz, randVec);
        float3 q = p + sign(dot(offset, n)) * PassConstantsCB.OcclusionRadius * offset;
        
        // find q's texture coordinates
        float4 qTexC = mul(PassConstantsCB.ProjTex, float4(q, 1.0f));
        qTexC /= qTexC.w;
        
        // reconstruct point r in q`s direction
        float rz = DepthMap.SampleLevel(DepthSampler, qTexC.xy, 0.0f).r;
        rz = PassConstantsCB.Proj[2][3] / (rz - PassConstantsCB.Proj[2][2]);
        float3 r = q * rz / q.z;
        
        // count occlusion by depth difference and angle with normal
        float distZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);
        float curOcclusion = 0.0f;
        
        // if to close then points probably in the same surface
        if (distZ > PassConstantsCB.OcclusionEpsilon) {
            // fade lineary from fade start to fade end
            curOcclusion = saturate((PassConstantsCB.OcclusionFadeEnd - distZ) / (PassConstantsCB.OcclusionFadeEnd - PassConstantsCB.OcclusionFadeStart));
        }
        
        // if (r - p) orthogonal to n then probably they are in the same surface
        curOcclusion *= dp;
        
        sumOcclusion += curOcclusion;
    }
    
    sumOcclusion /= numSamples;
    
    return saturate(pow(1 - sumOcclusion, 2.0f));
}