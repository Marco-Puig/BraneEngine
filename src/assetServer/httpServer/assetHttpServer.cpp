#include "assetHttpServer.h"

AssetHttpServer::AssetHttpServer(const std::string& domain, bool useHttps, Database& db, FileManager& fm): HTTPServer(domain, useHttps), _db(db), _fm(fm)
{
	setUpListeners();
    setUpAPICalls();
}

void AssetHttpServer::setUpListeners()
{
	_server->Get("/app?(/.*)", [this](const httplib::Request &req, httplib::Response &res){
		if(_sessions.count(getCookie("session_id", req)))
		{
			serveFile(req, res, _files["/app"]);
		}
		else
		{
			res.set_content("Must be logged in to see this page.", "text/html");
			res.status = 403;
		}
	});
	_server->Get("/(.*)", [this](const httplib::Request &req, httplib::Response &res){
		std::cout << "Request: " << req.path <<std::endl;
		if(_files.count(req.path))
		{
			serverFile& file = _files[req.path];
			std::string sessionID = getCookie("session_id", req);
			if(_sessions.count(sessionID) && _sessions[sessionID].userAuthorized(file))
				serveFile(req, res, _files[req.path]);
			else if(file.authLevel == "public")
				serveFile(req, res, _files[req.path]);
			else
			{
				res.set_content("Must be authorized to see this page.", "text/html");
				res.status = 403;
			}

		}
		else
		{
			res.set_content("404 not found", "text/html");
			res.status = 404;
		}
	});
	_server->Post("/login-submit",[this](const httplib::Request &req, httplib::Response &res){

		try{
			Json::Value newUser;
			Json::Reader reader;
			if(!reader.parse(req.body, newUser))
			{
				std::cerr << "Problem parsing login request: " << req.body << std::endl;
				res.status = 400;
				res.set_content("{\"text\":\"Request format incorrect\",\"logged_in\":false}", "application/json");
				return;
			}
			if(!newUser.isMember("username") || !newUser.isMember("password"))
			{
				res.status = 400;
				res.set_content("{\"text\":\"Request format incorrect\",\"logged_in\":false}", "application/json");
				return;
			}


			std::string username = newUser["username"].asString();
			if(!_db.stringSafe(username))
			{
				res.set_content("{\"text\":\"Invalid Characters\",\"logged_in\":false}", "application/json");
				return;
			}


			bool foundUser = false;
			std::string passHash;
			std::string salt;
			std::string userID;
			_db.rawSQLCall(
					"SELECT Logins.Password, Logins.Salt, Logins.UserID FROM Users INNER JOIN Logins ON Logins.UserID = Users.UserID WHERE lower(Users.Username)=lower('" +
					username + "');", [&](const std::vector<Database::sqlColumn>& columns)
					{
						foundUser = true;
						passHash = columns[0].value;
						salt = columns[1].value;
						userID = columns[2].value;
					});


			if(foundUser)
			{
				std::string hashedPassword = hashPassword(newUser["password"].asString(), salt);

				if(passHash == hashedPassword)
				{
					//Login stuff happen here
					std::string sessionID;
					while(sessionID.empty() || _sessions.count(sessionID))
					{
						sessionID = randHex(32);
					}



					SessionContext sc;
					sc.updateTimer();
					sc.userID = userID;
					sc.username = username;
					sc.permissions = _db.userPermissions(userID);
					_sessions.insert({sessionID, sc});

					setCookie("session_id", sessionID, res);
					res.set_content("{\"text\":\"Login successful\",\"logged_in\":true}", "application/json");
					return;
				}
			}


			//Not login stuff happen here
			res.set_content("{\"text\":\"Login fail\",\"logged_in\":false}", "application/json");
		}
		catch(const std::exception& e){
			std::cerr << "login submission error: " << e.what();
			res.set_content("{\"text\":\"Internal error\",\"logged_in\":false}", "application/json");
			res.status = 500;
		}
	});
	_server->Post("/create-account-submit",[this](const httplib::Request &req, httplib::Response &res){
		try{
			Json::Value newUser;
			Json::Reader reader;
			if(!reader.parse(req.body, newUser))
			{
				std::cerr << "Problem parsing login request: " << req.body << std::endl;
				res.status = 400;
				res.set_content("{\"text\":\"Request format incorrect\",\"created\":false}", "application/json");
				return;
			}
			if(!newUser.isMember("username") || !newUser.isMember("password") || !newUser.isMember("email"))
			{
				res.status = 400;
				res.set_content("{\"text\":\"Request format incorrect\",\"created\":false}", "application/json");
				return;
			}
			std::string username = newUser["username"].asString();
			std::string email = newUser["email"].asString();
			std::string password = newUser["password"].asString();

			if(username == "")
			{
				res.set_content("{\"text\":\"Must enter a username\",\"created\":false}", "application/json");
				return;
			}

			if(password.size() < 8)
			{
				res.set_content("{\"text\":\"Password must be at least 8 characters long\",\"created\":false}", "application/json");
				return;
			}

			if(!_db.stringSafe(username))
			{
				res.set_content("{\"text\":\"Invalid characters in username\",\"created\":false}", "application/json");
				return;
			}

			std::regex emailRegex("^([a-zA-Z0-9_\\-\\.]+)@((\\[[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.)|(([a-zA-Z0-9\\-]+\\.)+))([a-zA-Z]{2,4}|[0-9]{1,3})(\\]?)$");
			if(!std::regex_match(email, emailRegex))
			{
				res.set_content("{\"text\":\"Invalid email\",\"created\":false}", "application/json");
				return;
			}


			bool usernameTaken = false;
			_db.rawSQLCall("SELECT * FROM Users WHERE lower(Username)=lower('" + username + "');",
			               [&usernameTaken](const std::vector<Database::sqlColumn>& columns)
			               {
				               usernameTaken = true;
				               return 0;
			               });

			if(usernameTaken)
			{
				res.set_content("{\"text\":\"Username Taken\",\"created\":false}", "application/json");
				return;
			}

			_db.rawSQLCall("INSERT INTO Users (Username, Email) VALUES ('" +
			               username + "', '" +
			               email +
			               "');", [&usernameTaken](const std::vector<Database::sqlColumn>& columns)
			               {
				               return 0;
			               });

			std::string salt = randHex(64);
			password = hashPassword(password, salt);

			std::string userID;
			_db.rawSQLCall("SELECT UserID FROM Users WHERE lower(Username)=lower('" + username + "');",
			               [&userID](const std::vector<Database::sqlColumn>& columns)
			               {
				               userID = columns[0].value;
				               return 0;
			               });

			_db.rawSQLCall("INSERT INTO Logins (UserID, Password, Salt) VALUES ('" +
			               userID + "', '" +
			               password + "', '" +
			               salt +
			               "');", [&usernameTaken](const std::vector<Database::sqlColumn>& columns)
			               {
				               return 0;
			               });


			res.set_content("{\"text\":\"Created account\",\"created\":true}", "application/json");
		}
		catch(const std::exception& e){
			std::cerr << "user creation error: " << e.what();
			res.set_content("{\"text\":\"Internal error\",\"created\":false}", "application/json");
			res.status = 500;
		}
	});

}

void AssetHttpServer::setUpAPICalls()
{
    _server->Post("/api/create-asset",[this](const httplib::Request &req, httplib::Response &res)
    {
        try{
            std::string sessionID = getCookie("session_id", req);
            if(_sessions.count(sessionID) && _sessions[sessionID].hasPermission("create assets"))
            {
                std::cout << "Creating Asset" << std::endl;
                const auto& assetDataField = req.get_file_value("assetData");
                std::cout << "AssetData: " << assetDataField.content << std::endl;
				// Convert to json
				Json::Value assetData;
	            Json::Reader reader;
	            if(!reader.parse( assetDataField.content, assetData))
	            {
		            std::cerr << "Problem parsing assetData: " << req.body << std::endl;
		            res.status = 400;
		            res.set_content("{\"text\":\"Request format incorrect\",\"logged_in\":false}", "application/json");
		            return;
	            }

				AssetID id;
	            id.serverAddress = _domain;
	            id.owner = _sessions[sessionID].userID;
				id.type.set(assetData["type"].asString());
				id.name = assetData["name"].asString();

				switch(id.type.type()){
					case AssetType::Type::mesh:
					{
						std::cout << "Creating new mesh asset: " << id << std::endl;

						const auto& file = req.get_file_value("file");
						std::cout << "Mesh size: " << file.content.size() << " bytes" << std::endl;

						tinygltf::Model model;
						tinygltf::TinyGLTF loader;
						std::string warn;
						std::string err;

						loader.LoadBinaryFromMemory(&model, &err, &warn, (unsigned char*)file.content.data(), file.content.size());
						if (!warn.empty()) {
							std::cout << "gltf warning: " << warn << std::endl;
						}

						if (!err.empty()) {
							std::cerr << "gltf error: " << err << std::endl;
						}


						for(auto& mesh : model.meshes){

							std::cout << "Uploaded mesh: " << mesh.name << std::endl;
							for(auto& primitive  : mesh.primitives){
								const tinygltf::Accessor& accessor = model.accessors[primitive.attributes["POSITION"]];
								const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

								const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
								const float* positions = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
								for (size_t i = 0; i < accessor.count; ++i) {
									// Positions are Vec3 components, so for each vec3 stride, offset for x, y, and z.
									std::cout << "(" << positions[i * 3 + 0] << ", "// x
									          << positions[i * 3 + 1] << ", " // y
									          << positions[i * 3 + 2] << ")" // z
									          << "\n";
								}
							}
						}


						break;
					}


					default:
						throw std::runtime_error("Unimplemented asset type: " + id.type.string());
				}
            }
            else
            {
                res.status = 403;
                res.set_content("Not authorized for that action", "text/plain");
            }
        }
        catch(const std::exception& e){
            std::cerr << "asset upload error: " << e.what();
            res.status = 500;
        }

    });
}

void AssetHttpServer::SessionContext::updateTimer()
{
	lastAction = std::chrono::system_clock::now();
}

bool AssetHttpServer::SessionContext::userAuthorized(serverFile& file)
{
	return file.authLevel == "public" || file.authLevel == "app" || permissions.count(file.authLevel);
}

bool AssetHttpServer::SessionContext::hasPermission(const std::string& permission)
{
	return permissions.count(permission);
}

#define WIN32_LEAN_AND_MEAN
#include <openssl/rand.h>
#include <openssl/crypto.h>

std::string AssetHttpServer::hashPassword(const std::string& password, const std::string& salt)
{
	size_t hashIterations = Config::json()["security"].get("hash_iterations", 10000).asLargestInt();
	unsigned char hash[SHA256_DIGEST_LENGTH];
	std::string output = password;
	for (int i = 0; i < hashIterations; ++i)
	{
		output += salt;
		SHA256_CTX sha256;
		SHA256_Init(&sha256);
		SHA256_Update(&sha256, output.c_str(), output.size());
		SHA256_Final(hash, &sha256);
		std::stringstream ss;
		for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
		{
			ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
		}
		output = ss.str();
	}
	return output;
}

std::string AssetHttpServer::randHex(size_t length)
{
	uint8_t* buffer = new uint8_t[length];
	RAND_bytes(buffer, length);
	std::stringstream output;
	for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		output << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i];
	}
	return output.str();
}

void AssetHttpServer::processAsset(const std::string filename, const std::string data)
{

}


