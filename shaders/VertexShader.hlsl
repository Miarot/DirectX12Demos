struct PixelIn {
    float4 color : COLOR;
};

float4 main(PixelIn pin) : SV_Target {
    return pin.color;
}