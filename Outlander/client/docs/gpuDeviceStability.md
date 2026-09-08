# GPU 디바이스 안정성 디버깅 노트 — 멀티클라이언트 연쇄 종료/TDR (2026-06)

한 PC에서 클라이언트 2~4개로 멀티플레이를 테스트할 때 "클라이언트 하나를 닫으면
나머지가 전부 종료(또는 먹통)"되던 문제의 전말 기록. 표면 증상은 하나였지만
실제로는 **서로 다른 결함 4겹**이 겹쳐 있었고, 한 겹을 걷어낼 때마다 다음 겹이
드러났다. 같은 부류의 버그가 재발했을 때 진단 시간을 줄이기 위해 남긴다.

## 증상 타임라인

| 단계 | 표면 증상 | 실제 원인 |
|------|-----------|-----------|
| 1 | 한 클라이언트를 닫으면 나머지가 **순차적으로 종료** | `WM_INPUT` 핸들러의 `DISPLAY_ERROR_*( ..., willExit=true )`. 형제 창이 닫히면 포커스 전환으로 합성/깨진 raw input이 다음 창에 도착 → `std::exit(-1)` → 그 창도 닫히며 도미노 |
| 2 | (1 수정 후) 남은 클라이언트가 **moodycamel 큐에서 AV 크래시** | `Font::CreateBitmapFromText`의 `EndDraw()` 실패 → `willExit=true` → `std::exit` → 정적 소멸자들이 임의 순서로 실행되며 `~ServerSession`이 **이미 파괴된** `SendBufferManager::sendBufferChunks_`(다른 TU의 정적 객체)에 SendBuffer 반납 → AV. 진짜 에러(EndDraw 실패)는 가려짐 |
| 3 | (2 수정 후) 남은 클라이언트가 죽지 않고 **먹통** + 콘솔에 `DXGI_ERROR_DEVICE_HUNG`(TDR) | GPU 행 → 어댑터 리셋 → 같은 GPU의 모든 프로세스 디바이스 제거. 종료 측 정리 문제로 추정해 `~Game` 드레인 + vsync를 적용했으나 **미해결** (클라 2개에서도 재현) |
| 4 | 동일 | **진짜 원인**: 닫는 쪽이 아니라 **남은 클라이언트 자신의 폴트**. `S_Leave` → `removePlayer`가 파티 HUD 위젯을 게임 도중 `removeChild`로 즉시 파괴 → `UI::Label`이 소유한 `TextImage`(전용 ID3D12 텍스처+업로드 버퍼+SRV 슬롯)가 즉시 Release → **in-flight 프레임이 그 텍스처를 참조 중** → 디바이스 폴트 → 약 2초 뒤 TDR |

파티 HP HUD 도입 커밋(367ef159) 이전의 `removePlayer`는 GPU 리소스가 없는
ProgressBar만 제거했기 때문에 이 경로가 무해했다. 또한 그 이전에는 1번 결함
(WM_INPUT exit) 때문에 leave 경로가 끝까지 실행된 적 자체가 없어서, 잠복해 있던
4번 결함이 1번을 고치자 비로소 드러났다.

## 수정 요약

| 수정 | 위치 |
|------|------|
| WM_INPUT 3개 검사 비치명화(이벤트만 폐기) | `online/onlineGame.cpp`, `standalone/game.cpp` `receiveWndMsg` |
| 텍스트 렌더링 경로 bool 반환·비치명화, Label 실패 시 dirty 유지로 재시도 | `font.cpp/hpp`, `gfx.cpp/hpp`, `ui/widgets/Label.cpp` |
| `atexit` 훅(런타임 등록은 pre-main 정적 소멸자보다 먼저 실행됨)으로 std::exit 시 세션 선정리 | `ClientApp::init` |
| `GFX::drainGpu()` — 모든 FrameFence+LoadFence 블로킹 대기 | `gfx.cpp/hpp` |
| `~Game` 본문에서 멤버 소멸 전 드레인 | online/standalone `~Game` |
| **`removePlayer`/`createOtherPlayerHud`에서 위젯 파괴 직전 드레인 (핵심 수정)** | `online/onlineGame.cpp` |
| vsync 기본 ON + 설정 토글 (DWM 기아 완화) | `GameSettings`, `GFX::setVsync` |

## 재발 방지 규칙

1. **게임 루프에서 도달 가능한 코드에 `willExit=true` 금지.** 입력·렌더링 실패는
   해당 이벤트/업데이트만 폐기하고 로그를 남긴다. `std::exit`는 이 코드베이스에서
   정적 소멸 순서 AV(2번 결함)까지 동반한다.
2. **게임 도중 GPU 리소스를 소유한 객체(특히 `UI::Label`)를 파괴하기 전에는
   반드시 `gfx_.drainGpu()`를 호출하거나 지연 파괴를 사용한다.** D3D12는 Release
   즉시 메모리를 회수하므로, in-flight 프레임(백버퍼 수만큼)이 참조 중인 리소스를
   해제하면 디바이스 폴트 → TDR로 **같은 GPU의 다른 프로세스까지** 죽는다.
   - 현재 `Label`만 전용 GPU 리소스(`TextImage`)를 소유한다. ProgressBar/Image는
     공유 텍스처 포인터만 들고 있어 안전하다. 새 위젯이 전용 리소스를 갖게 되면
     이 규칙이 그 위젯에도 적용된다.
3. **`Game`의 멤버 선언 순서에 의존하지 말 것.** `gfx_`보다 뒤에 선언된 멤버는
   `~GFX`의 드레인보다 먼저 파괴된다. `~Game` 본문의 `drainGpu()`가 보호하지만,
   새 GPU 리소스 멤버를 추가할 때 이 사실을 인지할 것.
4. 새 정적(static) 객체가 풀/매니저(다른 TU의 정적)에 의존하면 소멸 순서를
   의심할 것. TU 간 정적 소멸 순서는 미정의다.

## 진단 가이드

- 콘솔에 `DXGI_ERROR_DEVICE_HUNG` → TDR. 이후 매 프레임 `D2DERR_RECREATE_TARGET`
  (-2003238900, font.cpp) 로그가 반복되는 것은 정상(텍스트 경로가 비치명화되어
  재시도 중) — 프로세스는 생존한다.
- 콘솔 로그를 보려면 각 클라이언트를 `start cmd /c "client.exe > c1.log 2>&1"`
  식으로 리다이렉트해서 띄운다(콘솔 X로 닫으면 로그가 사라지므로).
- 멀티클라 스트레스 테스트용 개발 머신이라면 TDR 타임아웃 연장(관리자, 재부팅 필요):
  `HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers`에 `TdrDelay`,
  `TdrDdiDelay` (REG_DWORD, 10) 추가.

## 향후 과제

- **UI 위젯 지연 파괴(graveyard)**: 위젯 파괴가 잦아지면(방 나가기, 리스폰 UI 등)
  `drainGpu()` 호출(수십 ms 멈칫) 대신, 트리에서 분리만 하고 프레임 펜스 경과 후
  실제 파괴하는 방식으로 교체.
- **디바이스 손실 복구**: 디바이스/스왑체인/전체 리소스 재생성. 트리거가 제거되어
  우선순위 낮음.
- 무기 선택 클라→서버 배선(현재 서버가 전원 Katana 하드코딩).
