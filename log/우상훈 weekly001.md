## 기록

2024-09-28

- Reader-Writer Lock
  - 비트 플래그를 활용하여 lock을 걸고 해제한다.
  - atomic 타입의 compare and swap 동작으로 lock 경합을 제어한다.
- DeadLock 탐지
  - 핵심 내용은 DFS(Depth-First Search) 알고리즘을 활용한다.
  
## 노트

- DeadLock을 탐지할 때, map에 몇 번 노드가 몇 번 노드에 대한 lock을 잡는지 history를 기록하고, DFS 알고리즘을 사용해서 모든 노드를 순회하며 역방향 간선인 경우를 찾는다.