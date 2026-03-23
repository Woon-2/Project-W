### 물리 아키텍처
- `standalone/physics.hpp`, `collision.hpp`, `object.hpp`가 연관되었음

`PhysicSystem` - 물리 시뮬레이션을 총괄 책임지는 클래스
`PhysicSystem::step` 함수에 의해서 물리 시뮬레이션이 실행됨

`PhysicSystem::step`의 내부적 4단계
- `integrate`: 속도, 각속도 등 적분
- `broadPhase`: 충돌 후보군 계산 (성능을 위함)
- `narrowPhase`: 충돌 후보군 중 실제 충돌 대상 식별
- `solveCollisions`: 충돌 해소

물리 시뮬레이션은 `Object`를 상속하는 클래스 객체들을 대상으로 수행
각 객체들의 `PhysicState`를 갱신

성능 및 안정성을 위해 물리 시뮬레이션은 고정 간격으로 실행

### 공격 충돌체 (Attack Hitbox)
파일: `collision.hpp`, `collision.cpp`

`buildAttackAABB(pos, forward, halfExtent, offsetFwd)` 함수로 공격 hitbox를 생성한다.
공격자의 위치에서 forward 방향으로 offsetFwd만큼 이동한 위치에 AABB를 배치한다.

공격 충돌 판정은 PhysicSystem과 완전히 분리된 CombatSystem에서 수행된다.
- `PhysicSystem`: 지형/오브젝트 간 물리 충돌 (MTV 기반 관통 해소)
- `CombatSystem`: 공격 hitbox ↔ 대상 AABB 교차 판정 (데미지 이벤트 발생)

**현재 충돌체 타입:** AABB (`collides` 함수 사용)

**향후 확장 포인트:**
- OBB 충돌체: `buildAttackOBB` 함수 추가, `overlapsAny`에서 OBB-AABB 교차 판정 지원
- BVH (Bounding Volume Hierarchy): 다수 오브젝트 환경에서 broadphase 성능 개선