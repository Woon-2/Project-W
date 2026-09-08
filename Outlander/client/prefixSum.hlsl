// 그룹 오프셋(exclusive prefix sum) 계산.
//   gInput[i] = 그룹 i의 visible 인스턴스 수
//   gOut[i]   = 그룹 i가 visibleIndices 버퍼에서 차지할 시작 오프셋(exclusive scan)
//
// 단일 스레드그룹으로 Dispatch(1,1,1) 하고, 그룹 수가 GROUP_SIZE를 넘으면 chunk 단위로
// 순회하며 직전 chunk까지의 누적합(base)을 이어받는다. → groupCnt 에 상한이 없다.
//
// (구버전은 스레드그룹 *내부* 스캔만 수행하고 Dispatch((groupCnt+63)/64,1,1) 로 여러
//  스레드그룹을 띄웠는데, 스레드그룹 간 누적 패스가 없어 그룹 수가 64를 넘으면 64번째
//  이후 그룹의 오프셋이 0부터 다시 시작했다. 그 결과 visibleIndices 영역이 겹쳐 indirect
//  draw가 다른 오브젝트의 인스턴스 인덱스를 읽어 본/메시가 뒤섞이는 버그가 있었다.)

StructuredBuffer<uint>   gInput : register(t3);
RWStructuredBuffer<uint> gOut   : register(u1);

cbuffer PerFrameData : register(b1) {
    uint groupCnt;
    uint3 padding1;
};

#define GROUP_SIZE 256
groupshared uint sdata[GROUP_SIZE];

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 gtid : SV_GroupThreadID)
{
    const uint tid = gtid.x;
    uint base = 0u;   // 직전 chunk 까지의 누적합 (모든 스레드 동일 값)

    for (uint chunkStart = 0u; chunkStart < groupCnt; chunkStart += GROUP_SIZE)
    {
        const uint i = chunkStart + tid;
        const uint x = (i < groupCnt) ? gInput[i] : 0u;

        sdata[tid] = x;
        GroupMemoryBarrierWithGroupSync();

        // Hillis-Steele inclusive scan (chunk 내부)
        [unroll]
        for (uint offset = 1u; offset < GROUP_SIZE; offset <<= 1u)
        {
            const uint v = (tid >= offset) ? sdata[tid - offset] : 0u;
            GroupMemoryBarrierWithGroupSync();
            sdata[tid] += v;
            GroupMemoryBarrierWithGroupSync();
        }

        // inclusive → exclusive 변환 후 직전 chunk 까지의 base 를 더해 기록
        if (i < groupCnt)
            gOut[i] = base + (sdata[tid] - x);

        // 이 chunk 전체 합을 base 에 누적 (마지막 슬롯 = 패딩 0 포함 inclusive 합)
        base += sdata[GROUP_SIZE - 1u];
        GroupMemoryBarrierWithGroupSync();
    }
}
