#pragma once
#include "HttpRequestBuffer.h"

#include <functional>
#include <cstring>
#include <atomic>
#include <cstdint>
#include <experimental/filesystem>

namespace Http
{
	void CreateServerSocket(uintptr_t & serverSocket, int port);
	void CloseSocket(uintptr_t & socket);

	struct RequestArgs
	{
		std::string path;
		std::function<void(int)> Progress;
	};

	class ServerException : public std::exception
	{
	private:
		const std::string message;
	public:
		ServerException(const std::string &message) : message(message)
		{
		}
		const char* what() const noexcept override {
			return message.data();
		}
	};

	class NotFoundException : public ServerException
	{
	public:
		NotFoundException(const std::string &message) : ServerException(message)
		{

		}
	};

	class Server
	{
	private:
		void * servermain;
		std::atomic<bool> servermainstop;
		uintptr_t httpServerSocket, httpsServerSocket;
		std::experimental::filesystem::path rootfolder;
		void processRequest(std::unique_ptr<Http::RequestBuffer> pbuffer);
	public:
		Server();
		~Server();
		void Starten(const int httpPort, const int httpsPort);
		void Stoppen();
		void SetRootFolder(std::experimental::filesystem::path path);
		std::function<void(RequestArgs*)> Request;
	};
}