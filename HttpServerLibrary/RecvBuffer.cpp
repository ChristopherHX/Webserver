#include "RecvBuffer.h"
#include "ByteArray.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#undef min
#else
#include <sys/socket.h>
#include <unistd.h> 
#endif
#include <algorithm>
#include <cstring>
#include <thread>
#include <atomic>
#include <iostream>

#include <openssl/ssl.h>
#include <openssl/err.h>

using namespace HttpServer;

HttpServer::RecvBuffer::RecvBuffer(RecvBuffer && obj) : buffer(std::move(obj.buffer)), capacity(obj.capacity), readpos(obj.readpos), writepos(obj.writepos), recvstate(obj.recvstate), blocksize(obj.blocksize), client(obj.client), ssl(obj.ssl)
{
	
}

RecvBuffer::RecvBuffer(uintptr_t socket, const int capacity, std::string client, void * ssl) : socket(socket), buffer(capacity), capacity(capacity), readpos(0), writepos(0), recvstate(false), blocksize(capacity >> 4), client(client), ssl(ssl)
{
	
}

RecvBuffer::~RecvBuffer()
{
	if(ssl != nullptr) SSL_free((SSL*)ssl);
#ifdef _WIN32
	shutdown(socket, SD_BOTH);
	closesocket(socket);
#else
	shutdown(socket, SHUT_RDWR);
	close(socket);
#endif
}

bool HttpServer::RecvBuffer::isSecure()
{
	return ssl != nullptr;
}

void RecvBuffer::RecvData(std::function<int(RecvBuffer*, DualByteArray)> callback)
{
	fd_set rsockets;
	timeval timeout;
	const int nfds = socket + 1;
	recvstate = true;
	FD_ZERO(&rsockets);
	memset(&timeout, 0, sizeof(timeval));
	do
	{
		FD_SET(socket, &rsockets);
		if (select(nfds, &rsockets, nullptr, nullptr, &timeout) == -1) break;
		if ((FD_ISSET(socket, &rsockets) || (ssl != nullptr && SSL_pending((SSL*)ssl))) && (((writepos - readpos + capacity) % capacity) > blocksize || writepos == readpos))
		{
			//int n = std::min(blocksize, capacity - readpos);
			int n = ((readpos + blocksize) <= capacity) ? blocksize : (capacity - readpos);
			char * buf = buffer.Data() + readpos;
			if(ssl != nullptr)
			{
				n = SSL_read((SSL*)ssl, buf, n);
			}
			else
			{
				n = recv(socket, buf, n, 0);
			}
			if (n <= 0)
			{
				break;
			}
			readpos = (readpos + n) % capacity;
		}
		else if (writepos != readpos)
		{
			int n = ((writepos < readpos) ? callback(this, DualByteArray(ByteArray(buffer.Data(), writepos, readpos - writepos))) : callback(this, DualByteArray(ByteArray(buffer.Data(), writepos, capacity - writepos), ByteArray(buffer.Data(), 0, readpos))));
			writepos = (writepos + n) % capacity;
		}
		else
		{
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;
			continue;
		}
		memset(&timeout, 0, sizeof(timeval));
	} while (recvstate);
	recvstate = false;
}

bool RecvBuffer::RecvDataState()
{
	return recvstate;
}

void RecvBuffer::RecvDataState(bool recvstate)
{
	this->recvstate = recvstate;
}

int RecvBuffer::Send(const std::string &str)
{
	if (ssl != nullptr)
	{
		return SSL_write((SSL*)ssl, str.data(), str.length());
	}
	else
	{
		return send(socket, str.data(), str.length(), 0);
	}
}

int RecvBuffer::Send(std::istream &stream, int a, int b)
{
	using namespace std::chrono_literals;
	int sr = 0, sw = 0, ri = a, wi = a;
	std::atomic<bool> interrupt(true);
	std::thread readfileasync([this, &stream, &b, &sr, &sw, &ri, &interrupt]() {
		stream.seekg(ri, std::ios::beg);
		while (ri <= b && interrupt.load())
		{
			if (((sw - sr + capacity) % capacity) > blocksize || sw == sr)
			{
				ri = stream.read(buffer.Data() + (ri % capacity), std::min({blocksize, b - ri + 1, capacity - (ri % capacity) })).tellg();
				sr = ri % capacity;
			}
			else std::this_thread::sleep_for(1ms);
		}
	});
	fd_set wsockets;
	timeval timeout;
	FD_ZERO(&wsockets);
	memset(&timeout, 0, sizeof(timeval));
	while (wi <= b)
	{
		FD_SET(socket, &wsockets);
		if(select(socket + 1, nullptr, &wsockets, nullptr, &timeout) == -1) break;
		if (FD_ISSET(socket, &wsockets) && sw != sr)
		{
			int l1 = (sw < sr) ? (sr - sw) : (capacity - sw);
			const int &length = std::min(blocksize, l1);
			const char* buf = buffer.Data() + sw;
			int n = ssl == nullptr ? send(socket, buf, length, 0) : SSL_write((SSL*)ssl, buf, length);
			if (n < 0)
			{
				break;
			}
			else if(n == 0)
			{
				timeout.tv_sec = 0;
				timeout.tv_usec = 1000;
				continue;
			}
			else 
			{
				wi += n;
				sw = wi % capacity;
			}
		}
		else
		{
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;
			continue;
		}
		memset(&timeout, 0, sizeof(timeval));		
	}
	interrupt.store(false);
	readfileasync.join();
	return wi - a + 1;
}

int RecvBuffer::GetCapacity()
{
	return capacity;
}

int RecvBuffer::CopyTo(std::ostream & stream, int length)
{
	bool oldstate = recvstate;
	int i = 0;
	RecvData([&stream, &i, length, this](RecvBuffer *, DualByteArray array)
	{
		int n = std::min({ array.Length(), blocksize, length - i});
		ByteArray range(array.GetRange(0, array.Length()));
		stream.write(range.Data(), n);
		i += n;
		if (i >= length) recvstate = false;
		return n;
	});
	recvstate = oldstate;
	return i;
}