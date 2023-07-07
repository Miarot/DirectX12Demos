Texture2D NormalsMap : register(t0);
Texture2D DepthMap : register(t1);

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    return DepthMap.Load(int3(pin.PosH.xy, 0));
    return float4((pin.PosV.xyz + 1.0f) / 2, 1.0f);
}