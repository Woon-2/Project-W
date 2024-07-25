## Graphics

### Mesh
- 리소스 파일을 부분적, 혹은 전체적으로 읽어들이는 기능을 포함 
  - Positions, Normals, Texture Coordinates, Tangents, Bitangents, Bounding Volume, Color, etc.
- 게임에서 지원하는 형태의 Mesh로 변환이 가능한지 쿼리 후 변환해서 사용
  - 예를 들어 Solid Mesh를 지원하려면 Positions와 Color가 로드되어있으면 되고, Phong Mesh with Normal Map을 지원하기 위해선 Positions, Normals, Tangents, Bitangents, Texture Coordinates가 로드되어있으면 됨
- 리소스는 종류별로 버퍼를 구축하고 메모리 덩어리에서 각 버퍼에 접근하기 위한 오프셋을 설정
- 게임에서 지원하는 형태의 Mesh로 변환하면서 각 버퍼를 바탕으로 게임에 맞게 효율적인 메모리 레이아웃으로 재구축 (필요 없는 리소스는 제외)

### Material
- 리소스 파일을 부분적, 혹은 전체적으로 읽어들이는 기능을 포함
  - Texture, Normal Map(Tangent Space/Object Space), etc.
- 절차적 생성을 지원하기 위한 방법 모색 필요

### Renderer
- 사용할 Shader와 Root Signature, 인스턴스 개수 한계 설정

### Pipelining

## Entity Component System