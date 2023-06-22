struct VertexIn
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOut
{
    float4 color : COLOR;
    float4 position : SV_Position;
};

struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

// Another way to define mvp gMVP is global name of matrix 
//cbuffer cbPerObject : register(b0) {
//    matrix gMVP;
//}

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    vout.color = float4(vin.color, 1.0f);
    vout.position = mul(ModelViewProjectionCB.MVP, float4(vin.position, 1.0f));
    
    return vout;
}