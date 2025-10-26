#ifndef __mesh_HPP
#define __mesh_HPP

#include "pch.hpp"

// 드로우콜 시 사용할 인덱스 버퍼 뷰와 재질 정보를 담는 구조체
// 정점 버퍼는 parentMesh에서 가져와 사용한다.
// 인스턴싱(드로우콜)의 단위가 된다.
// Mesh의 정점 버퍼들을 여러 SubMesh가 공유하면서
// 부분적 렌더링, 재질 분리 교체, 중복 정점 방지 등의 효과가 가능해진다.
struct SubMesh {
	std::wstring name;
	D3D12_INDEX_BUFFER_VIEW ibView;
	u32t material;
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
	std::map<std::wstring, u32t> vbIdxMap;

	std::map<std::wstring, ComPtr<ID3D12Resource>> ibs;
	std::map<std::wstring, SubMesh> subMeshes;
};

// 1x1x1 큐브 메시를 생성한다.
// @return std::pair<Mesh, std::vector<ComPtr<ID3D12Resource>>
//     생성된 메시와, 메시를 생성하는데 사용된 업로드 버퍼들의 벡터,
//     Fence 객체와 연관시키는 등으로 업로드 버퍼들이 적절한 수명을 갖도록 하자.
std::pair<Mesh, std::vector<ComPtr<ID3D12Resource>>> buildCubeMesh(
	ID3D12Device* device, ID3D12GraphicsCommandList* cmdList
);

#endif	// __mesh_HPP