#ifndef __mesh_HPP
#define __mesh_HPP

#include "gfxUtil.hpp"
#include "collision.hpp"

// 재질 정보를 표현하는 구조체
// 드로우콜 시 사용할 텍스처나 상수를 지정한다.
struct Material {
	Texture mapAlbedo;
	Texture mapMetallicSmoothness;	// 유니티 익스포터를 사용하기 때문에 유니티와 텍스처 포맷 맞춰준다.
									// R 채널에 metallic, A 채널에 Smoothness (1 - roughness) 값이 들어있게 된다.
	Texture mapNormal;
	Texture mapEmmisive;
	Texture mapAmbientOcclusion;

	XMFLOAT4 constantAlbedo;
	float constantRoughness;
	float constantMetallic;
	float constantAOStrength;
	XMFLOAT3 constantEmmisive;
};

struct MaterialSet {
	std::string name;
	std::vector<Material> materials;
};

// 드로우콜 시 사용할 인덱스 버퍼 뷰와 재질 정보를 담는 구조체
// 정점 버퍼는 parentMesh에서 가져와 사용한다.
// 인스턴싱(드로우콜)의 단위가 된다.
// Mesh의 정점 버퍼들을 여러 SubMesh가 공유하면서
// 부분적 렌더링, 재질 분리 교체, 중복 정점 방지 등의 효과가 가능해진다.
struct SubMesh {
	std::string name;
	D3D12_INDEX_BUFFER_VIEW ibView;
};

// 정점 버퍼들과 인덱스 버퍼들, 그리고 재질 정보들을 저장하는 구조체
// 이름을 통해 리소스에 접근할 수 있으며,
// 정점 버퍼들은 IASetVertexBuffers 함수에 효율적으로 쓰이기 위해 직렬화되므로,
// std::vector를 통해 저장하고, 맵은 리소스가 아니라 정점 버퍼에 접근하기 위한
// 인덱스를 얻기 위한 용도로 사용한다.
// 
// * 드로우콜 시에는 Mesh가 아니라 SubMesh를 활용하도록 한다.
struct Mesh {
	std::string name;
	std::vector<ComPtr<ID3D12Resource>> vbs;
	// 파이프라인에 바인드할 용도라면 vbViewsByPipeline 멤버를 사용한다.
	std::vector<D3D12_VERTEX_BUFFER_VIEW> vbViews;
	// 정점 버퍼들은 속성에 따라 {메시 이름}_VB_{정점 속성}의 양식을 갖는 key에 매핑된다.
	// ex) CubeMesh_VB_Position, Vangaurd_Mesh_VB_Normal
	// vbs[ vbViews.at("CubeMesh_VB_Position") ]와 같이 위치 속성에 해당하는 정점 버퍼를 얻는다.
	std::map<std::string, u32t> vbIdxMap;

	// 파이프라인마다 Input Layout이 다르다.
	// 메시의 정점 버퍼들을 바인드할 때에는 vbViewsByPipeline에서 각 파이프라인에 맞는
	// Vertex Buffer View 배열을 쿼리해서 바인드해야 한다.
	// 
	// 각 파이프라인은 특정 메시를 최초로 그릴 때
	// vbViewsByPipeline에서 그 파이프라인에 맞는 Vertex Buffer View 배열을
	// 적절한 key(파이프라인의 이름)와 함께 만든다.
	// (Vertex Buffer View의 인덱스가 해당 정점 속성의 slot 인덱스와 같아야 한다.)
	// 이때 vbViews와 vbIdxMap을 이용한다.
	// 파이프라인의 그리기 함수에는 const Mesh*가 전달되는데,
	// 이 멤버는 그리기 함수에서 최초에 한번 수정되기 때문에 mutable일 필요가 있다.
	mutable std::map<std::string, std::vector<D3D12_VERTEX_BUFFER_VIEW>> vbViewsByPipeline;

	std::vector<ComPtr<ID3D12Resource>> ibs;
	std::vector<SubMesh> subMeshes;
	std::vector<MaterialSet> materialSets;
};

// 1x1x1 큐브 메시를 생성한다.
// @return Mesh
// 메시 로드에 임시 업로드 버퍼들이 사용된다.
// 사용된 업로드 버퍼들은 전달된 펜스에 연관되므로,
// 펜스에서 GPU 작업 완료를 검사한 후 이 업로드 버퍼들을 해제하도록 하자.
Mesh buildCubeMesh(
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	std::unordered_map<std::string, Texture>& texHashMap,
	DescriptorPool& texPool, Fence& fenceToAssociate
);

// 빌보드를 위한 점 한개짜리 메시를 생성한다.
// @return Mesh
Mesh buildPointMesh(
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	std::unordered_map<std::string, Texture>& texHashMap,
	DescriptorPool& texPool, Fence& fenceToAssociate
);

struct Skeleton;

// 애니메이션에 쓰이는 한 개의 본을 나타내는 구조체
// 
// 애니메이션 프레임은 로컬 공간(본 공간)에서의 변환을 표현하므로,
// 드레스 공간에 있는 본을 Bone::toLocal 멤버를 통해 로컬 공간으로 변환한 뒤
// 프레임 변환을 적용해야 올바른 변환이 출력된다.
struct Bone {
	enum class SocketType {
		None,
		RightHand,
		LeftHand,
		SIZE
	};

	mu::Mat4x4 toLocal;	// 드레스 공간에서 로컬 공간으로의 변환
	mu::Mat4x4 toDress;	// 로컬 공간에서 T-pose 드레스 공간으로의 변환
	std::string name;
	std::vector<Bone*> children;	// 이 본을 소유한 Skeleton::bones에 담긴 자식 본들을 가리킨다.
	i32t boneIdx;	// 이 본을 소유한 Skeleton::bones에서 이 본의 인덱스
	SocketType socketType;	// 해당 본이 소켓이 아니라면 None, 소켓이라면 소켓 타입
};

// 바이너리 파일의 Bone Socket Type을 나타내는 문자열로부터
// 실제 열거형 값을 얻어낸다.
Bone::SocketType convertStrToBoneSocketType(const std::string& boneSocketTypeStr);

// 스켈레톤의 구조를 나타내는 열거형,
// 애니메이션 클립이 대상으로 하는 스켈레톤 구조와
// 모델에 부여된 스켈레톤 구조가 같아야만
// 올바른 스키닝이 가능하다.
enum class SkeletonEnumeration {
	Humanoid,
	Unique,
	SIZE
};

// 스키닝의 대상이 되는 스켈레톤을 나타내는 구조체
// 본들과 스켈레톤 구조를 나타내는 열거형 값을 저장한다.
struct Skeleton {
	std::string name;
	std::map<Bone::SocketType, i32t> socketToBoneIdx;
	// 본이 자식 본을 생포인터로 저장하기 위해서는
	// 스켈레톤이 복사되어도 본들의 메모리 공간이 변하지 않아야 한다.
	// std::shared_ptr로 감싸 본들의 저장 공간을 별도의 free store에 구성한다.
	// 이로 인해 부수적으로 Skeleton의 복사도 값싸진다.
	std::shared_ptr< std::vector<Bone> > bones;
	Bone* pRoot;	// 이 멤버를 통해 전체 본 트리를 순회할 수 있다.
	SkeletonEnumeration skeletonEnumeration;
};

// 바이너리 파일의 Skeleton Enumeration을 나타내는 문자열로부터
// 실제 열거형 값을 얻어낸다.
SkeletonEnumeration convertStrToSkeletonEnum(const std::string& skeletonEnumerationStr);

// Model 구조체에서 메시와 드레스 공간 변환을 함께 저장하기 위해 쓰인다.
struct MeshWithDressXform {
	Mesh mesh;
	mu::Mat4x4 dressXform;
};

// 현재 게임 내에서 모델의 계층 구조 내부의 변환을 변경할 일은 없으므로,
// 메시를 그 메시를 메시 로컬 공간에서 드레스 공간으로 변환시켜 주는 변환 행렬과 함께
// std::vector에 저장하여 모델을 표현한다.
struct Model {
	std::string name;
	std::vector<MeshWithDressXform> meshWithDressXforms;
	std::vector<AABB> aabbs;
	std::map<std::string, int> aabbIdxMap;
	std::map<Bone::SocketType, mu::Mat4x4> socketOffsets;
	Skeleton skeleton;
};

// 바이너리 파일로부터 모델을 읽어온다.
// 메시들을 생성하며 각 메시들의 버텍스 버퍼와 서브메시, 그리고 재질 집합을 생성한다.
// 그 과정에서 필요한 텍스처들이 texHashMap에 존재하지 않는다면, 로드한다.
// (로드되는 텍스처의 경로들은 바이너리 파일 내에 적혀있다.)
// 모델에 스켈레톤이 있는 경우, 스켈레톤 정보도 로드한다.
// 모델에 아이템 정보가 있는 경우, 아이템 정보도 로드한다.
// * 수정 시 주의사항: 유니티의 추출 스크립트와 구조가 대칭이어야 한다.
Model loadModelFromFile( const std::filesystem::path& path,
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	std::unordered_map<std::string, Texture>& texHashMap,
	DescriptorPool& texPool, Fence& fenceToAssociate	
);

struct Skybox {
	std::string name;
	Texture texSkybox;
};

// 바이너리 파일로부터 스카이박스를 읽어온다.
// 파일에 적혀있는 큐브맵 텍스처 정보를 통해 스카이박스 재질을 완성한다.
// 이 함수는 무조건 연관된 텍스처를 로드하므로, 중복호출되지 않도록 주의한다.
// * 수정 시 주의사항: 유니티의 추출 스크립트와 구조가 대칭이어야 한다.
Skybox loadSkyboxFromFile( const std::filesystem::path& path,
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	DescriptorPool& texCubePool, Fence& fenceToAssociate
);

#endif	// __mesh_HPP