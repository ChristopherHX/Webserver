#include "HttpRequestBuffer.h"
#include "ByteArray.h"
#include "HttpServer.h"

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

RequestBuffer::RequestBuffer(RequestBuffer && obj) : buffer(std::move(obj.buffer)), capacity(obj.capacity), readpos(obj.readpos), writepos(obj.writepos), blocksize(obj.blocksize), nfds(obj.nfds), client(obj.client), ssl(obj.ssl), rootPath(obj.rootPath), iterator(obj.iterator)
{
	
}

RequestBuffer::RequestBuffer(uintptr_t socket, const int capacity, std::string client, void * ssl, std::experimental::filesystem::path & rootPath) : socket(socket), buffer(capacity), capacity(capacity), readpos(0), writepos(0), blocksize(capacity >> 2), nfds(socket + 1), client(client), ssl(ssl), rootPath(rootPath), iterator(buffer.Data(), capacity)
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
				ri = stream.read(buffer.Data() + (ri % capacity), std::min({blocksize, 1 + b - ri, capacity - (ri % capacity) })).tellg();
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
			int n;
			const int length = std::min(blocksize, (sw < sr) ? (sr - sw) : (capacity - sw));
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
		const int &n = std::min({ buffer.SeqLength(), blocksize, length - i});
		stream.write(buffer.begin().Pointer(), n);
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
	return (writepos < readpos ? readpos : capacity) - writepos;
}

const int RequestBuffer::Length()
{
	return (readpos - writepos + capacity) % capacity;
}

int RequestBuffer::IndexOf(const char * buffer, int length, int offset)
{
	RequestBufferIterator begin = this->begin(), end = this->end();
	RequestBufferIterator res = std::search(begin + offset, end, buffer, buffer + length);
	return res != end ? (res - begin) : -1;
	//std::search(this->buffer.Data(), this->buffer.Data() + this->buffer.Length(), buffer, buffer + length);
	//int seq1 = SeqLength();
	//bool zarray = seq1 < (offset + 1);s
	//bool zarrays = writepos >= readpos && readpos > 0;
	//if (zarray && !zarrays)
	//{
	//	RecvData([length = offset + 1](RequestBuffer &buffer) -> int {
	//		if (length < buffer.Length())
	//		{
	//			return -1;
	//		}
	//		return 0;
	//	});
	//	seq1 = SeqLength();
	//	zarray = seq1 < (offset + 1);
	//	zarrays = writepos >= readpos && readpos > 0;
	//	if (zarray && !zarrays)
	//	{
	//		throw ServerException(u8"Puffer Überlauf: Startindex plus Zeichenfolgenlänge ist zu groß");
	//	}
	//}
	//int i = ByteArray(this->buffer.Data(), zarray ? 0 : writepos, zarray ? readpos : seq1).IndexOf(buffer, length, zarray ? (offset - seq1) : offset, mlength);
	//if (length > mlength && zarrays)
	//{
	//	if (mlength > 0)
	//	{
	//		if (memcmp(this->buffer.Data(), buffer + mlength, length - mlength) == 0)
	//		{
	//			mlength = length;
	//			return i;
	//		}
	//		mlength = 0;
	//		return -1;
	//	}
	//	i = ByteArray(this->buffer.Data(), 0, readpos).IndexOf(buffer, length, 0, mlength);
	//	return std::max<unsigned int>(i, i + seq1);
	//}
	//return i;
}

int RequestBuffer::IndexOf(const char * string, int offset)
{
	return IndexOf(string, strlen(string), offset);
}

int RequestBuffer::IndexOf(std::string string, int offset)
{
	return IndexOf(string.data(), string.length(), offset);
}

//ByteArray RequestBuffer::GetRange(int offset, int length)
//{
//	const int minlength = offset + length;
//	if (Length() < minlength)
//	{
//		RecvData([minlength](RequestBuffer &buffer) -> int {
//			if (buffer.Length() >= minlength)
//			{
//				return -1;
//			}
//			return 0;
//		});
//	}
//	int seq = SeqLength();
//	if (offset < seq)
//	{
//		if (seq < minlength)
//		{
//			ByteArray newarray(length);
//			seq -= offset;
//			memcpy(newarray.Data(), buffer.Data() + writepos + offset, seq);
//			memcpy(newarray.Data() + seq, buffer.Data(), length - seq);
//			return std::move(newarray);
//		}
//		else
//		{
//			return ByteArray(this->buffer.Data(), writepos + offset, length);
//		}
//	}
//	else
//	{
//		return ByteArray(this->buffer.Data(), offset - seq, length);
//	}
//}

std::experimental::filesystem::path & RequestBuffer::RootPath()
{
	return rootPath;
}

Request & RequestBuffer::Request()
{
	return request;
}

Response & RequestBuffer::Response()
{
	return response;
}

RequestBufferIterator Http::RequestBuffer::begin()
{
	return iterator + writepos;
}

RequestBufferIterator Http::RequestBuffer::end()
{
	return iterator + readpos;
}

Http::RequestBufferIterator::RequestBufferIterator(pointer begin, pointer end, pointer pos) : begin(begin), end(end), pos(pos)
{
}

Http::RequestBufferIterator::RequestBufferIterator(pointer begin, pointer end) : RequestBufferIterator(begin, end, begin)
{
}

Http::RequestBufferIterator::RequestBufferIterator(pointer begin, int capacity) : RequestBufferIterator(begin, begin + capacity)
{
}

RequestBufferIterator::reference RequestBufferIterator::operator*()
{
	return *pos;
}

RequestBufferIterator &RequestBufferIterator::operator++()
{
	pos = (pos + 1) == end ? begin : (pos + 1);
	return *this;
}

bool RequestBufferIterator::operator==(const RequestBufferIterator &right)
{
	return pos == right.pos;
}

bool Http::RequestBufferIterator::operator!=(const RequestBufferIterator & right)
{
	return pos != right.pos;
}

RequestBufferIterator RequestBufferIterator::operator++(int)
{
	RequestBufferIterator val = *this;
	++(*this);
	return val;
}

RequestBufferIterator & RequestBufferIterator::operator--()
{
	pos = (pos == begin ? end : pos) - 1;
	return *this;
}

RequestBufferIterator RequestBufferIterator::operator--(int)
{
	RequestBufferIterator val = *this;
	--(*this);
	return val;
}


RequestBufferIterator & RequestBufferIterator::operator+=(int n)
{
	pos = ((end - begin + (pos - begin + n)) % (end - begin)) + begin;
	return *this;
}

RequestBufferIterator & Http::RequestBufferIterator::operator-=(int n)
{
	(*this) += -n;
	return *this;
}

RequestBufferIterator::reference RequestBufferIterator::operator[](int index)
{
	return *(*this + index);
}

bool Http::RequestBufferIterator::operator<(int right)
{
	return *pos < right;
}

bool Http::RequestBufferIterator::operator>(int right)
{
	return *pos > right;
}

bool Http::RequestBufferIterator::operator<=(int right)
{
	return *pos <= right;
}

bool Http::RequestBufferIterator::operator>=(int right)
{
	return *pos >= right;
}

RequestBufferIterator::pointer Http::RequestBufferIterator::Pointer()
{
	return pos;
}

RequestBufferIterator::difference_type Http::RequestBufferIterator::difference(const RequestBufferIterator & left, const RequestBufferIterator & right)
{
	return (left.pos - right.pos + left.end - left.begin) % (left.end - left.begin);
}

RequestBufferIterator std::operator+(const RequestBufferIterator & left, const int right)
{
	RequestBufferIterator val = left;
	val += right;
	return val;
}

RequestBufferIterator std::operator+(const int left, const RequestBufferIterator & right)
{
	RequestBufferIterator val = right;
	val += left;
	return val;
}

RequestBufferIterator::difference_type std::operator-(const RequestBufferIterator & left, const RequestBufferIterator & right)
{
	return RequestBufferIterator::difference(left,	right);
}

//template<>
//void std::swap(Http::RequestBufferIterator & left, Http::RequestBufferIterator & right)
//{
//	Http::RequestBufferIterator tmp = left;
//	left = right;
//	right = std::move(tmp);
//}