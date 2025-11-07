#ifndef __mesh_HPP
#define __mesh_HPP

#include "pch.hpp"
#include "gfxUtil.hpp"

// 재질 정보를 표현하는 구조체
// SubMesh에 담겨, 드로우콜 시 사용할 텍스처나 상수를 지정한다.
struct Material {
	Texture mapAlbedo;
	Texture mapMetallicSmoothness;	// 유니티 익스포터를 사용하기 때문에 유니티와 텍스처 포맷 맞춰준다.
									// R 채널에 metallic, A 채널에 Smoothness (1 - roughness) 값이 들어있게 된다.

	XMFLOAT4 constantAlbedo;
	float constantRoughness;
	float constantMetallic;
	float constantAmbientOcllusion;
	XMFLOAT3 constantEmmisive;
};

// 드로우콜 시 사용할 인덱스 버퍼 뷰와 재질 정보를 담는 구조체
// 정점 버퍼는 parentMesh에서 가져와 사용한다.
// 인스턴싱(드로우콜)의 단위가 된다.
// Mesh의 정점 버퍼들을 여러 SubMesh가 공유하면서
// 부분적 렌더링, 재질 분리 교체, 중복 정점 방지 등의 효과가 가능해진다.
struct SubMesh {
	std::string name;
	D3D12_INDEX_BUFFER_VIEW ibView;
	Material material;
};

// 정점 버퍼들과 인덱스 버퍼들, 그리고 재질 정보들을 저장하는 구조체
// 이름을 통해 리소스에 접근할 수 있으며,
// 정점 버퍼들은 IASetVertexBuffers 함수에 효율적으로 쓰이기 위해 직렬화되므로,
// std::vector를 통해 저장하고, 맵은 리소스가 아니라 정점 버퍼에 접근하기 위한
// 인덱스를 얻기 위한 용도로 사용한다.
// 
// * 드로우콜 시에는 Mesh가 아니라 SubMesh를 활용하도록 한다.
struct Mesh {
	std::vector<ComPtr<ID3D12Resource>> vbs;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> vbViews;
	std::map<std::string, u32t> vbIdxMap;

	std::map<std::string, ComPtr<ID3D12Resource>> ibs;
	std::map<std::string, SubMesh> subMeshes;
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

// Model 구조체에서 메시와 드레스 공간 변환을 함께 저장하기 위해 쓰인다.
struct MeshWithDressXform {
	Mesh mesh;
	mu::Mat4x4 dressXform;
};

// 현재 게임 내에서 모델의 계층 구조 내부의 변환을 변경할 일은 없으므로,
// 메시를 그 메시를 메시 로컬 공간에서 드레스 공간으로 변환시켜 주는 변환 행렬과 함께
// std::vector에 저장하여 모델을 표현한다.
struct Model {
	std::vector<MeshWithDressXform> meshWithDressXforms;
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

#endif	// __mesh_HPP