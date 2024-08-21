#include "ecs.hpp"

/**
 * @brief This namespace is for ecs ( Entity Component System )
 */

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

	void ConfigEntity()
	{
		for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
		{
			gAvailableEntities.push(entity);
		}
	}

	Entity CreateEntity()
	{
		if (gLivingEntityCount >= MAX_ENTITIES) {
			throw ECS_EXCEPT("You can't create more entities than MAX_ENTITIES.");
		}

		// 큐의 앞에서 부터 번호를 부여한다.
		if (gAvailableEntities.empty()) {
			throw ECS_EXCEPT("Can't create Entity");
		}

		Entity id = gAvailableEntities.front();
		gAvailableEntities.pop();
		++gLivingEntityCount;

		return id;
	}

	void DestroyEntity(const Entity entity)
	{
		if (entity >= MAX_ENTITIES)
		{
			throw ECS_EXCEPT("Entity out of Range");
		}

		// std::bitset::reset
		gSignature[entity].reset();

		// 제거한 아이디는 큐의 맨뒤로 보낸다.
		gAvailableEntities.push(entity);
		--gLivingEntityCount;
	}

	void SetSignature(Entity entity, Signature signature)
	{
		if (entity >= MAX_ENTITIES)
		{
			throw ECS_EXCEPT("Entity out of Range");
		}

		gSignature[entity] = signature;
	}

	Signature GetSignature(Entity entity)
	{
		if (entity >= MAX_ENTITIES)
		{
			throw ECS_EXCEPT("Entity out of Range");
		}

		return gSignature[entity];
	}

	void componentDestroyEntity(Entity entity)
	{
		for (auto const& pair : gComponentArrays)
		{
			auto const& component = pair.second;

			component->EntityDestroyed(entity);
		}
	}

	void systemDestroyEntity(Entity entity)
	{
		for (auto const& pair : gSystems)
		{
			auto const& system = pair.second;

			system->entites_.erase(entity);
		}
	}

	void SetEntity(std::string sysName, Entity entity)
	{
		auto entitySignature = GetSignature(entity);

		if ((entitySignature & gSystemSignature[sysName]) == gSystemSignature[sysName])
		{
			gSystems[sysName]->entites_.insert(entity);
		}
	}

	void EntitySignatureChanged(Entity entity)
	{
		auto entitySignature = GetSignature(entity);

		for (auto const& pair : gSystems)
		{
			auto const& type = pair.first;
			auto const& system = pair.second;
			auto const& systemSignature = gSystemSignature[type];

			if ((entitySignature & systemSignature) == systemSignature)
			{
				system->entites_.insert(entity);
			}
			else
			{
				system->entites_.erase(entity);
			}
		}
	}
}