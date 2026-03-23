### 물리 아키텍처
- `standalone/physics.hpp`, `collision.hpp`, `object.hpp`가 연관되었음

`PhysicSystem` - 물리 시뮬레이션을 총괄 책임지는 클래스
`PhysicSystem::step` 함수에 의해서 물리 시뮬레이션이 실행됨

`PhysicSystem::step`의 내부적 4단계
- `integrate`: 속도, 각속도 등 적분 + 각 오브젝트 volumes 재빌드
- `broadPhase`: 충돌 후보군 계산 (성능을 위함)
- `narrowPhase`: 충돌 후보군 중 실제 충돌 대상 식별
- `solveCollisions`: 충돌 해소 후 volumes 재빌드

물리 시뮬레이션은 `Object`를 상속하는 클래스 객체들을 대상으로 수행
각 객체들의 `PhysicState`를 갱신

성능 및 안정성을 위해 물리 시뮬레이션은 고정 간격으로 실행

### 충돌체 타입 (CollisionVolume)
파일: `collision.hpp`

```cpp
using CollisionVolume = std::variant<AABB, OBB>;
```

- `AABB`: 축 정렬 박스. 단순 사물(환경, 지형)에 사용. pos + scale만 적용.
- `OBB`: 방향 박스. 캐릭터(플레이어, 몬스터)에 사용. pos + scale + orient 적용.

충돌체 타입은 **모델 바이너리 리소스의 `<Type>` 태그**로 결정된다.
`importBoundingVolume()`이 타입을 읽어 AABB 또는 OBB를 `model.volumes`에 저장한다.
Object 측에 별도 플래그 없음 — 타입은 `std::holds_alternative<OBB>(...)`로 확인.

### volumes 재빌드 시점
파일: `object.cpp` — `Object::rebuildVolumes(PhysicState&)`

- `setModel()`, `setPos()`, `setCurrPos()`, `setScale()` 호출 시
- `setOrient()` 호출 시: 모델에 OBB volume이 하나라도 있으면 재빌드
- `PhysicSystem::integrate()` 단계 후
- `PhysicSystem::solveCollisions()` 단계 후

### 공격 충돌체 (Attack Hitbox)
파일: `collision.hpp`, `collision.cpp`

`buildAttackAABB(pos, forward, halfExtent, offsetFwd)` 함수로 공격 hitbox를 생성한다.
공격 hitbox는 AABB로 유지된다.

공격 충돌 판정은 PhysicSystem과 완전히 분리된 CombatSystem에서 수행된다.
- `PhysicSystem`: 지형/오브젝트 간 물리 충돌 (MTV 기반 관통 해소)
- `CombatSystem`: 공격 hitbox(AABB) ↔ 대상 volumes(AABB/OBB) 교차 판정 (데미지 이벤트 발생)
  - `collides(CollisionVolume, CollisionVolume)`으로 cross-type dispatch 처리

### 충돌 검사 함수
파일: `collision.hpp`, `collision.cpp`

- `collides(AABB, AABB)` → 3축 SAT
- `collides(OBB, OBB)` → 15축 SAT (3 face normals A + 3 face normals B + 9 edge cross products)
- `collides(CollisionVolume, CollisionVolume)` → `std::visit`로 타입 dispatch; cross-type은 `toOBB()`로 AABB를 OBB로 변환 후 OBB-OBB 처리

**향후 확장 포인트:**
- BVH (Bounding Volume Hierarchy): 다수 오브젝트 환경에서 broadphase 성능 개선
