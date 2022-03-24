#include "connection.h"

namespace net
{

	bool Connection::popIMessage(std::shared_ptr<IMessage>& iMessage)
	{
		if(!_ibuffer.empty())
		{
			iMessage = _ibuffer.pop_front();
			return true;
		}
		return false;
	}

	template<>
	void ServerConnection<tcp_socket>::connectToClient()
	{
		async_readHeader();
	}

	template<>
	void ServerConnection<ssl_socket>::connectToClient()
	{
		_socket.async_handshake(asio::ssl::stream_base::server, [this](std::error_code ec) {
			if (!ec)
			{
				async_readHeader();
			}
			else
			{
				std::cout << "SSL handshake failed: " << ec.message() << std::endl;
			}
		});

	}

	template<>
	void ClientConnection<tcp_socket>::connectToServer(const asio::ip::tcp::resolver::results_type& endpoints, std::function<void()> onConnect)
	{
		asio::async_connect(_socket.lowest_layer(), endpoints, [this, onConnect](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
			if (!ec)
			{
				async_readHeader();
				onConnect();
			}
			else
			{
				std::cerr << "Failed to connect to server.";
			}
		});
	}

	template<>
	void ClientConnection<ssl_socket>::connectToServer(const asio::ip::tcp::resolver::results_type& endpoints, std::function<void()> onConnect)
	{
		asio::async_connect(_socket.lowest_layer(), endpoints, [this, onConnect = std::move(onConnect)](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
			if (!ec)
			{
				_socket.async_handshake(asio::ssl::stream_base::client, [this, onConnect = std::move(onConnect)](std::error_code ec) {
					if (!ec)
					{
						async_readHeader();
						onConnect();
					}
					else
					{
						std::cout << "SSL handshake failed: " << ec.message() << std::endl;
					}
				});
			}
			else
			{
				std::cerr << "Failed to connect to server.";
			}
		});
	}
}