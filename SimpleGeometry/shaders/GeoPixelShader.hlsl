#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 1
#endif

#include <GeoUtils.hlsl>

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);
ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

float4 main(VertexOut pin) : SV_Target
{
    Material mat = {
        MaterilaConstantsCB.DiffuseAlbedo * Texture.Sample(Sampler, pin.TexC),
        MaterilaConstantsCB.FresnelR0,
        1 - MaterilaConstantsCB.Roughness
    };
    
    float4 inderectLight = mat.DiffuseAlbedo * PassConstantsCB.AmbientLight;
        
    
    float3 norm = normalize(pin.Norm);
    float3 toEye = normalize(PassConstantsCB.EyePos - pin.PosW);

    float4 directLight = ComputeLighting(PassConstantsCB.Lights, mat, norm, toEye, pin.PosW);
        
    float4 color = inderectLight + directLight;
        
    // It is said that it is common convention to take alpha from diffuse material
    color.a = mat.DiffuseAlbedo.a;
    
    return color;
}