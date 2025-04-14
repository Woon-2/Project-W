struct KeyFrame
{
    float3 translation;
    float3 scale;
    float4 rotation; // quaternion
    float  ratio;    // 보간 비율 (0~1)
};

StructuredBuffer<KeyFrame> keyframePrev : register(t3);
StructuredBuffer<KeyFrame> keyframeNext : register(t4);
RWStructuredBuffer<float4x4> outMatrices : register(u0);

float4 SlerpQuaternion(float4 q1, float4 q2, float t)
{
    q1 = normalize(q1);
    q2 = normalize(q2);

    float dotVal = dot(q1, q2);
    if (dotVal < 0.0f)
    {
        q2 = -q2;
        dotVal = -dotVal;
    }

    const float epsilon = 0.001f;
    if (dotVal > 1.0f - epsilon)
    {
        return normalize(lerp(q1, q2, t));
    }

    float theta = acos(dotVal);
    float sinTheta = sin(theta);
    float w1 = sin((1.0f - t) * theta) / sinTheta;
    float w2 = sin(t * theta) / sinTheta;

    return q1 * w1 + q2 * w2;
}

float4x4 QuaternionToMatrix(float4 q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    return float4x4(
        1 - 2 * (yy + zz), 2 * (xy - wz),     2 * (xz + wy),     0,
        2 * (xy + wz),     1 - 2 * (xx + zz), 2 * (yz - wx),     0,
        2 * (xz - wy),     2 * (yz + wx),     1 - 2 * (xx + yy), 0,
        0,                 0,                 0,                 1
    );
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;

    KeyFrame a = keyframePrev[index];
    KeyFrame b = keyframeNext[index];

    float t = a.ratio;

    float3 T = lerp(a.translation, b.translation, t);
    float3 S = lerp(a.scale, b.scale, t);
    float4 Q = SlerpQuaternion(a.rotation, b.rotation, t);
    float4x4 R = QuaternionToMatrix(Q);

    float4x4 M;
    M[0] = float4(R[0].xyz * S.x, 0);
    M[1] = float4(R[1].xyz * S.y, 0);
    M[2] = float4(R[2].xyz * S.z, 0);
    M[3] = float4(T, 1);

    outMatrices[index] = M;
}