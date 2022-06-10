#pragma once
#include <cstdint>
#include "component.h"
#include <memory>
#include <functional>
#include <unordered_map>
#include <common/utility/stackAllocate.h>
#include <utility/shared_recursive_mutex.h>
#include <utility/threadPool.h>
#include <unordered_set>
#include <list>
#include "componentSet.h"

class Archetype;

struct ArchetypeEdge
{
	const ComponentAsset* component;
	Archetype* archetype;
	ArchetypeEdge(const ComponentAsset* component, Archetype* archetype);
};

template <size_t N> class ChunkBase;
typedef ChunkBase<16384> Chunk;
class ChunkPool;

class Archetype
{
#ifdef TEST_BUILD
public:
#endif
	size_t _size = 0;
	size_t _entitySize;

	//Eventually move these to separate node class
	std::vector<std::shared_ptr<ArchetypeEdge>> _addEdges;
	std::vector<std::shared_ptr<ArchetypeEdge>> _removeEdges;
	//

	const ComponentSet _components;
	std::vector<std::unique_ptr<Chunk>> _chunks;
	std::shared_ptr<ChunkPool> _chunkAllocator;

	mutable shared_recursive_mutex _mutex;

	size_t chunkIndex(size_t entity) const;
	Chunk* getChunk(size_t entity) const;
public:
	Archetype(const ComponentSet& components, std::shared_ptr<ChunkPool>& _chunkAllocator);
	~Archetype();
	bool hasComponent(const ComponentAsset* component) const;
	bool hasComponents(const ComponentSet& comps) const;
	VirtualComponent getComponent(size_t entity, const ComponentAsset* component) const;
	void setComponent(size_t entity, const VirtualComponent& component);
	void setComponent(size_t entity, const VirtualComponentPtr& component);
	bool isChildOf(const Archetype* parent, const ComponentAsset*& connectingComponent) const;
	const ComponentSet& components() const;
	std::shared_ptr<ArchetypeEdge> getAddEdge(const ComponentAsset* component);
	std::shared_ptr<ArchetypeEdge> getRemoveEdge(const ComponentAsset* component);
	void addAddEdge(const ComponentAsset* component, Archetype* archetype);
	void addRemoveEdge(const ComponentAsset* component, Archetype* archetype);
	void forAddEdge(const std::function<void(std::shared_ptr<const ArchetypeEdge>)>& f) const;
	void forRemoveEdge(std::function<void(std::shared_ptr<const ArchetypeEdge>)>& f) const;
	size_t size() const;
	size_t createEntity();
	size_t copyEntity(Archetype* source, size_t index);
	size_t entitySize() const;
	void remove(size_t index);

	void forEach(const ComponentSet& components, const std::function<void(const byte* [])>& f, size_t start, size_t end);
	void forEach(const ComponentSet& components, const std::function<void(byte* [])>& f, size_t start, size_t end);
}; 


typedef uint64_t EntityForEachID;

class ArchetypeManager
{
#ifdef TEST_BUILD
public:
#endif

	struct ForEachData
	{
		ComponentSet components;
		ComponentSet exclude;
		std::vector<Archetype*> archetypes;
		ForEachData(ComponentSet components, ComponentSet exclude);
	};

	std::vector<ForEachData> _forEachData;

	std::shared_ptr<ChunkPool> _chunkAllocator;
	// Index 1: number of components, Index 2: archetype
	std::vector<std::vector<std::unique_ptr<Archetype>>> _archetypes;

	void findArchetypes(const ComponentSet& components, const ComponentSet& exclude, std::vector<Archetype*>& archetypes) const;
	void cacheArchetype(Archetype* arch);
	void removeCachedArchetype(Archetype* arch); //TODO create archetype cleanup system
	std::vector<Archetype*>& getForEachArchetypes(EntityForEachID id);

public:
	ArchetypeManager();
	Archetype* getArchetype(const ComponentSet& components);
	Archetype* makeArchetype(const ComponentSet& components);

	EntityForEachID getForEachID(const ComponentSet& components, const ComponentSet& exclude = ComponentSet());
	size_t forEachCount(EntityForEachID id);

	void forEach(EntityForEachID id, const std::function <void(byte* [])>& f);
	void constForEach(EntityForEachID id, const std::function <void(const byte* [])>& f);
	std::shared_ptr<JobHandle> forEachParallel(EntityForEachID id, const std::function <void(byte* [])>& f, size_t entitiesPerThread);
	std::shared_ptr<JobHandle> constForEachParallel(EntityForEachID id, const std::function <void(const byte* [])>& f, size_t entitiesPerThread);

	void clear();
};