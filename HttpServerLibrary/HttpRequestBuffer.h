#pragma once
#include "ByteArray.h"
#include "HttpHeader.h"

#include <cstdint>
#include <functional>
#include <string>
#include <istream>
#include <vector>
#include <experimental/filesystem>

namespace Http
{
	class RequestBuffer
	{
	private:
		uintptr_t socket;
		ByteArray buffer;
		const int capacity;
		const int blocksize;
		const uintptr_t nfds;
		int readpos, writepos;
		std::string client;
		void * ssl;
		std::experimental::filesystem::path & rootPath;
		RequestHeader request;
		ResponseHeader response;
	public:
		RequestBuffer(RequestBuffer&&);
		RequestBuffer(RequestBuffer&) = delete;
		RequestBuffer(uintptr_t socket, const int capacity, const std::string client, void * ssl, std::experimental::filesystem::path & rootPath);
		~RequestBuffer();
		bool isSecure();
		void RecvData(const std::function<int(RequestBuffer&)> callback);
		void Free(int count);
		int Send(const char* data, int length);
		int Send(const std::string &str);
		int Send(std::istream &stream, int offset, int length);
		//void Redirect(std::string cmd, std::string args);
		const int &Capacity();
		int CopyTo(std::ostream &stream, int length);
		const uintptr_t &GetSocket();
		const std::string &Client();
		const int SeqLength();
		const int Length();
		int IndexOf(const char * buffer, int length, int offset, int & mlength);
		int IndexOf(const char * string, int offset, int & mlength);
		int IndexOf(std::string string, int offset, int & mlength);
		ByteArray GetRange(int offset, int length);
		std::experimental::filesystem::path &RootPath();
		RequestHeader &Request();
		ResponseHeader &Response();
	};
}