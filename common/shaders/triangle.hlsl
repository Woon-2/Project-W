float4 VSMain(uint vertexID : SV_VertexID) : SV_POSITION {
    float4 result;
    // triangle
    if (vertexID == 0) {
        result = float4(-0.5f, -0.5f, 0.5f, 1.0f);
    }
    else if (vertexID == 1) {
        result = float4(0.f, 0.5f, 0.5f, 1.0f);
    }
    else {
        result = float4(0.5f, -0.5f, 0.5f, 1.0f);
    }
    return result;
}

float4 PSMain() : SV_TARGET {
    return float4(1.0f, 1.0f, 0.0f, 1.0f);
}