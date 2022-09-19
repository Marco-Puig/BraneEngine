//
// Created by eli on 3/3/2022.
//

#include "assetServer.h"
#include "networking/networking.h"
#include "assets/assetManager.h"
#include "fileManager/fileManager.h"
#include "utility/hex.h"

AssetServer::AssetServer() :
_nm(*Runtime::getModule<NetworkManager>()),
_am(*Runtime::getModule<AssetManager>()),
_fm(*Runtime::getModule<FileManager>()),
_db(*Runtime::getModule<Database>())
{
	_nm.start();
	_nm.configureServer();
	std::filesystem::create_directory(Config::json()["data"]["asset_path"].asString());

	if(!Config::json()["network"]["use_ssl"].asBool())
	{
		std::cout << "Started listening for asset requests on port: " << Config::json()["network"]["tcp_port"].asUInt() << std::endl;
		_nm.openClientAcceptor<net::tcp_socket>(Config::json()["network"]["tcp_port"].asUInt(), [this](const std::unique_ptr<net::Connection>& connection){
			std::cout << "User connected to tcp" << std::endl;
		});
	}
	else
	{
		std::cout << "Started listening for asset requests on port: " << Config::json()["network"]["ssl_port"].asUInt() << std::endl;
		_nm.openClientAcceptor<net::ssl_socket>(Config::json()["network"]["ssl_port"].asUInt(), [this](const std::unique_ptr<net::Connection>& connection){
			std::cout << "User connected to ssl" << std::endl;
		});
	}

	_nm.addRequestListener("login", [this](auto& rc){
		auto& ctx = getContext(rc.sender);
		if(ctx.authenticated)
			return;
        rc.code = net::ResponseCode::denied;
        std::string username, password;
        rc.req >> username >> password;
        if(_db.authenticate(username, password))
        {
            ctx.authenticated = true;
            ctx.username = username;
            ctx.userID = _db.getUserID(username);
            ctx.permissions = _db.userPermissions(ctx.userID);
            rc.code = net::ResponseCode::success;
        }

	});

	_nm.addRequestListener("asset", [this](auto& rc){
		auto ctx = getContext(rc.sender);
		if(!ctx.authenticated)
        {
            rc.code = net::ResponseCode::denied;
            return;
        }
		AssetID id;
		rc.req >> id;
		std::cout << "request for: " << id.string() << std::endl;
        _fm.readFile(assetPath(id), rc.responseData.vector());
	});

	_nm.addRequestListener("incrementalAsset", [this](auto& rc){
		auto ctx = getContext(rc.sender);
		if(!ctx.authenticated)
        {
            rc.code = net::ResponseCode::denied;
            return;
        }
		AssetID id;
		uint32_t streamID;
		rc.req >> id >> streamID;
		std::cout << "request for: " << id.string() << std::endl;

		Asset* asset = _am.getAsset<Asset>(id);
        auto ctxPtr = std::make_shared<RequestCTX>(std::move(rc));
		auto f = [this, ctxPtr, streamID](Asset* asset) mutable
		{
			auto* ia = dynamic_cast<IncrementalAsset*>(asset);
			if(ia)
			{
				std::cout<< "Sending header for: " << ia->id << std::endl;
				ia->serializeHeader(ctxPtr->res);

				IncrementalAssetSender assetSender{};
				assetSender.iteratorData = ia->createContext();
				assetSender.asset = ia;
				assetSender.streamID = streamID;
				assetSender.connection = ctxPtr->sender;
				_senders.push_back(std::move(assetSender));
                ctxPtr = nullptr;
			}
			else
				std::cerr << "Tried to request non-incremental asset as incremental" << std::endl;
		};

		if(asset)
			f(asset);
		else
			_am.fetchAsset<Asset>(id).then(f);

	});

    _nm.addRequestListener("updateAsset", [this](auto& rc){
        auto ctx = getContext(rc.sender);
        if(!validatePermissions(ctx, {"edit assets"}))
        {
            rc.code = net::ResponseCode::denied;
            return;
        }
        Asset* asset = Asset::deserializeUnknown(rc.req);

        auto assetInfo = _db.getAssetInfo(asset->id.id());

	    auto path = assetPath(asset->id);
		bool assetExists = true;
		if(assetInfo.hash.empty())
		{
			assetExists = false;
		}

        _fm.writeAsset(asset, path);
		assetInfo.id = asset->id.id();
	    assetInfo.name = asset->name;
	    assetInfo.type = asset->type;
	    assetInfo.hash = FileManager::fileHash(path);

		if(assetExists)
			_db.updateAssetInfo(assetInfo);
		else
			_db.insertAssetInfo(assetInfo);

        rc.res << asset->id;
    });

	_nm.addRequestListener("getAssetDiff", [this](auto& rc)
	{
		auto ctx = getContext(rc.sender);
		if(!validatePermissions(ctx, {"edit assets"}))
		{
			rc.code = net::ResponseCode::denied;
			return;
		}
		uint32_t hashCount;
		rc.req >> hashCount;
		std::vector<std::pair<AssetID, std::string>> hashes(hashCount);
		for(uint32_t h = 0; h < hashCount; ++h)
			rc.req >> hashes[h].first >> hashes[h].second;

		std::vector<AssetID> assetsWithDiff;
		for(auto& h : hashes)
		{
			if(h.first.address() != "")
				continue;
			auto info = _db.getAssetInfo(h.first.id());
			if(info.hash != h.second)
				assetsWithDiff.push_back(std::move(h.first));
		}

		rc.res << assetsWithDiff;
	});

	Runtime::timeline().addTask("send asset data", [this]{processMessages();}, "networking");
}

void AssetServer::processMessages()
{
	//Send one increment from every incremental asset that we are sending, to create the illusion of them loading in parallel
	_senders.remove_if([&](IncrementalAssetSender& sender)
	{
		try
		{
			SerializedData data;
            OutputSerializer s(data);
			bool moreData = sender.asset->serializeIncrement(s, sender.iteratorData.get());
			sender.connection->sendStreamData(sender.streamID, std::move(data));
			if(!moreData)
                sender.connection->endStream(sender.streamID);
			return !moreData;
		}
		catch(const std::exception& e) {
			std::cerr << "Asset sender error: " << e.what() << std::endl;
			return true;
		}
	});
}

AssetServer::~AssetServer()
{

}

const char* AssetServer::name()
{
	return "assetServer";
}

AsyncData<Asset*> AssetServer::fetchAssetCallback(const AssetID& id, bool incremental)
{
	AsyncData<Asset*> asset;
	auto info = _db.getAssetInfo(id.id());
	std::filesystem::path path = assetPath(id);
	if(!std::filesystem::exists(path))
		asset.setError("Asset not found");
	else
	{
		_fm.async_readUnknownAsset(path).then([this, asset](Asset* data){
			if(data)
				asset.setData(data);
			else
				asset.setError("Could not read requested asset");
		});
	}

	return asset;
}

AssetServer::ConnectionContext& AssetServer::getContext(net::Connection* connection)
{
	if(!_connectionCtx.count(connection))
	{
		_connectionCtx.insert({connection, ConnectionContext{}});
		connection->onDisconnect([this, connection]{
			_connectionCtx.erase(connection);
		});
	}

	return _connectionCtx[connection];
}

bool AssetServer::validatePermissions(AssetServer::ConnectionContext& ctx, const std::vector<std::string>& permissions)
{
	if(ctx.userID == 1)
		return true; //Admin has all permissions;
	for(auto& p : permissions)
	{
		if(!ctx.permissions.count(p))
			return false;
	}
	return true;
}

std::filesystem::path AssetServer::assetPath(const AssetID& id)
{
    return std::filesystem::path{Config::json()["data"]["asset_path"].asString()} / (std::string(id.idStr()) + ".bin");
}

//The asset server specific fetch asset function
AsyncData<Asset*> AssetManager::fetchAssetInternal(const AssetID& id, bool incremental)
{
	AsyncData<Asset*> asset;
	if(hasAsset(id))
	{
		asset.setData(getAsset<Asset>(id));
		return asset;
	}

	AssetData* assetData = new AssetData{};
	assetData->loadState = LoadState::requested;
	_assetLock.lock();
	_assets.insert({id, std::unique_ptr<AssetData>(assetData)});
	_assetLock.unlock();

	auto* nm = Runtime::getModule<NetworkManager>();
	auto* fm = Runtime::getModule<FileManager>();

	auto path = std::filesystem::path{Config::json()["data"]["asset_path"].asString()} / (std::string(id.idStr()) + ".bin");
	if(!std::filesystem::exists(path))
	{
		fm->async_readUnknownAsset(path).then([this, asset](Asset* ptr) {
			std::scoped_lock lock(_assetLock);
			auto& d = _assets.at(ptr->id);
			d->asset = std::unique_ptr<Asset>(ptr);
			if(dependenciesLoaded(ptr))
			{
				d->loadState = LoadState::loaded;
				asset.setData(ptr);
				return;
			}

			d->loadState = LoadState::awaitingDependencies;
			fetchDependencies(ptr, [this, ptr, asset]() mutable{
				auto& d = _assets.at(ptr->id);
				d->loadState = LoadState::usable;
				ptr->onDependenciesLoaded();
				asset.setData(ptr);
			});
		});
		return asset;
	}
	if(incremental)
	{
		nm->async_requestAssetIncremental(id).then([this, asset](Asset* ptr){
			auto& d = _assets.at(ptr->id);
			d->loadState = LoadState::usable;
			d->asset = std::unique_ptr<Asset>(ptr);
			ptr->onDependenciesLoaded();
			asset.setData(ptr);
		});
	}
	else
	{
		nm->async_requestAsset(id).then([this, asset](Asset* ptr){
			AssetID& id = ptr->id;
			auto& d = _assets.at(id);
			d->loadState = LoadState::awaitingDependencies;
			d->asset = std::unique_ptr<Asset>(ptr);
			if(dependenciesLoaded(d->asset.get()))
			{
				d->loadState = LoadState::loaded;
				d->asset->onDependenciesLoaded();
				asset.setData(ptr);
				return;
			}
			d->loadState = LoadState::awaitingDependencies;
			fetchDependencies(d->asset.get(), [this, &id, asset]() mutable{
				auto& d = _assets.at(id);
				d->loadState = LoadState::loaded;
				d->asset->onDependenciesLoaded();
				asset.setData(d->asset.get());
			});
		});
	}
	return asset;
}


