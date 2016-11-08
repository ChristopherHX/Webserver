#pragma once
#include "RecvBuffer.h"

#include <functional>
#include <cstring>
#include <atomic>
#include <cstdint>

namespace HttpServer
{
	struct RequestArgs
	{
		std::string path;
		std::function<void(int)> Progress;
	};

	enum RequestState
	{
		HandleRequest,
		FileUpload,
	};

	class Server
	{
	private:
		void * servermain;
		std::atomic<bool> servermainstop;
		uintptr_t serverSocket, sslServerSocket;
		bool showFolder;
		std::string rootfolder;
		void processRequest(std::unique_ptr<RecvBuffer> pbuffer);
	public:
		Server();
		~Server();
		void Starten(const int port, const int sslport);
		void Stoppen();
		void ShowFolder(bool show);
		void SetRootFolder(std::string path);
		std::function<void(RequestArgs*)> Request;
	};
}