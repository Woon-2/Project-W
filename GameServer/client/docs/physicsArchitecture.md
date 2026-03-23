### 물리 아키텍처
- `standalone/physics.hpp`, `collision.hpp`, `object.hpp`가 연관되었음

`PhysicSystem` - 물리 시뮬레이션을 총괄 책임지는 클래스
`PhysicSystem::step` 함수에 의해서 물리 시뮬레이션이 실행됨

`PhysicSystem::step`의 내부적 4단계
- `integrate`: 속도, 각속도 등 적분 + 각 오브젝트 BVH 재빌드
- `broadPhase`: 충돌 후보군 계산 (성능을 위함)
- `narrowPhase`: 충돌 후보군 중 실제 충돌 대상 식별
- `solveCollisions`: 충돌 해소 후 BVH 재빌드

물리 시뮬레이션은 `Object`를 상속하는 클래스 객체들을 대상으로 수행
각 객체들의 `PhysicState`를 갱신

성능 및 안정성을 위해 물리 시뮬레이션은 고정 간격으로 실행

### 충돌체 타입 (CollisionVolume = BVH)
파일: `collision.hpp`

```cpp
using CollisionVolume = BVH;
```

`CollisionVolume`은 더 이상 `std::variant<AABB, OBB>`의 flat list가 아니라 **BVH(Bounding Volume Hierarchy)** 트리다.

#### BVH 구조

```cpp
struct BVHNode {
    AABB                    bounds;      // 이 서브트리를 감싸는 AABB (fast reject)
    std::variant<AABB, OBB> shape;       // 실제 충돌 도형 (모든 레벨)
    std::vector<int>        children;    // 자식 인덱스 목록 (비어 있으면 리프)
    std::string             name;        // 디버그/식별용 박스 이름
    std::string             boneName;    // 로드 시 본 인덱스 해소에만 사용; 런타임 불필요
    int                     boneIdx = -1; // 해소된 본 인덱스; -1 = 루트 변환만 적용
    bool isLeaf() const { return children.empty(); }
};

struct BVH {
    std::vector<BVHNode> nodes;  // nodes[0] = root; 비어 있으면 충돌 없음
    bool empty() const { return nodes.empty(); }
};
```

- 최대 3레벨 N-ary 트리 (LOD 0 → LOD 1 → LOD 2)
- 모든 노드(내부 노드 포함)가 `AABB` 또는 `OBB` shape을 가진다
- `bounds`는 자식 전체를 감싸는 AABB로, fast reject에 사용된다
- Unity의 `MultiBoundingVolume` 컴포넌트의 LOD 계층 구조를 그대로 반영

#### LOD → BVH 변환 (`buildBVHFromLODs`, `mesh.cpp`)

1. LOD 0의 박스 → root 노드 (nodes[0])
2. LOD 1 박스들 → root의 children (기하적 포함 or 최근접 LOD 0 박스에 연결)
3. LOD 2 박스들 → 가장 가까운 LOD 1 노드의 children에 연결
4. 내부 노드 bounds = children bounds들의 union AABB (bottom-up)
5. LODCount == 0 → 빈 BVH

shape 결정: `rotationEuler == (0,0,0)` → `AABB`, 그 외 → `OBB`

#### 본(Bone) 연결 및 임포트 시 인덱스 해소

각 BVHNode는 Unity에서 특정 본에 종속되어 있으며 `boneName` 필드로 저장된다.
바이너리 로드 순서상 BVH 임포트가 스켈레톤 임포트보다 먼저 일어나므로,
스켈레톤 임포트 직후 `resolveBVHBoneIndices()`로 이름→인덱스 해소를 별도 패스로 수행한다.

```
importBoundingVolumes()   // BVH 로드 (boneName 저장)
importSkeleton()          // 스켈레톤 로드
resolveBVHBoneIndices()   // boneName → boneIdx 해소
```

런타임에는 `boneIdx`만 참조하며 `boneName`은 불필요하다.

### BVH 재빌드 시점
파일: `object.cpp` — `Object::rebuildBVH(PhysicState&)`

- `setModel()`, `setPos()`, `setCurrPos()`, `setScale()` 호출 시
- `setOrient()` 호출 시
- `PhysicSystem::integrate()` 단계 후
- `PhysicSystem::solveCollisions()` 단계 후

#### 본 연결 노드의 월드 변환 체인

mathUtil은 DirectXMath 규약을 따른다: 변환 적용 순서 A→B→C = 행렬 곱셈 순서 A*B*C.

`bone.toDress`: bone 로컬 공간 → dress(모델 루트 로컬) 공간
`finalXformData()[i]`: dress 공간 기준 애니메이션 변환 (내부적으로 toLocal * 누적 로컬 애니메이션)
`objWorld`: dress 공간 → 월드 공간

**결합 체인**: `boneToWorld = bone.toDress * finalXformData()[i] * objWorld`
- `Vec4(localCenter, 1.f) * boneToWorld` → 월드 공간 중심점
- halfExtents는 오브젝트 루트 scale만 적용 (bone 변환은 rigid 가정)
- `quatRotMat(boneToWorld.get())` → 월드 회전 쿼터니언

이 체인은 장비 소켓 렌더링과 동일하다 (`object.cpp` — `Object::update()` 내 equipment render 참조).

`boneIdx == -1` 또는 `animBlender`가 없으면 기존 루트 변환(pos + scale + orient)으로 fallback.

### 공격 충돌체 (Attack Hitbox)
파일: `collision.hpp`, `collision.cpp`

`buildAttackAABB(pos, forward, halfExtent, offsetFwd)` 함수로 공격 hitbox를 생성한다.
공격 hitbox는 AABB로 유지된다.

공격 충돌 판정은 PhysicSystem과 완전히 분리된 CombatSystem에서 수행된다.
- `PhysicSystem`: 지형/오브젝트 간 물리 충돌 (MTV 기반 관통 해소)
- `CombatSystem`: 공격 hitbox(AABB) ↔ 대상 BVH 트리 교차 판정 (데미지 이벤트 발생)

### 충돌 검사 함수
파일: `collision.hpp`, `collision.cpp`

- `collides(AABB, AABB)` → 3축 SAT
- `collides(OBB, OBB)` → 15축 SAT (face normals A×3 + face normals B×3 + edge cross products×9)
- `collides(BVH, BVH)` → dual-tree traversal (스택 기반 DFS, bounds AABB fast reject → shape 정확 판정)
- `collides(BVH, AABB)` → 스택 기반 DFS, 히트 즉시 return (combat용)
- `obbToAABB(OBB)` → OBB 8개 꼭짓점에서 min/max AABB 계산

#### BVH 트리 순회 알고리즘

**`collides(BVH, AABB)` (combat용)**:
```
stack에 root 인덱스 push
반복: nodeIdx pop
  bounds vs hitbox AABB miss → skip (early out)
  shape vs toOBB(hitbox) 정확 판정 → hit이면 즉시 return hit
  children들을 stack에 push
```

**`collides(BVH, BVH)` (physics용, dual-tree traversal)**:
```
stack에 (idxA=0, idxB=0) 페어 push
반복: 페어 pop
  두 bounds AABB 교차 안하면 skip
  두 shape 모두 정확 판정: hit이면 즉시 return hit
  양쪽 모두 리프면 판정 완료
  자식 있는 쪽을 펼쳐서 (자식 × 상대방) 페어를 stack에 push
  양쪽 다 자식 있으면 더 큰 bounds 쪽을 펼침
```
