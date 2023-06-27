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

struct PassConstants 
{
    float totalTime;
};

ConstantBuffer<PassConstants> PassConstantsCB :register(b1);

// Another way to define mvp gMVP is global name of matrix 
//cbuffer cbPerObject : register(b0) {
//    matrix gMVP;
//}

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    float3 timeColorModulation = { 
        1 + sin(PassConstantsCB.totalTime - 0.4),
        1 + cos(PassConstantsCB.totalTime + 0.9),
        1 + sin(PassConstantsCB.totalTime + 0.3)
    };
    
    timeColorModulation /= 2;
    
    vout.color = float4((vin.color * timeColorModulation),  1.0f);
    
    vout.position = mul(ModelViewProjectionCB.MVP, float4(vin.position, 1.0f));
    
    return vout;
}