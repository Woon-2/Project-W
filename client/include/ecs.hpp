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

/**
 * @brief This is namespace for ecs ( Entity_Component_System )
 * 
 * Entity has a id.
 * 
 * Component is a **struct** or **class**.
 * Component is assigned id as ComponentType.
 * 
 */

namespace ecs {
	/**
	* @typedef Entity
	* @brief A 32-bit integer type that represents an entity.
	*/
	using Entity = std::int32_t;
	/**
	* @typedef ComponentType
	* @brief An 8-bit unsigned integer type that represents the component type.
	* 
	* 
	*/
	using ComponentType = std::uint8_t;

	/**
	* @var MAX_ENTITIES
	* @brief Maximum number of entities that the system can manage.
	*/
	const Entity MAX_ENTITIES = 1024;
	/**
	* @var MAX_COMPONENTS
	* @brief Maximum number of components that the system can manage.
	*/
	const ComponentType MAX_COMPONENTS = 64;

	/**
	* @typedef Signature
	* @brief Bitset that represents the set of components assigned to an entity.
	* 
	* This bit set represents what components an entity has.
	*/
	using Signature = std::bitset<MAX_COMPONENTS>;

	/**
	* @var gSignature
	* @brief The array of signatures each entity has.
	* 
	* This array represents the component configuration of each entity managed by the system.
	*/
	extern std::array<Signature, MAX_ENTITIES> gSignature;

	/**
	* @var gAvailableEntities
	* @brief Queue that stores unused entity IDs.
	* 
	* This queue manages reusable entity IDs and is used when new entities are created.
	*/
	extern std::queue<Entity> gAvailableEntities;
	/**
	* @var gLivingEntityCount
	* @brief Variables representing the number of entities currently active.
	*/
	extern uint32_t gLivingEntityCount;

	/**
	* @brief Configures the ECS environment for entity management.
	* 
	* Makes the queue ready to manage the entity.
	*/
	void ConfigEntity();
	/**
	* @brief Creates a new entity.
	* 
	* Generates a new entity ID and returns it.
	* 
	* @return Entity The newly created entity ID.
	*/
	Entity CreateEntity();
	/**
	* @brief Destroys an entity.
	* 
	* Remove the specified entity from the ECS system, freeing up its resources.
	* 
	* @param entity The ID of the entity to destroy.
	*/
	void DestroyEntity(const Entity entity);
	/**
	* @brief Sets the signature for a given entity.
	* 
	* @code
	* ecs::Signature signature;
	* signature.set(ecs::GetComponentType<PlayerController>());
	* signature.set(ecs::GetComponentType<Position>());
	* ecs::SetSignature(entityNumber_, signature);
	* @endcode
	* 
	* @param entity The ID of the entity.
	* @param signature The signature to assign to the entity.
	*/
	void SetSignature(Entity entity, Signature signature);
	/**
	* @brief Retrieves the signature of a specified entity.
	* 
	* Returns the bitset signature that represents the components of the entity.
	* 
	* @param entity The ID of the entity.
	* @return Signature The signature associated with the entity.
	*/
	Signature GetSignature(Entity entity);

	/**
	* @brief Abstract class for managing component arrays.
	* 
	* The base class for component arrays that provides an interface for handling entity destruction.
	*/
	class IComponentArray
	{
	public:
		/**
		* @brief Virtual destructor.
		*/
		virtual ~IComponentArray() = default;
		/**
		* @brief Handles the destruction of an entity.
		* 
		* When an entity is destroyed, this function is called to remove it from the component array.
		* 
		* @param entity The ID of the entity to destroy.
		*/
		virtual void EntityDestroyed(Entity entity) = 0;
	};

	/**
	* @brief Template class for managing specific component arrays.
	* 
	* Manages arrays of specific component types and allows for insertion, removal, 
	*	and retrieval of components associated with entities
	* 
	* @tparam Component The type of component to manage.
	*/
	template <class Component>
	class ComponentArray : public IComponentArray
	{
	public:
		/**
		* @brief Inserts a component for a specific entity.
		* 
		* Adds a component to the array, associating it with a specific entity.
		* 
		* @param entity The ID of the entity.
		* @param component The component to insert.
		*/
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

		/**
		* @brief Removes a component associated with a specific entity.
		* 
		* Removes the component for a given entity, reorganizing the array if necessary.
		* 
		* reorganizing : Cover the index to be erased with the last index and clear the last index
		* 
		* if entity is deleted this function is called.
		* so you don't need to call this.
		* 
		* @param entity The ID of the entity.
		*/
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

		/**
		* @brief Retrieves the component associated with a specific entity.
		* 
		* Returns the component associated with the given entity ID.
		* 
		* For example, if you want specific player's position (position is component), call this. 
		* 
		* or call template function GetComponent()
		* 
		* @param entity The ID of the entity.
		* @return Component& Reference to the component associated with the entity.
		*/
		Component& GetData(Entity entity)
		{
			if (entityToIndexMap_.find(entity) == entityToIndexMap_.end())
			{
				throw ECS_EXCEPT("This Entity is not exist.");
			}

			return componentArray_[entityToIndexMap_[entity]];
		}
		/**
		* @brief Handles the destruction of an entity.
		* 
		* Removes the entity's component from the array if it exists.
		* 
		* @param entity The ID of the entity to destroy.
		*/
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

	/**
	* @var gComponentTypes
	* @brief Maps component type names to 'ComponentType' IDs.
	* 
	* This map is used to index component types using 'gNextComponentType'.
	*/
	extern std::map<std::string, ComponentType> gComponentTypes;
	/**
	* @var gComponentArrays
	* @brief Maps component type names to arrays of components.
	* 
	* This map associates component types with their respective arrays that store the components associated with entities.
	*/
	extern std::map<std::string, std::shared_ptr<IComponentArray>> gComponentArrays;
	/**
	* @var gNextComponentType
	* @brief The ID to be assigned to the next registered component type.
	*/
	extern ComponentType gNextComponentType;

	/**
	* @brief Retrieves the component array for a specific component type.
	* 
	* Returns the array of components associated with the given component type.
	* 
	* @tparam Component The type of component to retrieve the array for.
	* @return std::shared_ptr<ComponentArray<Component>> A shared pointer to the component array.
	*/
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
	/**
	* @brief Registers a new component type.
	* 
	* This function registers a component type, allowing it to be used within the ECS.
	* 
	* @tparam Component The type of component to register.
	*/
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
	/**
	* @brief Retrieves the component type ID for a specific component.
	* 
	* Returns the 'ComponentType' ID associated with the given component type.
	* Return values can be used to combine signatures.
	* 
	* @tparam Component The type of component to retrieve the ID for.
	* @return ComponentType The ID of the component type.
	*/
	template<class Component>
	ComponentType GetComponentType()
	{
		std::string typeName = typeid(Component).name();

		if (gComponentTypes.find(typeName) == gComponentTypes.end())
		{
			throw ECS_EXCEPT("Component not exit");
		}

		return gComponentTypes[typeName];
	}

	/**
	* @brief Adds a component to an entity.
	* 
	* Inserts a component into the array for the specified entity.
	* 
	* @tparam Component The type of component to add.
	* @param entity The ID of entity.
	* @param component The component to add.
	*/
	template<class Component>
	void AddComponent(Entity entity, Component component)
	{
		GetComponentArray<Component>()->InsertData(entity, component);
	}
	/**
	* @brief Removes a component from a entity.
	* 
	* Removes the component associated with the specified entity from the component array.
	* 
	* @tparam Component The type of component to remove.
	* @param entity The ID of the entity.
	*/
	template<class Component>
	void RemoveComponent(Entity entity)
	{
		GetComponentArray<Component>()->RemoveData(entity);
	}
	/**
	* @Retrieves a component from an entity.
	* 
	* Returns the component associated with the specified entity.
	* 
	* @tparam Component The type of component to retrieve.
	* @param entity The ID of the entity.
	* @return Component& Reference to the component associated with the entity.
	*/
	template<class Component>
	Component& GetComponent(Entity entity)
	{
		return GetComponentArray<Component>()->GetData(entity);
	}
	/**
	* @brief Handles the destruction of an entity in the component.
	* 
	* Removes the entity from all relevant component arrays.
	* 
	* @param entity The ID of the entity to destroy.
	*/
	void componentDestroyEntity(Entity entity);

	/**
	* @brief Represents a system in the ECS framework.
	* 
	* Systems operate on entities with specific component signatures.
	*/
	class System
	{
	public:
		/**
		* @brief A set of entities that this system operates on.
		*/
		std::set<Entity> entites_;
	};

	/**
	* @var gSystemSignature
	* @brief Maps system type names to their component signatures.
	* 
	* This map associates systems with the signatures of components they operate on.
	*/
	extern std::map<std::string, Signature> gSystemSignature;
	/**
	* @var gSystems
	* @brief Maps system type names to system instances.
	* 
	* This map stores the instances of systems registered in the ECS.
	*/
	extern std::map<std::string, std::shared_ptr<System>> gSystems;

	/**
	* @brief Registers a new system with the ECS.
	* 
	* This function registers a system type, creating an instance of it within the ECS.
	* 
	* @tparam Sys The type of system to register.
	* @return std::shared_ptr<System> A shared pointer to the registered system instance.
	*/
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

	/**
	* @brief Sets the component signature for a system.
	* 
	* Associates a component signature with a system, defining which entities it will operate on.
	* 
	* @tparam Sys The type of system.
	* @param signature The signature to set for the system.
	*/
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

	/**
	* @brief Handles the destruction of and entity in the system.
	* 
	* Removes the entity from all relevant systems.
	* 
	* @param entity The ID of the entity to destroy.
	*/
	void systemDestroyEntity(Entity entity);

	/**
	* @brief Adds an entity to a specific system.
	* 
	* Assigns an entity to a system based on its name.
	* 
	* @param sysName The name of system.
	* @param entity The ID of the entity to add.
	*/
	void SetEntity(std::string sysName, Entity entity);

	/**
	* @brief Updates the system when an entity's signature changes.
	* 
	* Ensures systems are updated when an entity's component configuration changes.
	*/
	void EntitySignatureChanged(Entity entity);

}	// namespace ecs

#endif // !__ECS_HPP
