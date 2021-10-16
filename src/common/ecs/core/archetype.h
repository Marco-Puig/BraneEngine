#pragma once
#include <cstdint>
#include "Component.h"
#include "VirtualSystem.h"
#include <memory>
#include <functional>
#include <unordered_map>
#include <common/utility/stackAllocate.h>

typedef uint64_t ArchetypeID;
class Archetype;

struct ArchetypeEdge
{
	const ComponentDefinition* component;
	Archetype* archetype;
	ArchetypeEdge(const ComponentDefinition* component, Archetype* archetype);
};

class Archetype
{
private:
	size_t _size = 0;
	std::vector<std::shared_ptr<ArchetypeEdge>> _addEdges;
	std::vector<std::shared_ptr<ArchetypeEdge>> _removeEdges;
	const ComponentSet _componentDefs;
	std::vector<VirtualComponentVector> _components;
public:
	Archetype(const ComponentSet& componentDefs);
	bool hasComponent(const ComponentDefinition* component) const;
	bool hasComponents(const ComponentSet& comps) const;
	const VirtualComponentPtr getComponent(size_t entity, const ComponentDefinition* component) const;
	bool isChildOf(const Archetype* parent, const ComponentDefinition*& connectingComponent) const;
	bool isRootForComponent(const ComponentDefinition* component) const;
	const ComponentSet& componentDefs() const;
	std::shared_ptr<ArchetypeEdge> getAddEdge(const ComponentDefinition* component);
	std::shared_ptr<ArchetypeEdge> getRemoveEdge(const ComponentDefinition* component);
	void addAddEdge(const ComponentDefinition* component, Archetype* archetype);
	void addRemoveEdge(const ComponentDefinition* component, Archetype* archetype);
	void forAddEdge(const std::function<void(std::shared_ptr<ArchetypeEdge>)>& f);
	void forRemoveEdge(std::function<void(std::shared_ptr<ArchetypeEdge>)>& f);
	size_t size();
	size_t createEntity();
	size_t copyEntity(Archetype* source, size_t index);
	void remove(size_t index);

	void forEach(const ComponentSet& components, const std::function<void(byte* [])>& f);
}; 
