#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include <GeoUtils.hlsl>

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);
ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

float4 main(VertexOut pin) : SV_Target
{
    float4 color;
    
    if (PassConstantsCB.IsDrawNorm == 1) 
    {
        color = float4((pin.Norm + 1) / 2, 1.0f);
    } else {
        float4 inderectLight = MaterilaConstantsCB.DiffuseAlbedo * PassConstantsCB.AmbientLight;
        
        Material mat = {
            MaterilaConstantsCB.DiffuseAlbedo,
            MaterilaConstantsCB.FresnelR0,
            1 - MaterilaConstantsCB.Roughness
        };
        float3 norm = normalize(pin.Norm);
        float3 toEye = normalize(PassConstantsCB.EyePos - pin.PosW);

        float4 directLight = ComputeLighting(PassConstantsCB.Lights, mat, norm, toEye, pin.PosW);
        
        color = inderectLight + directLight;
        
        // It is said that it is common convention to take alpha from diffuse material
        color.a = mat.DiffuseAlbedo.a;
    }
    
    return color;
}