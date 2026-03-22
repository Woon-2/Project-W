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