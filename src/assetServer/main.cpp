// Asset server
#include <iostream>
#include <networking/connectionAcceptor.h>
#include <common/config/config.h>
#include <json/json.h>
#include <fstream>
#include <string>
#include <ecs/ecs.h>
#include <assetNetworking/networkAuthenticator.h>
#include <assets/types/meshAsset.h>
#include <fileManager/fileManager.h>

struct SentMesh : public NativeComponent<SentMesh>
{
	REGESTER_MEMBERS_0();
};

int main()
{
	Config::loadConfig();

	uint16_t tcpPort = Config::json()["network"].get("tcp port", 80).asUInt();
	uint16_t sslPort = Config::json()["network"].get("ssl port", 81).asUInt();

	EntityManager em;


	net::ConnectionAcceptor ca(tcpPort, &em);
	net::NetworkAuthenticator na(&em);
	
	std::vector<const ComponentAsset*> components;
	components.push_back(EntityIDComponent::def());
	components.push_back(net::ConnectionComponent::def());
	ComponentSet exclude;
	exclude.add(SentMesh::def());
	NativeForEach nfe = NativeForEach(components, exclude, &em);

	
	MeshAsset quad = MeshAsset(AssetID("localhost/this/mesh/quad"), std::vector<uint32_t>({0, 1, 2, 2, 3, 0}),
													std::vector<Vertex>({
														{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
														{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
														{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
														{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
													}));

	FileManager fm;
	fm.writeAsset(&quad);
	
	while (true)
	{
		em.runSystems();

		std::vector<EntityID> sent;
		em.forEach(nfe.id(), [&](byte** component) {
			EntityIDComponent* id = EntityIDComponent::fromVirtual(component[nfe.getComponentIndex(0)]);
			net::ConnectionComponent* cc = net::ConnectionComponent::fromVirtual(component[nfe.getComponentIndex(1)]);
			
			if (cc->connection && cc->connection->isConnected())
			{
				net::OMessage m;
				m.header.type = net::MessageType::meshAsset;
				quad.serialize(m);
				std::cout << "sending message: " << m;
				cc->connection->send(m);
			}
			sent.push_back(id->id);
		});

		for (EntityID id : sent)
		{
			em.addComponent(id, SentMesh::def());
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	return 0;
}