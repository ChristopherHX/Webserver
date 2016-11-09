#pragma once
#include "RecvBuffer.h"

#include <functional>
#include <cstring>
#include <atomic>
#include <cstdint>
#include <experimental/filesystem>

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

	class Server
	{
	private:
		void * servermain;
		std::atomic<bool> servermainstop;
		uintptr_t httpServerSocket, httpsServerSocket;
		bool showFolder;
		std::experimental::filesystem::path rootfolder;
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