#ifndef __mesh_HPP
#define __mesh_HPP

#include "gfxUtil.hpp"
#include "collision.hpp"

// 재질 정보를 표현하는 구조체
// SubMesh에 담겨, 드로우콜 시 사용할 텍스처나 상수를 지정한다.
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
};

// 바이너리 파일로부터 모델을 읽어온다.
// 메시들을 생성하며 각 메시들의 버텍스 버퍼와 서브메시(인덱스버퍼, 재질)을 생성한다.
// 그 과정에서 필요한 텍스처들이 texHashMap에 존재하지 않는다면, 로드한다.
// (로드되는 텍스처의 경로들은 바이너리 파일 내에 적혀있다.)
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