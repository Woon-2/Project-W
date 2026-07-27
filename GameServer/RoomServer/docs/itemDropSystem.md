# 아이템 드롭 & 습득 시스템

몬스터 처치 → 보석 드롭 → 조준 → `F` 습득 → 인벤토리 반영 → DB 영속화.
인벤토리 자체의 권위 모델은 `docs/inventoryPersistence.md`를 먼저 볼 것.

## 1. 권위 경계

| 항목 | 소유 |
|---|---|
| 드롭 여부/개수/종류/**착지점** | **서버** (`Room::spawnGemDrops`) |
| 습득 판정(거리·중복·용량) | **서버** (`Room::pickupItem`) |
| TTL 만료 | **서버** (`Room::updateItemDrops`) |
| 낙하 연출(튐·구름·회전) | **클라** (로컬 Dynamic RigidBody) |
| 조준 감지·강조·프롬프트 | **클라** |

서버는 사망 시점에 착지점을 **1회** 확정해 브로드캐스트하고 그 뒤로는 위치를 갱신하지
않는다. 위치 스트림 패킷이 없고, 습득 판정에 필요한 건 최종 좌표뿐이며, 드롭 물리에는
결정론이 요구되지 않기 때문이다. 서버는 드롭에 **물리 바디를 만들지 않는다**
(`physicsWorld_` 미등록).

클라는 시체 위치에서 착지점을 향해 탄도해가 착지점에서 끝나도록 초기 속도를 역산해
던진다(`v = (target - origin)/T - ½gT`, T ≈ 0.75s). 접촉·감쇠 때문에 정확히 떨어지지는
않으므로, 정지하거나 2.0s가 지나면 물리에서 빼고 권위 착지점으로 0.3s 블렌딩한 뒤
정적 비주얼(느린 회전 + 보빙)로 전환한다. **이 수렴 단계가 클라 물리 오차와 서버 습득
판정이 어긋나는 것을 막는다.** 동시에 물리 비용이 드롭 개수와 무관하게 상한된다.

## 2. dropId — 룸 로컬 id 공간 (IdPool 아님)

`Room::nextDropId_`가 1부터 단조 증가하며(0은 sentinel, 65535에서 1로 랩),
**`IdPool::pop/push`를 절대 호출하지 않는다.**

이유:
- `IdPool`은 프로세스 전역 1..65535이고 `Room::init`이 이미 룸당 242개를 쓴다(아레나 +61).
  킬마다 전역 id를 뽑으면 id 증가율 자체가 달라진다.
- 클라의 `skillObjectById_`는 **서버 id로 직접 색인하는 희소 배열**이다
  (`objectIdLifecycle.md` 참조). 드롭이 그 공간을 쓰면 배열 초과 계열 버그가 재발한다.
- 반납 경로가 있으면 룸 파괴 시 누락 = id 누수(`serverHandoff.md` §3/§4-P0).
  룸 로컬 공간에는 **반납 경로 자체가 없어** 그 클래스 버그에 노출되지 않는다.

클라도 희소 배열이 아니라 `std::unordered_map<uint16, GemDrop>`으로 받는다.

드롭은 `Object`가 아니다 — HP·팩션·AI·피격 BVH가 전부 불필요하다.
`objectById_`에 등록하지 않으며, `barriers_`와 같은 부류의 POD 상태다.

## 3. 패킷 (`ServerEngine/protocol.hpp`, append-only)

| 패킷 | 방향 | 내용 |
|---|---|---|
| `S_ItemDropBatch` | S→C 브로드캐스트 | 킬 1회분 `ItemDropInfo` 배열 (패킷 1개) |
| `C_ItemPickup` | C→S | `{ dropId }` |
| `S_ItemDropRemove` | S→C | `{ dropId, pickerObjId, ItemPickupResult }` |

`S_ItemDropRemove` 하나가 두 역할을 겸한다:
- `PickedUp` / `Expired` → **브로드캐스트**, 클라는 드롭을 제거
- 그 외(`TooFar`/`NotFound`/`InventoryFull`/`NotReady`) → **요청자에게만**, 드롭은 월드에 유지

습득 성공 시 획득자에게는 `S_InventorySnapshot`이 함께 간다(부분 갱신 패킷 대신
전체 스냅샷 — 습득은 여러 슬롯을 건드릴 수 있다).

## 4. "주운 사람이 임자"

소유권 필드도 락도 없다. 한 룸의 모든 `C_ItemPickup`은 그 룸의 **단일 JobQueue에서 직렬
실행**되므로, 먼저 큐에 들어간 요청이 이긴다. 진 쪽은 `NotFound`를 받는다.

## 5. 전부 아니면 전무

`Inventory::add()`에는 롤백이 없다. 부분 습득 상태를 만들지 않으려고
`Inventory::canFit()`(`common/inventory.{hpp,cpp}`)으로 먼저 거르고,
통과했을 때만 `add()`를 호출한다(잔여 0 보장). 실패는 `InventoryFull`.

## 6. 드롭 개수 — 클래스 티어

`Object::rollGemDropCount()` virtual. 티어가 클래스 계층과 정확히 일치하므로
`killChargeReward_`처럼 스폰 시 주입하지 않고 virtual로 둔다.

| 클래스 | 개수 |
|---|---|
| `Object` (거점·플레이어 등 기본) | 0 |
| `Npc` (필드 일반 몬스터) | 1~2 |
| `TacticalNpc` (전술 부대원) | 1~2 |
| `PlatoonLeader` (중간보스 Hobgoblin/Grandbaum/Isys) | 5 |
| `FinalBoss` | 10 |

종류는 6종 **완전 랜덤**, variant(비주얼)도 종류별 메시 개수 내 랜덤.

## 7. 사망 훅

`Room::noteAndMaybeReward`에서 처리한다. **드롭 분기는 `killChargeReward() <= 0.f`
게이트보다 앞에 있다** — charge(ChargeConfig 주입값)와 드롭(클래스 티어)은 독립 보상이라,
charge가 0인 몬스터도 드롭은 해야 하기 때문이다. 이 순서를 되돌리면 조용히 드롭이 사라진다.

`noteAndMaybeReward`는 몬스터 사망 경로 3곳 전부를 덮는다:
`Room::updateSkillSystem`(주 경로), 레거시 평타 2곳(`goblins_`, tactical).

## 8. 알려진 제약 (의도적 비수정)

`elapsedMs_ += dt`가 `updateMonsterAI` 안에 있고 그 함수는 `if (sessions_.empty()) return;`
으로 조기 반환한다 → **빈 룸에서 룸 시계가 멈춰 TTL(180s)도 멈춘다.**
하지만 플레이어가 전부 나가면 룸 자체가 파괴되고 `drops_`는 해제할 자원이 없는 POD
벡터라 실질 영향이 없다. `elapsedMs_` 위치를 옮기는 것은 광범위한 행동 변경이라
이 작업 범위에서 제외했다.

## 9. 클라 조준·강조

- 화면 중앙 `screenToRay` → **습득 반경 안의** 드롭에 대해 `RaycastBVH`.
  놓치면 `worldToScreen` 화면 거리 60px 근접 폴백(에디터 `pickHitbox`와 같은 2단 방식).
- **`kPickupRadius = 2.5f`는 서버(`Room::pickupItem`)와 클라
  (`Game::updateItemDropAim`)에 각각 `constexpr`로 있다. 반드시 같아야 한다** —
  다르면 프롬프트는 뜨는데 서버가 `TooFar`로 거절하는 거짓말 UX가 된다.
- 강조는 신규 `OutlinePipeline`(inverted hull). 설계는
  `client/docs/graphicsArchitecture.md`의 해당 절 참조.
- 프롬프트는 `client/ui/pickupPromptHUD.{hpp,cpp}`. 한국어 문구는 BOM이 있는
  `onlineGame.cpp`에서 조립해 `std::wstring`으로 넘긴다(신규 파일은 ASCII 주석만).

## 10. 아이콘 저작 (1회성)

보석은 3D 메시만 있고 인벤토리 UI 아이콘이 없다. 저작 절차:

1. standalone 클라이언트 실행 → **F10**으로 아이콘 저작 모드 진입.
2. **1~6** = 보석 종류, **`[` / `]`** = variant. 보석은 카메라 앞에 고정되고 천천히 회전한다.
3. 마음에 드는 각도에서 화면을 캡처해 정사각으로 자른다.
4. `resources/UI/texConv.bat`(`texconv -f BC7_UNORM_SRGB`)으로 변환해 저장(git-lfs 대상).
   **파일명은 `resources/data/inventory.json`의 `iconPath`와 정확히 같아야 한다** —
   다르면 로더가 조용히 건너뛰고 폴백 블록만 보인다(경고는 `gSharedLog`에만 남는다).

현재 저작된 아이콘(540×540 BC7, 메시 이름과 동일한 명명):
`ui_gem_BlueCrystal.dds`, `ui_gem_MoonStone.dds`, `ui_gem_Obsidian.dds`,
`ui_gem_GreenObsidian.dds`, `ui_gem_PurpCrystal.dds`, `ui_gem_RedCrystal.dds`.

> `texconv.exe`는 저장소에 없다. `texConv.bat`은 같은 폴더의 `texconv.exe`를 호출하므로
> DirectXTex 배포본에서 받아 해당 폴더에 두어야 한다(다른 모델 폴더의 배치도 동일).

배경이 **불투명**이라 알파 키잉이 필요 없고, 그래서 GPU 리드백 경로를 만들지 않았다.
`.dds`가 아직 없어도 `AssetManager::loadInventoryItemIcons`가 파일 존재를 확인하고
건너뛰므로, 인벤토리 패널은 폴백 블록을 그린다(크래시 없음).
