#include "ecs.hpp"

namespace ecs {

	// 각 Entity에 해당할 시그니처 배열
	std::array<Signature, MAX_ENTITIES> gSignature;

	// 사용하지 않은 엔티티 아이디
	std::queue<Entity> gAvailableEntities;
	uint32_t gLivingEntityCount{};

	// 타입 이름과 컴포넌트 타입을 매핑 ComponentType은 int임 gNextComponentType으로 인덱싱
	std::map<std::string, ComponentType> gComponentTypes{};
	// 타입 이름과 해당 타입의 배열(엔터티 배열을 가지고 있는)을 매핑 
	std::map<std::string, std::shared_ptr<IComponentArray>> gComponentArrays{};
	// 등록된 컴포넌트 정보
	ComponentType gNextComponentType{};

	std::map<std::string, Signature> gSystemSignature{};
	std::map<std::string, std::shared_ptr<System>> gSystems{};

	namespace {
		void internalFunction() {}
	}

	void foo() {
		internalFunction();
	}

}