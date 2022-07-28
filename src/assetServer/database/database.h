//
// Created by eli on 1/10/2022.
//

#ifndef BRANEENGINE_DATABASE_H
#define BRANEENGINE_DATABASE_H
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "PreppedSQLCall.h"
#include "databaseAsset.h"

#include "runtime/module.h"

class AssetID;

class Database : public Module
{
	sqlite3* _db;
	PreppedSQLCall<sqlTEXT> _getLastInserted;
	PreppedSQLCall<sqlTEXT> _loginCall;
	PreppedSQLCall<sqlTEXT> _userIDCall;

	PreppedSQLCall<sqlINT> _getAssetInfo;
	PreppedSQLCall<sqlINT, sqlTEXT, sqlTEXT, sqlTEXT> _updateAssetInfo;
	PreppedSQLCall<sqlTEXT, sqlTEXT, sqlTEXT> _insertAssetInfo;
    PreppedSQLCall<sqlTEXT, sqlTEXT> _moveAssets;
	PreppedSQLCall<sqlTEXT> _fileToAssetID;
	PreppedSQLCall<sqlINT> _deleteAsset;
	PreppedSQLCall<sqlINT, sqlINT> _getAssetPermission;
	PreppedSQLCall<sqlINT, sqlINT, sqlINT> _updateAssetPermission;
	PreppedSQLCall<sqlINT, sqlINT> _deleteAssetPermission;

	static int sqliteCallback(void *callback, int argc, char **argv, char **azColName);

	static std::string randHex(size_t length);
	std::string hashPassword(const std::string& password, const std::string& salt);

	std::unordered_map<size_t, std::string> _permissions;
public:
	struct sqlColumn
	{
		char* name;
		char* value;
	};
	using sqlCallbackFunction =  std::function<void(const std::vector<sqlColumn>& columns)>;
	Database();
	~Database();
	void rawSQLCall(const std::string& cmd, const sqlCallbackFunction& f);

	bool authenticate(const std::string& username, const std::string& password);
	int64_t getUserID(const std::string& username);
	std::unordered_set<std::string> userPermissions(int64_t userID);

	AssetInfo getAssetInfo(uint32_t id);
	void insertAssetInfo(const AssetInfo& info);
	void insertAssetInfo(AssetInfo& info);
    void moveAssets(const std::string& oldDir, const std::string& newDir);
	void deleteAssetInfo(uint32_t id);
	bool fileToAssetID(const std::string& path, AssetID& id);
	AssetPermissionLevel getAssetPermission(uint32_t assetID, uint32_t userID);
	void setAssetPermission(uint32_t assetID, uint32_t userID, AssetPermissionLevel level);
	std::string assetName(AssetID& id);

	std::vector<AssetInfo> listUserAssets(const uint32_t& userID);

	static const char* name();
};


#endif //BRANEENGINE_DATABASE_H