#pragma once
#include "HttpRequestBuffer.h"

#include <functional>
#include <cstring>
#include <atomic>
#include <cstdint>
#include <experimental/filesystem>

namespace Http
{
	struct RequestArgs
	{
		std::string path;
		std::function<void(int)> Progress;
	};

	class ServerError : public std::runtime_error
	{
	public:
		ServerError(const std::string &message) : std::runtime_error(message)
		{
		}
	};

	class NotFoundError : public std::runtime_error
	{
	public:
		NotFoundError(const std::string &message) : std::runtime_error(message)
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
		std::function<void(RequestArgs&)> onrequest;
	public:
		Server();
		~Server();
		void Starten(const int httpPort, const int httpsPort);
		void Stoppen();
		void SetRootFolder(const std::experimental::filesystem::path & path);
		void OnRequest(std::function<void(RequestArgs&)> onrequest);
	};
}