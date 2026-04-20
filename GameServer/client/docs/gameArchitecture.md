### 게임 아키텍처
- `online/onlineGame.hpp` — 네트워크 모드
- `standalone/game.hpp` — 싱글 플레이어 모드

---

### 게임 루프 (StandAlone::Game::update)

매 프레임 아래 순서로 실행된다.

```
1. 평가 물리량(evVelocity, evOmega) 초기화
2. processInput(dt)        — 키보드/마우스 입력 처리, LButton 시 combatSystem_.onPlayerAttack()
3. 평가 물리량 갱신
4. combatSystem_.update()  — 몬스터 AI 공격 판정 (쿨타임 + AABB 교차)
5. 이벤트 처리 루프        — EventList 순회, Hit/Death/Attack/Blood 분배
6. PhysicsWorld::step()    — 고정 주기(60Hz) 물리 시뮬레이션
7. Object::update()        — viewFrustumCulled || hiZCulled_ 이면 조기 반환; 아니면 PhysicsState 보간 → RenderState 갱신
8. AnimSystem::update()    — 애니메이션 스케줄링 (culled 오브젝트 스킵)
9. Game::render():
   a. cullObjects()         — view frustum culling → setFrustumCulled (frustum culled만 DrawEvent 차단)
   b. Object::render() 호출 — frustum culled 제외, Hi-Z culled는 DrawEvent 계속 제출 (self-reinforcing 방지)
   c. GFX::render()         — Hi-Z 5단계 compute + indirect draw + visibleFlags readback 복사 포함
   d. applyHiZCulling()     — 이전 프레임 readback → setHiZCulled + AnimBlender::setCulled 갱신
                               (다음 프레임 7번 Object::update에 반영됨)
```

**컬링 시스템 설계 원칙:**
- `viewFrustumCulled`: DrawEvent 제출 차단 전용. Hi-Z와 무관.
- `hiZCulled_`: Object::update()/AnimBlender 연산 스킵 전용. 1-frame delay.
- `AnimBlender::setCulled(frustum || !hiZVisible)`: 두 플래그를 통합해 AnimSystem 스킵 조건으로 사용.
- `applyHiZCulling()` 한 곳에서만 animBlender 동기화 (`cullObjects()`에서 setCulled 직접 호출 금지).

---

### 이벤트 시스템
파일: `event.hpp`

이벤트는 크기별 오브젝트 풀에서 할당되어 `EventList(std::list<char*>)`에 저장된다. 순회 중 삽입이 안전하다 (list 반복자 무효화 없음).

**이벤트 타입:**
| 이벤트 | 구조체 | 주요 필드 | 용도 |
|--------|--------|-----------|------|
| `Hit` | `EvHit` | targetId, hp | 피격 — 대상 HP 갱신 + 피격 애니메이션 |
| `Death` | `EvDeath` | victimId | 사망 — 사망 애니메이션, playerDead_ 플래그 |
| `Attack` | `EvAttack` | attackerId | 공격 애니메이션 트리거 |
| `Blood` | `EvBlood` | victimId | 피 이펙트 스프라이트 재생 |

**흐름 예시 — 몬스터가 플레이어를 공격하는 경우:**
```
combatSystem_.update()
  → EvAttack(monsterId)   : 몬스터 공격 애니메이션
  → EvHit(playerId, hp)   : 플레이어 HP 감소 + 피격 애니메이션 + EvBlood
  → (hp == 0) EvDeath(playerId) : 사망 애니메이션, playerDead_ = true
```

**이벤트 버스:**
- 각 Object는 `IEventBus* eventBus()`를 제공
- AnimBlender도 별도 `IEventBus`를 가져 애니메이션 상태를 갱신
- Object::EventBus → AnimBlender::EventBus 순으로 전파

---

### CombatSystem (standalone)
파일: `standalone/combatSystem.hpp`, `standalone/combatSystem.cpp`

전투 로직을 Game 클래스로부터 분리한 서브시스템. Game은 등록/호출만 담당한다.

**주요 인터페이스:**
- `registerCombatant(Object*, CombatConfig)` — 전투 참가자 등록. `obj->getId()`를 키로 사용
- `onPlayerAttack(playerId, evList)` — 플레이어 능동 공격. forward 방향 AABB hitbox 생성 후 등록된 몬스터와 교차 검사 → EvHit 발생
- `update(dt, playerId, evList)` — 매 프레임. 몬스터 AI 쿨타임 감소 및 플레이어와 교차 시 EvAttack + EvHit 발생

**CombatConfig 파라미터:**
- `attackHalfExtent` — 공격 hitbox AABB 반크기 (x=좌우, y=상하, z=전후)
- `attackOffsetFwd` — hitbox 중심을 전방으로 밀 거리
- `damage` — 한 번 공격으로 줄 데미지
- `cooldown` — 공격 쿨타임 (몬스터 AI용; 플레이어는 입력으로 제어)

**몬스터별 기본 설정값:**
| 몬스터 | halfExtent | offsetFwd | damage | cooldown |
|--------|-----------|-----------|--------|----------|
| Goblin | 1.2/1.5/1.2 | 0.8m | 15 | 2000ms |
| Anubis | 1.5/2.0/1.5 | 1.0m | 25 | 3000ms |
| Bat | 0.8/0.8/0.8 | 0.6m | 10 | 1500ms |
| Bomber | 1.5/1.5/1.5 | 1.2m | 30 | 4000ms |
| Demon | 1.5/2.0/1.5 | 1.0m | 20 | 2500ms |
| Dragon | 2.5/2.0/2.5 | 1.5m | 40 | 5000ms |
| Eyeball | 1.2/1.2/1.2 | 1.0m | 15 | 2000ms |
| Fishman | 1.2/1.8/1.2 | 0.9m | 20 | 2500ms |
| Gargoyle | 1.5/2.0/1.5 | 1.0m | 25 | 3000ms |

**향후 확장 포인트:**
- OBB 충돌체 지원: `overlapsAny`를 가상화하거나 충돌체 타입 추상화
- BVH broadphase: 등록된 참가자를 공간 분할 자료구조로 관리
- 다수 공격 충돌체: 무기별·파트별 여러 hitbox 지원
