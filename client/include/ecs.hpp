#ifndef __ECS_HPP
#define __ECS_HPP

#include <iostream>
#include <bitset>
#include <queue>
#include <array>
#include <map>
#include <string>
#include <set>
#include <memory>
#include <cassert>

#include "ecsExcept.hpp"

namespace ecs {

	using Entity = std::int32_t;
	using ComponentType = std::uint8_t;

	const Entity MAX_ENTITIES = 1024;
	const ComponentType MAX_COMPONENTS = 64;

	using Signature = std::bitset<MAX_COMPONENTS>;

	// 각 Entity에 해당할 시그니처 배열
	extern std::array<Signature, MAX_ENTITIES> gSignature;

	// 사용하지 않은 엔티티 아이디
	extern std::queue<Entity> gAvailableEntities;
	extern uint32_t gLivingEntityCount;

	void ConfigEntity();
	// 새로운 엔터티를 만듭니다.
	Entity CreateEntity();
	void DestroyEntity(const Entity entity);

	void SetSignature(Entity entity, Signature signature);

	Signature GetSignature(Entity entity);

	class IComponentArray
	{
	public:
		virtual ~IComponentArray() = default;
		virtual void EntityDestroyed(Entity entity) = 0;
	};

	template <class Component>
	class ComponentArray : public IComponentArray
	{
	public:
		// Component 배열에 n번 엔티티로 삽입
		void InsertData(Entity entity, Component component)
		{
			if (entityToIndexMap_.find(entity) != entityToIndexMap_.end())
			{
				throw ECS_EXCEPT("This Entity is already inserted into the ComponentArray.");
			}

			size_t newIndex = size_;
			entityToIndexMap_[entity] = newIndex;
			indexToEntityMap_[newIndex] = entity;
			componentArray_[newIndex] = component;
			++size_;
		}

		// 마지막 인덱스를 지우려는 인덱스쪽으로 덮어씌우고 마지막 인덱스를 erase
		void RemoveData(Entity entity)
		{
			if (entityToIndexMap_.find(entity) == entityToIndexMap_.end())
			{
				throw ECS_EXCEPT("This Entity is not exist.");
			}

			size_t indexOfRemoveEntity = entityToIndexMap_[entity];
			size_t indexOfLastElement = size_ - 1;
			componentArray_[indexOfRemoveEntity] = componentArray_[indexOfLastElement];

			Entity entityOfLastElement = indexToEntityMap_[indexOfLastElement];
			entityToIndexMap_[entityOfLastElement] = indexOfRemoveEntity;
			indexToEntityMap_[indexOfRemoveEntity] = entityOfLastElement;

			entityToIndexMap_.erase(entity);
			indexToEntityMap_.erase(indexOfLastElement);

			--size_;
		}

		Component& GetData(Entity entity)
		{
			if (entityToIndexMap_.find(entity) == entityToIndexMap_.end())
			{
				throw ECS_EXCEPT("This Entity is not exist.");
			}

			return componentArray_[entityToIndexMap_[entity]];
		}

		void EntityDestroyed(Entity entity) override
		{
			if (entityToIndexMap_.find(entity) != entityToIndexMap_.end())
			{
				// Remove the entity's component if it existed
				RemoveData(entity);
			}
		}

	private:
		std::array<Component, MAX_ENTITIES> componentArray_;
		std::map<Entity, size_t> entityToIndexMap_;
		std::map<size_t, Entity> indexToEntityMap_;
		size_t size_;
	};

	// 타입 이름과 컴포넌트 타입을 매핑 ComponentType은 int임 gNextComponentType으로 인덱싱
	extern std::map<std::string, ComponentType> gComponentTypes;
	// 타입 이름과 해당 타입의 배열(엔터티 배열을 가지고 있는)을 매핑 
	extern std::map<std::string, std::shared_ptr<IComponentArray>> gComponentArrays;
	// 등록된 컴포넌트 정보
	extern ComponentType gNextComponentType;

	// Component 타입의 배열을 반환해준다.
	template <class Component>
	std::shared_ptr<ComponentArray<Component>> GetComponentArray()
	{
		std::string typeName = typeid(Component).name();
		
		if (gComponentTypes.find(typeName) == gComponentTypes.end())
		{
			throw ECS_EXCEPT("Component not registered before use.");
		}

		return std::static_pointer_cast<ComponentArray<Component>>(gComponentArrays[typeName]);
	}

	template<class Component>
	void RegisterComponent() {
		std::string typeName = typeid(Component).name();

		if (gComponentTypes.find(typeName) == gComponentTypes.end())
		{
			ECS_EXCEPT("This Component is already Registered.");
		}

		gComponentTypes.insert({typeName, gNextComponentType});

		gComponentArrays.insert({ typeName, std::make_shared<ComponentArray<Component>>() });

		++gNextComponentType;
	}

	template<class Component>
	ComponentType GetComponentType()
	{
		std::string typeName = typeid(Component).name();

		if (gComponentTypes.find(typeName) == gComponentTypes.end())
		{
			throw ECS_EXCEPT("Component not exit");
		}

		// 시그니처를 만들기 위해 반환됨.
		return gComponentTypes[typeName];
	}

	// ComponentArray의 InsertData가 호출됨
	template<class Component>
	void AddComponent(Entity entity, Component component)
	{
		GetComponentArray<Component>()->InsertData(entity, component);
	}

	// ComponentArray의 RemoveData가 호출됨
	template<class Component>
	void RemoveComponent(Entity entity)
	{
		GetComponentArray<Component>()->RemoveData(entity);
	}

	// ComponentArray의 GetData가 호출됨
	template<class Component>
	Component& GetComponent(Entity entity)
	{
		return GetComponentArray<Component>()->GetData(entity);
	}

	void componentDestroyEntity(Entity entity);

	class System
	{
	public:
		std::set<Entity> entites_;
	};

	extern std::map<std::string, Signature> gSystemSignature;
	extern std::map<std::string, std::shared_ptr<System>> gSystems;

	template<class Sys>
	std::shared_ptr<System> RegisterSystem()
	{
		std::string typeName = typeid(Sys).name();

		if (gSystems.find(typeName) != gSystems.end())
		{
			throw ECS_EXCEPT("This System is already Registered.");
		}

		auto system = std::make_shared<System>();
		gSystems.try_emplace( typeName, system );
		return system;
	}

	template<class Sys>
	void SetSystemSignature(Signature signature)
	{
		std::string typeName = typeid(Sys).name();

		if (gSystems.find(typeName) == gSystems.end())
		{
			throw ECS_EXCEPT("System not exit");
		}

		gSystemSignature.insert({ typeName, signature });
	}

	void systemDestroyEntity(Entity entity);

	void SetEntity(std::string sysName, Entity entity);

	void EntitySignatureChanged(Entity entity);

}	// namespace ecs

#endif // !__ECS_HPP
