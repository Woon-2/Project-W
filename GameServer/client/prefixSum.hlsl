StructuredBuffer<uint> gInput : register(t3);
RWStructuredBuffer<uint> gOut : register(u1);

groupshared uint waveSums[64]; 

[numthreads(64, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID,
                 uint3 gtid : SV_GroupThreadID)
{
    uint idx = tid.x;
    uint lane = WaveGetLaneIndex();
    uint laneCount = WaveGetLaneCount();
    uint waveID = gtid.x / laneCount;
    uint numWaves = (64 + laneCount - 1) / laneCount; // 실제 생성된 Wave 개수

    uint x = gInput[idx];

    // 1. Wave-level Exclusive Sum
    uint exclusive = WavePrefixSum(x);
    uint inclusive = exclusive + x;

    // 2. 각 Wave의 합을 기록
    if (lane == laneCount - 1)
    {
        waveSums[waveID] = inclusive;
    }

    GroupMemoryBarrierWithGroupSync();

    // 3. 첫 번째 Wave가 각 Wave의 시작 Offset을 계산
    if (waveID == 0)
    {
        // 유효한 waveID 범위 내의 값만 가져옴
        uint val = (gtid.x < numWaves) ? waveSums[gtid.x] : 0;
        
        uint waveExclusive = WavePrefixSum(val);
        
        // 결과 저장 (자신이 담당하는 waveSums 인덱스에 저장)
        if (gtid.x < numWaves)
        {
            waveSums[gtid.x] = waveExclusive;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // 4. 최종 Exclusive Prefix Sum: 내 Wave 내 위치 + 내 Wave의 시작 Offset
    uint offset = exclusive + waveSums[waveID];

    gOut[idx] = offset;
}