#pragma once
#include "DualByteArray.h"
#include "ByteArray.h"

#include <cstdint>
#include <functional>
#include <string>
#include <istream>
#include <vector>

namespace HttpServer
{
	class RecvBuffer
	{
	private:
		uintptr_t socket;
		ByteArray buffer;
		const int capacity;
		const int blocksize;
		int readpos, writepos;
		bool recvstate;
		std::string client;
		void * ssl;
	public:
		RecvBuffer(RecvBuffer&&);
		RecvBuffer(RecvBuffer&) = delete;
		RecvBuffer(uintptr_t socket, const int capacity, const std::string client, void * ssl);
		~RecvBuffer();
		bool isSecure();
		void RecvData(std::function<int(RecvBuffer*, DualByteArray)> callback);
		bool RecvDataState();
		void RecvDataState(bool recvstate);
		int Send(const std::string &str);
		int Send(std::istream &stream, int offset, int length);
		int GetCapacity();
		int CopyTo(std::ostream &stream, int length);
		const std::string &Client()
		{
			return client;
		}
	};
}