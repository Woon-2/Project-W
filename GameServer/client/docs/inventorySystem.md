# 인벤토리 시스템 README

이 문서는 현재 첫 번째 인벤토리 구현을 유지보수하거나 아이템을 추가할 때 필요한
구조와 규칙을 설명합니다. 인벤토리는 **고정 슬롯·스택형**이며, 무게·그리드
계산·카테고리 분류는 사용하지 않습니다.

## 목차

- [현재 동작](#현재-동작)
- [코드와 리소스 위치](#코드와-리소스-위치)
- [아이템 추가 방법](#아이템-추가-방법)
- [서버 권한과 프로토콜](#서버-권한과-프로토콜)
- [UI 연결 규칙](#ui-연결-규칙)
- [새로운 사용 효과 추가](#새로운-사용-효과-추가)
- [검증과 빌드](#검증과-빌드)
- [유지보수 시 주의사항](#유지보수-시-주의사항)
- [현재 구현의 범위](#현재-구현의-범위)

## 현재 동작

- 슬롯은 24칸(6열 × 4행)입니다.
- 빈 슬롯은 항상 `itemId = 0`, `quantity = 0`입니다.
- 아이템을 추가하면 낮은 슬롯부터 기존 같은 아이템 스택을 먼저 채운 뒤,
  빈 슬롯을 사용합니다.
- 스택 수량은 아이템별 `maxStack`을 넘을 수 없습니다.
- 인벤토리 변경이 성공할 때마다 `revision`이 1 증가합니다.
- 온라인 인벤토리는 `RoomServer`의 플레이어 객체가 소유하며 접속 세션 동안만
  유지됩니다. 재접속하면 시작 아이템으로 초기화됩니다.
- 시작 아이템은 슬롯 0의 체력 회복 물약 5개입니다.
- 물약 사용은 생존 중이고 체력이 최대치보다 낮을 때만 성공합니다. 성공하면
  최대 체력의 20%를 회복하고 1개를 차감합니다.
- 버리기는 월드 아이템을 생성하지 않고 선택한 스택에서 1개만 차감합니다.

## 코드와 리소스 위치

| 경로 | 역할 |
| --- | --- |
| `common/inventory.hpp/.cpp` | 아이템 정의, 스택, 카탈로그 로드, 인벤토리 모델, 공용 사용/버리기 실행기 |
| `resources/data/inventory.json` | 슬롯 수, 아이템 정의, 시작 슬롯의 단일 원본 |
| `ServerEngine/protocol.hpp` | 인벤토리 패킷 타입과 action/result enum. 기존 값 뒤에만 추가 |
| `RoomServer/object.hpp` | 서버 `Player`가 실제 `Inventory`를 소유 |
| `RoomServer/Room.cpp` | 입장 초기화, 사용/버리기 권한 판정, HP 브로드캐스트 |
| `RoomServer/PacketManager.cpp` | 인벤토리 요청 수신과 스냅샷/result 패킷 생성 |
| `client/PacketManager.cpp` | 서버 스냅샷/result 검증 및 게임 객체 전달 |
| `client/online/onlineGame.cpp` | 온라인 인벤토리 미러, 요청 전송, HP 반영 |
| `client/standalone/game.cpp` | 서버 없이 공용 실행기로 동기 처리 |
| `client/ui/inventoryPanel.hpp/.cpp` | 6×4 패널, 호버 툴팁, 우클릭 메뉴, 입력 차단 |
| `client/AssetManager.cpp` | 패널·버튼 텍스처와 아이템 DDS 아이콘 로드 |
| `resources/UI/ui_potion_0.png` | 물약 아이콘 원본 PNG |
| `resources/UI/ui_potion_0.dds` | 런타임에서 사용하는 DDS |
| `tests/inventoryModelSelfTest.cpp` | 모델·카탈로그·공용 액션 회귀 테스트 |

## 아이템 추가 방법

### 1. 아이콘 준비

런타임 아이콘은 투명 배경의 정사각형 DDS를 사용합니다. 원본 PNG도 함께
보존합니다.

1. `resources/UI/`에 소스 PNG를 추가합니다.
2. 프로젝트의 텍스처 변환 도구로 DDS를 생성합니다.
3. 파일 이름과 JSON의 `key`를 동일한 규칙으로 맞춥니다. 예를 들어
   `ui_potion_1`이면 `ui_potion_1.dds`를 사용합니다.
4. `inventory.json`의 `iconPath`에는 실행 파일 기준 상대 경로를 적습니다.
   현재 규칙은 `../resources/UI/<파일명>.dds`입니다.

아이콘을 찾지 못하면 UI는 점유 슬롯을 숨기지 않고 기본 `HP` 플레이스홀더를
표시합니다. 따라서 아이콘 파일 누락은 치명적이지 않지만, 배포 전에는 경로를
확인해야 합니다.

### 2. `inventory.json`에 정의 추가

```json
{
  "slotCount": 24,
  "items": [
    {
      "id": 2,
      "key": "ui_potion_1",
      "name": "대형 체력 회복 물약",
      "description": "사용하면 최대 체력의 40%를 회복합니다.",
      "iconPath": "../resources/UI/ui_potion_1.dds",
      "maxStack": 10,
      "use": {
        "type": "heal_percent",
        "value": 0.4
      }
    }
  ],
  "starter": [
    {
      "slot": 1,
      "itemId": 2,
      "quantity": 2
    }
  ]
}
```

필수 규칙은 다음과 같습니다.

- `id`는 0이 아니고 모든 아이템 사이에서 유일해야 합니다. 타입은 `uint32`입니다.
- `key`도 유일해야 하며 아이콘 요청 이름으로 사용됩니다.
- `name`, `description`은 UTF-8 문자열입니다. 화면에 표시할 때 공용 UI가
  UTF-8에서 wide string으로 변환합니다.
- `maxStack`은 1 이상이어야 합니다.
- `use.type`은 현재 `heal_percent` 또는 `none`만 허용합니다.
- `heal_percent.value`는 0보다 크고 1 이하인 비율이어야 합니다.
- `starter.slot`은 0부터 `slotCount - 1` 사이여야 하고 중복될 수 없습니다.
- `starter.quantity`는 1 이상이며 해당 아이템의 `maxStack` 이하여야 합니다.
- 시작 아이템을 추가하지 않으려면 `starter` 배열에서만 제외하면 됩니다.

카탈로그가 위 규칙을 위반하면 서버와 클라이언트가 시작 단계에서 명확한 오류를
내고 로드를 거부합니다. JSON을 바꾼 뒤에는 반드시 두 실행 경로가 같은 파일을
읽는지 확인하십시오.

### 3. 아이콘·정의·프로젝트 파일 확인

일반 아이템 추가는 JSON과 리소스만으로 충분합니다. `client.vcxproj`나
`RoomServer.vcxproj`에 아이템별 C++ 파일을 추가하지 마십시오. 공용 모델과
카탈로그가 정의를 동적으로 읽습니다.

## 서버 권한과 프로토콜

온라인 클라이언트는 아이템 수량이나 HP를 직접 확정하지 않습니다.

```text
Client -- C_InventoryAction(revision, slotIndex, action) --> RoomServer
Client <-- S_InventoryActionResult(revision, slotIndex, result, slot) -- Server
Client <-- S_InventorySnapshot(revision, all slots) --------------- Server
```

`C_InventoryAction`에는 아이템 ID, 수량, 회복량이 없습니다. 서버는 자신의
`Player::inventory()`와 카탈로그를 기준으로만 판정합니다.

- 요청의 `revision`이 현재 서버 값과 다르면 `StaleRevision`을 반환하고 전체
  스냅샷을 다시 보냅니다.
- 잘못된 슬롯이나 알 수 없는 아이템은 수량을 변경하지 않고 재동기화합니다.
- 빈 슬롯, 최대 체력, 사망 상태, 사용할 수 없는 아이템에서는 수량을 소비하지
  않습니다.
- 사용 성공으로 HP가 바뀌면 기존 `S_PlayerHp` 패킷을 모든 클라이언트에
  브로드캐스트합니다.
- 패킷 enum과 구조체는 이미 배포된 값과의 호환성을 위해 **기존 항목을
  재정렬하거나 삭제하지 말고 끝에만 추가**해야 합니다.

`Inventory`의 revision은 서버 권위 상태를 식별하는 값입니다. 클라이언트에서
낙관적으로 수량을 줄이거나 revision을 임의로 증가시키면 안 됩니다.

## UI 연결 규칙

`InventoryPanel`은 인벤토리 상태를 소유하지 않습니다. `setInventory()`로 전달받은
상태를 표시하고, 행동이 선택되면 `Callbacks::onAction`만 호출합니다.

- `E`: 열기/닫기
- `Esc`: 닫기
- 슬롯 호버: 이름·설명 툴팁
- 점유 슬롯 우클릭: `사용`, `버리기` 메뉴
- 외부 클릭, 다른 슬롯 선택, 닫기 입력: 메뉴·툴팁 정리
- 온라인 행동: 서버 응답 전에는 표시 수량을 바꾸지 않음
- 패널이 열린 동안: 커서 표시, 이동·카메라·공격·스킬·standalone 에디터 입력 차단
- 월드 업데이트는 계속 진행

새 게임 모드에서 패널을 연결할 때는 다음 순서를 지키십시오.

1. `ItemCatalog`를 로드합니다.
2. `InventoryPanel::build()`에 행동 콜백과 아이콘 resolver를 전달합니다.
3. `setInventory()`로 현재 미러 또는 로컬 인벤토리를 표시합니다.
4. 온라인에서는 콜백에서 `C_InventoryAction`만 전송하고, 응답 핸들러에서
   `applyAuthoritativeSlot()` 또는 `applySnapshot()`을 호출합니다.

메뉴나 툴팁에 새 텍스트를 추가할 때는 [inventoryPanel.cpp](../ui/inventoryPanel.cpp)의
UTF-8 소스 규칙과 `넥슨Lv2고딕` 폰트 사용을 유지해야 합니다. 해당 파일은
프로젝트 설정에서 `/utf-8`로 독립 컴파일됩니다.

## 새로운 사용 효과 추가

새 효과를 단순히 standalone이나 클라이언트 한쪽에만 구현하면 온라인 판정이
어긋납니다. 다음 순서를 따르십시오.

1. `ItemUseKind`에 도메인 효과를 추가합니다.
2. `resources/data/inventory.json`의 `use.type` 파싱과 유효성 검사를 추가합니다.
3. `executeInventoryCommand()`에 서버·standalone 공통 판정을 구현합니다.
4. `Room.cpp`와 standalone의 protocol result 매핑을 확인합니다.
5. 성공 시에만 인벤토리 수량과 revision을 변경합니다.
6. 필요한 경우 새 `InventoryActionResult`를 `protocol.hpp`의 enum 끝에 추가하고,
   온라인 UI의 상태 문구를 연결합니다.
7. 회귀 테스트에 성공·실패·수량 보존·revision 동작을 모두 추가합니다.

현재 `executeInventoryCommand()`는 `Use`와 `DiscardOne`만 처리하며, HP 회복량은
아이템 정의의 비율로 계산하고 정수 HP로 올림한 뒤 최대 HP에서 clamp합니다.

## 검증과 빌드

아이템 JSON이나 공용 모델을 수정한 뒤 다음을 확인합니다.

1. `tests/inventoryModelSelfTest.cpp`를 실행합니다. 현재 테스트는 스택 병합,
   빈 슬롯 배치, 전체 용량 초과, 1개 제거, 잘못된 정의, 오래된 revision,
   사용·버리기 결과를 검증합니다.
2. Visual Studio에서 `RoomServer` Debug x64를 빌드합니다.
3. Visual Studio에서 `client` Debug x64를 빌드합니다.
4. standalone에서 E, 호버, 우클릭, 최대 HP 사용 거부, 부상 후 사용, 버리기,
   E/Esc 입력 복구를 확인합니다.
5. 온라인 두 클라이언트에서 접속 시 물약 5개, 플레이어별 독립 수량, HP 동기화,
   재접속 초기화를 확인합니다.

전체 솔루션 또는 직접 CLI 빌드가 `LobbyServer`, `DummyClient`, `sepch.hpp` 등의
기존 환경 문제로 실패할 수 있습니다. 이 경우 변경된 `RoomServer`와 `client`
타깃 빌드 및 Visual Studio 실행 결과를 우선 판단 기준으로 삼습니다.

## 유지보수 시 주의사항

- `resources/data/inventory.json`은 서버와 클라이언트가 공유하는 단일 정의입니다.
  서버·클라이언트용 JSON을 따로 만들지 마십시오.
- 온라인 UI는 서버 응답 전 수량을 낙관적으로 수정하지 마십시오.
- 아이템 ID를 재사용하거나 기존 ID의 의미를 바꾸면 저장·패킷·로그 해석이
  달라집니다. 이미 배포된 아이템은 새 ID를 사용하십시오.
- protocol enum 순서를 바꾸지 마십시오. 새 값은 끝에만 추가합니다.
- 빈 슬롯은 `itemId`와 `quantity`를 항상 함께 0으로 유지하십시오.
- `Inventory::add()`는 기존 스택 우선이라는 결정적 순서를 전제로 합니다.
  정렬 방식이나 슬롯 순서를 임의로 바꾸면 두 클라이언트의 표시가 달라질 수
  있습니다.
- 현재 인벤토리는 세션 메모리 전용입니다. 저장·로드, 월드 드롭, 교환, 무게,
  카테고리 기능은 구현되어 있지 않습니다.
- `resources/UI/ui_potion_0.png`와 `.dds`처럼 아이콘의 원본과 런타임 산출물을
  함께 보존하십시오.
- `inventoryPanel.cpp`의 한글 문자열은 직접 UTF-8로 작성하며 `\uXXXX` 이스케이프를
  사용하지 않습니다. 폰트 패밀리명은 `넥슨Lv2고딕`입니다.

## 현재 구현의 범위

현재 제공되는 아이템은 다음 한 종류입니다.

| ID | Key | 최대 스택 | 효과 | 시작 수량 |
| ---: | --- | ---: | --- | ---: |
| 1 | `ui_potion_0` | 20 | 최대 HP의 20% 회복 | 슬롯 0에 5개 |

다음 작업자가 아이템 종류를 늘릴 때도 UI를 아이템별로 하드코딩하지 말고,
카탈로그 정의와 공용 실행기·resolver 경로를 확장하는 방식으로 구현해야 합니다.
