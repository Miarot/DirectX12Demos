#include <GeoUtils.hlsl>

float4 main(VertexOut pin) : SV_Target 
{
    //return float4(pin.TexC, 0.0f, 1.0f);
    return float4((pin.Norm + 1) / 2, 1.0f);
}