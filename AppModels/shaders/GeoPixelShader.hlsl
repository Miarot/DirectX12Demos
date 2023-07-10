#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 3
#endif

#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);
ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

Texture2D Texture : register(t0);
Texture2D OcclusionMap : register(t1);
SamplerState LinearWrapSampler : register(s0);

float4 main(VertexOut pin) : SV_Target
{
    Material mat =
    {
        MaterilaConstantsCB.DiffuseAlbedo * Texture.Sample(LinearWrapSampler, pin.TexC),
        MaterilaConstantsCB.FresnelR0,
        1 - MaterilaConstantsCB.Roughness
    };
    
    #ifdef ALPHA_TEST
        clip(mat.DiffuseAlbedo.a - 0.1f);
    #endif
    
    float3 norm = normalize(pin.Norm);
    
    #ifdef DRAW_NORMS
        return float4((norm + 1) / 2, 1.0f);
    #endif
    
    float4 inderectLight = 1.0f;
    
    #ifdef SSAO
        float4 occlusionMapTexC = mul(PassConstantsCB.ViewProjTex, float4(pin.PosW, 1.0f));
        occlusionMapTexC /= occlusionMapTexC.w;
        float occlusion = OcclusionMap.SampleLevel(LinearWrapSampler, occlusionMapTexC.xy, 0.0f).r;
        inderectLight = occlusion;
        //return occlusion;
    #endif
    
    inderectLight *= mat.DiffuseAlbedo * PassConstantsCB.AmbientLight;
    float3 toEye = normalize(PassConstantsCB.EyePos - pin.PosW);

    float4 directLight = ComputeLighting(PassConstantsCB.Lights, mat, norm, toEye, pin.PosW);
        
    float4 color = inderectLight + directLight;
        
    // It is said that it is common convention to take alpha from diffuse material
    color.a = mat.DiffuseAlbedo.a;
    
    return color;
}