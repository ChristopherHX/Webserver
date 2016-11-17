#include "HttpRequestBuffer.h"
#include "ByteArray.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#undef min
#define SHUT_RDWR SD_BOTH
#else
#include <sys/socket.h>
#include <unistd.h> 
#include <dlfcn.h>
#include <sys/types.h>
#include <signal.h>
#define closesocket(socket) close(socket)
#endif
#include <algorithm>
#include <cstring>
#include <thread>
#include <atomic>
#include <iostream>

#include <openssl/ssl.h>
#include <openssl/err.h>

using namespace Http;
using namespace std::chrono_literals;

RequestBuffer::RequestBuffer(RequestBuffer && obj) : buffer(std::move(obj.buffer)), capacity(obj.capacity), readpos(obj.readpos), writepos(obj.writepos), blocksize(obj.blocksize), nfds(obj.nfds), client(obj.client), ssl(obj.ssl), rootPath(obj.rootPath)
{
	
}

RequestBuffer::RequestBuffer(uintptr_t socket, const int capacity, std::string client, void * ssl, std::experimental::filesystem::path & rootPath) : socket(socket), buffer(capacity), capacity(capacity), readpos(0), writepos(0), blocksize(capacity >> 2), nfds(socket + 1), client(client), ssl(ssl), rootPath(rootPath)
{
	
}

RequestBuffer::~RequestBuffer()
{
	if(isSecure()) SSL_free((SSL*)ssl);
	shutdown(socket, SHUT_RDWR);
	closesocket(socket);
}

bool RequestBuffer::isSecure()
{
	return ssl != nullptr;
}

void RequestBuffer::RecvData(const std::function<int(RequestBuffer&)> callback)
{
	fd_set rsockets;
	timeval timeout;
	FD_ZERO(&rsockets);
	memset(&timeout, 0, sizeof(timeval));
	while(true)
	{
		FD_SET(socket, &rsockets);
		if (select(nfds, &rsockets, nullptr, nullptr, &timeout) == -1) break;
		if ((FD_ISSET(socket, &rsockets) || (isSecure() && SSL_pending((SSL*)ssl))) && (((writepos - readpos + capacity) % capacity) > blocksize || writepos == readpos))
		{
			int n = std::min<unsigned int>(blocksize, capacity - readpos);
			char * offset = buffer.Data() + readpos;
			if ((n = (isSecure() ? SSL_read((SSL*)ssl, offset, n) : recv(socket, offset, n, 0))) <= 0)
			{
				break;
			}
			readpos = (readpos + n) % capacity;
		}
		else if (writepos != readpos)
		{
			int n = callback(*this);
			if(n < 0)
			{
				writepos = (writepos + ~n) % capacity;
				break;
			}
			writepos = (writepos + n) % capacity;
		}
		else
		{
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;
			continue;
		}
		memset(&timeout, 0, sizeof(timeval));
	}
}

void RequestBuffer::Free(int count)
{
	writepos = (writepos + count) % capacity;
}

int RequestBuffer::Send(const std::string &str)
{
	return isSecure() ? SSL_write((SSL*)ssl, str.data(), str.length()) : send(socket, str.data(), str.length(), 0);
}

int RequestBuffer::Send(const char* data, int length)
{
	return isSecure() ? SSL_write((SSL*)ssl, data, length) : send(socket, data, length, 0);
}

int RequestBuffer::Send(std::istream &stream, int a, int b)
{
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
		if(select(nfds, nullptr, &wsockets, nullptr, &timeout) == -1) break;
		if (FD_ISSET(socket, &wsockets) && sw != sr)
		{
			int n, l1 = (sw < sr) ? (sr - sw) : (capacity - sw);
			const int &length = std::min(blocksize, l1);
			const char* offset = buffer.Data() + sw;
			if ((n = (isSecure() ? SSL_write((SSL*)ssl, offset, length) : send(socket, offset, length, 0))) < 0)
			{
				break;
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

//void RequestBuffer::Redirect(std::string cmd, std::string args)
//{
//#ifdef _WIN32
//#else
//	struct { int rfd, wfd; } input, output;
//	pipe((int*)&input);
//	pipe((int*)&output);
//	int pid = fork();
//	if (pid == -1)
//	{
//		close(input.rfd);
//		close(output.rfd);
//		close(input.wfd);
//		close(output.wfd);
//	}
//	else if (pid == 0)
//	{
//		//dup2(output.wfd, 0);
//		//dup2(input.rfd, 1);
//		//close(input.rfd);
//		close(output.rfd);
//		close(input.wfd);
//		//close(output.wfd);
//		std::cout << (output.wfd, "<h1>Hallo Welt</h1>", 19);
//		//execl(cmd.data(), cmd.data(), args.data(), (char*)0);
//		_exit(0);
//	}
//	else
//	{
//		close(output.wfd);
//		close(input.rfd);
//		fd_set phpread;
//		timeval timeout;
//		FD_ZERO(&phpread);
//		memset(&timeout, 0, sizeof(timeval));
//		ByteArray buf(2048);
//		while (kill(pid, 0) == 0)
//		{
//			FD_SET(output.rfd, &phpread);
//			FD_SET(socket, &phpread);
//			if (select(std::max<int>(output.rfd + 1, nfds), &phpread, nullptr, nullptr, &timeout) == -1) break;
//			if (FD_ISSET(output.rfd, &phpread))
//			{
//				Send(buf.Data(), read(output.rfd, buf.Data(), buf.Length()));
//			}
//			else if (FD_ISSET(socket, &phpread) || (ssl != nullptr && SSL_pending((SSL*)ssl)))
//			{
//				write(input.wfd, buf.Data(), ((ssl == nullptr) ? recv(socket, buf.Data(), buf.Length(), 0) : SSL_read((SSL*)ssl, buf.Data(), buf.Length())));
//			}
//			else
//			{
//				timeout.tv_sec = 0;
//				timeout.tv_usec = 1000;
//				continue;
//			}
//			memset(&timeout, 0, sizeof(timeval));
//		}
//		close(input.rfd);
//		close(output.rfd);
//		close(input.wfd);
//		close(output.wfd);
//	}
//#endif
//}

const int &RequestBuffer::Capacity()
{
	return capacity;
}

int RequestBuffer::CopyTo(std::ostream & stream, int length)
{
	int i = 0;
	RecvData([&i, &stream, &length, blocksize = blocksize](RequestBuffer & buffer) -> int
	{
		ByteArray range(buffer.GetRange(0, buffer.SeqLength()));
		const int &n = std::min({ range.Length(), blocksize, length - i});
		stream.write(range.Data(), n);
		i += n;
		return i < length ? n : ~n;
	});
	return i;
}

const uintptr_t &RequestBuffer::GetSocket()
{
	return socket;
}

const std::string &RequestBuffer::Client()
{
	return client;
}

const int RequestBuffer::SeqLength()
{
	return (writepos < readpos ? writepos : capacity) - readpos;
}

const int RequestBuffer::Length()
{
	return (readpos - writepos + capacity) % capacity;
}

int RequestBuffer::IndexOf(const char * buffer, int length, int offset, int &mlength)
{
	int off = offset + writepos, l1 = SeqLength();
	bool farray1 = writepos < readpos, farray2 = offset < l1;
	int i = ByteArray(this->buffer.Data(), farray1 ? off : 0, farray1 ? (l1 - offset) : readpos).IndexOf(buffer, length, farray1 && farray2 ? 0 : (offset - l1), mlength);
	if (farray1)
	{
		if (mlength > 0)
		{
			if (memcmp(this->buffer.Data(), buffer + mlength, length - mlength) == 0)
			{
				mlength = length;
				return i;
			}
			mlength = 0;
			return -1;
		}
		i = ByteArray(this->buffer.Data(), 0, readpos).IndexOf(buffer, length, 0, mlength);
		return (i < 0 ? 0 : l1) + i;
	}
	return i;
}

int RequestBuffer::IndexOf(const char * string, int offset, int &mlength)
{
	return IndexOf(string, strlen(string), offset, mlength);
}

int RequestBuffer::IndexOf(std::string string, int offset, int &mlength)
{
	return IndexOf(string.data(), string.length(), offset, mlength);
}

ByteArray RequestBuffer::GetRange(int offset, int length)
{
	if (Length() < (offset + length))
	{
		RecvData([length = offset + length](RequestBuffer &buffer) -> int {
			if (buffer.Length() >= length)
			{
				return -1;
			}
			return 0;
		});
	}
	const int off = offset + writepos, l1 = SeqLength();
	if (l1 > offset)
	{
		if ((offset + length) > l1)
		{
			ByteArray newarray(length);
			int lp1 = l1 - offset;
			memcpy(newarray.Data(), buffer.Data() + off, lp1);
			memcpy(newarray.Data() + lp1, buffer.Data(), length - lp1);
			return std::move(newarray);
		}
		else
		{
			return ByteArray(this->buffer.Data(), off, length);
		}
	}
	else
	{
		return ByteArray(this->buffer.Data(), off - l1, length);
	}
}

std::experimental::filesystem::path & RequestBuffer::RootPath()
{
	return rootPath;
}

RequestHeader & RequestBuffer::Request()
{
	return request;
}

ResponseHeader & RequestBuffer::Response()
{
	return response;
}
