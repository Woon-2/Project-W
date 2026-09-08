# Commit Message Style

## 형식

```
[type](scope): 제목
```

본문은 선택 사항. 변경 이유나 상세 내역을 bullet로 작성.

---

## Type

| type | 사용 상황 |
|---|---|
| `feat` | 새 기능 추가 |
| `fix` | 버그 수정 |
| `refactor` | 동작 변경 없는 코드 개선 |
| `docs` | 문서만 변경 |
| `build` | 빌드 설정, 인코딩 등 |
| `chore` | 기타 잡무 (리소스 추가 등) |

---

## Scope

작업 대상을 나타낸다. 브랜치명과 일치시키는 것을 권장.

| 예시 | 설명 |
|---|---|
| `client` | 클라이언트 전반 |
| `client_particle` | 파티클 시스템 작업 브랜치 |
| `server` | 서버 전반 |

---

## 규칙

- 제목은 명사형 또는 동사 원형으로 끝낸다 (예: `~구현`, `~수정`, `~최적화`)
- `Co-Authored-By` 트레일러는 사용하지 않는다
- 제목만으로 의도가 충분히 전달되면 본문 생략 가능

---

## 예시

```
[feat](client_particle): 불꽃 파티클 연속 emit 구현
```

```
[fix](client_particle): 파티클이 스카이박스에 가려지는 문제 해결
```

```
[refactor](client_particle): ParticleSystem ring-buffer → compact array + swap-remove 최적화

- pool_ 순회를 4096 전체에서 activeCount_ 개수만으로 축소 (캐시 미스 감소)
- 파티클 만료 시 swap-remove로 O(1) 제거, Particle::active 필드 제거
- render() const 수정, activeCount() accessor 추가
```
