#include "HttpRequestBuffer.h"
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
#include <condition_variable>

#include <openssl/ssl.h>
#include <openssl/err.h>

using namespace Http;
using namespace std::chrono_literals;

Http::RequestBufferIterator::RequestBufferIterator(pointer begin, pointer end, pointer pos) : begin(begin), end(end), pos(pos), capacity(end - begin), counter(0)
{
}

Http::RequestBufferIterator::RequestBufferIterator(pointer begin, pointer end) : RequestBufferIterator(begin, end, begin)
{
}

Http::RequestBufferIterator::RequestBufferIterator(pointer begin, long long capacity) : RequestBufferIterator(begin, begin + capacity)
{
}

RequestBufferIterator::reference RequestBufferIterator::operator*()
{
	return *pos;
}

bool RequestBufferIterator::operator==(const RequestBufferIterator &right)
{
	return pos == right.pos && counter == right.counter;
}

bool Http::RequestBufferIterator::operator!=(const RequestBufferIterator & right)
{
	return pos != right.pos || counter != right.counter;
}

RequestBufferIterator &RequestBufferIterator::operator++()
{
	if (++pos == end)
	{
		pos = begin;
		++counter;
	}
	return *this;
}

RequestBufferIterator & RequestBufferIterator::operator--()
{
	if (pos == begin)
	{
		pos = end - 1;
		--counter;
	}
	else
	{
		--pos;
	}
	return *this;
}

RequestBufferIterator RequestBufferIterator::operator++(int)
{
	RequestBufferIterator val = *this;
	++(*this);
	return val;
}

RequestBufferIterator RequestBufferIterator::operator--(int)
{
	RequestBufferIterator val = *this;
	--(*this);
	return val;
}


RequestBufferIterator & RequestBufferIterator::operator+=(long long n)
{
	pos += n;
	while (pos >= end)
	{
		pos -= capacity;
		++counter;
	}
	while (pos < begin)
	{
		pos += capacity;
		--counter;
	}
	return *this;
}

RequestBufferIterator & Http::RequestBufferIterator::operator-=(long long n)
{
	pos -= n;
	while (pos >= end)
	{
		pos -= capacity;
		++counter;
	}
	while (pos < begin)
	{
		pos += capacity;
		--counter;
	}
	return *this;
}

RequestBufferIterator::reference RequestBufferIterator::operator[](long long index)
{
	char * ipos = pos + index;
	while (ipos >= end)
	{
		ipos -= capacity;
	}
	while (ipos < begin)
	{
		ipos += capacity;
	}
	return *ipos;
}

bool Http::RequestBufferIterator::operator<(const RequestBufferIterator& right)
{
	return counter < right.counter || (pos < right.pos && counter == right.counter);
}

bool Http::RequestBufferIterator::operator>(const RequestBufferIterator& right)
{
	return counter > right.counter || (pos > right.pos && counter == right.counter);
}

bool Http::RequestBufferIterator::operator<=(const RequestBufferIterator& right)
{
	return counter < right.counter || (pos <= right.pos && counter == right.counter);
}

bool Http::RequestBufferIterator::operator>=(const RequestBufferIterator& right)
{
	return counter > right.counter || (pos >= right.pos && counter == right.counter);
}

RequestBufferIterator::pointer Http::RequestBufferIterator::Pointer()
{
	return pos;
}

RequestBufferIterator::difference_type Http::RequestBufferIterator::PointerReadableLength()
{
	return end - pos;
}

RequestBufferIterator::difference_type Http::RequestBufferIterator::PointerReadableLength(const RequestBufferIterator& other)
{
	return other.pos <= pos && (counter + 1) == other.counter ? end - pos : (pos < other.pos && counter == other.counter ? other.pos - pos : 0);
}

RequestBufferIterator::difference_type Http::RequestBufferIterator::PointerWriteableLength(const RequestBufferIterator& other)
{
	return other.pos <= pos && counter == other.counter ? end - pos : (pos < other.pos && counter == (other.counter + 1) ? other.pos - pos : 0);
}

RequestBufferIterator::difference_type Http::RequestBufferIterator::difference(const RequestBufferIterator & left, const RequestBufferIterator & right)
{
	return (left.counter - right.counter) * left.capacity + left.pos - right.pos;
}

RequestBuffer::RequestBuffer(RequestBuffer && other) : buffer(std::move(other.buffer)), capacity(other.capacity), readiter(other.readiter), writeiter(other.writeiter), client(other.client), ssl(other.ssl), rootPath(other.rootPath)
{

}

Http::RequestBuffer::RequestBuffer(uintptr_t socket, long long capacity, std::string client, void * ssl, const std::experimental::filesystem::path &rootPath) : socket(socket), buffer(capacity), capacity(capacity), readiter(buffer.data(), capacity), writeiter(buffer.data(), capacity), client(client), ssl(ssl), rootPath(rootPath)
{
}

RequestBuffer::~RequestBuffer()
{
	if (isSecure()) SSL_free((SSL*)ssl);
	shutdown(socket, SHUT_RDWR);
	closesocket(socket);
}

bool RequestBuffer::isSecure()
{
	return ssl != nullptr;
}

long long RequestBuffer::RecvData(std::function<long long(RequestBuffer&)> callback)
{
	long long length;
	fd_set socketfd;
	timeval timeout{ 0, 0 };
	FD_ZERO(&socketfd);
	while (true)
	{
		FD_SET(socket, &socketfd);
		select(socket + 1, &socketfd, nullptr, nullptr, &timeout);
		length = readiter.PointerWriteableLength(writeiter);
		if ((FD_ISSET(socket, &socketfd) || (isSecure() && SSL_pending((SSL*)ssl))) && length > 0)
		{
			if ((length = isSecure() ? SSL_read((SSL*)ssl, readiter.Pointer(), length) : recv(socket, readiter.Pointer(), length, 0)) <= 0) return -1;
			readiter += length;
		}
		else if (writeiter != readiter)
		{
			if ((length = callback(*this)) < 0)
			{
				writeiter += ~length;
				return 0;
			}
			writeiter += length;
		}
		else
		{
			timeout = { 1, 0 };
			continue;
		}
		timeout = { 0, 0 };
	}
	return 0;
}

long long RequestBuffer::Free(long long count)
{
	writeiter += count;
	return count;
}

long long RequestBuffer::Send(const std::string &str)
{
	return isSecure() ? SSL_write((SSL*)ssl, str.data(), str.length()) : send(socket, str.data(), str.length(), 0);
}

long long RequestBuffer::Send(const char* data, long long length)
{
	return isSecure() ? SSL_write((SSL*)ssl, data, length) : send(socket, data, length, 0);
}

long long RequestBuffer::Send(std::istream &stream, long long offset, long long length)
{
	if (readiter != writeiter)
	{
		throw std::runtime_error("readiter != writeiter, RequestBuffer::Send Nicht alles wurde gelesen");
	}
	long long read = 0, sent = 0;
	std::atomic<bool> interrupt(true);
	/*std::condition_variable cv;
	std::mutex mtx;*/
	stream.seekg(offset, std::ios::beg);
	std::thread readfileasync([this, /*&cv, &mtx,*/ &stream, &length, &read, &interrupt]() {
		try {
			long long count;
			while (read < length && interrupt.load())
			{
				if ((count = std::min(length - read, readiter.PointerWriteableLength(writeiter))) > 0)
				{
					stream.read(readiter.Pointer(), count);
					readiter += count;
					//cv.notify_one();
					read += count;
				}
				//else
				//{
				//	std::this_thread::sleep_for(1ms);
				//	//cv.notify_one();
				//	//cv.wait(std::unique_lock<std::mutex>(mtx));
				//}
			}
		}
		catch (const std::exception &ex)
		{

		}
		interrupt.store(false);
	});
	long long count;
	while (sent < length || interrupt.load() || writeiter != readiter)
	{
		if ((count = writeiter.PointerReadableLength(readiter)) > 0)
		{
			if ((count = isSecure() ? SSL_write((SSL*)ssl, writeiter.Pointer(), count) : send(socket, writeiter.Pointer(), count, 0)) <= 0)
			{
				interrupt.store(false);
				readfileasync.join();
				throw std::runtime_error("RequestBuffer::Send(std::istream &stream, long long offset, long long length) Verbindungs Fehler");
			}
			writeiter += count;
			//cv.notify_one();
			sent += count;
		}
		//else
		//{
		//	std::this_thread::sleep_for(1ms);
		//	//cv.wait(std::unique_lock<std::mutex>(mtx));
		//}
	}
	interrupt.store(false);
	readfileasync.join();
	if (readiter != writeiter) throw std::runtime_error("readiter != writeiter, RequestBuffer::Send Nicht mehr Syncron");
	return sent;
}

const long long &RequestBuffer::Capacity()
{
	return capacity;
}

long long RequestBuffer::MoveTo(std::ostream & stream, long long length)
{
	long long i = 0;
	RecvData([&i, &stream, length, &writeiter = writeiter](RequestBuffer & buffer) -> long long
	{
		long long n = std::min(buffer.SeqLength(), length - i);
		stream.write(writeiter.Pointer(), n);
		return (i += n) < length ? n : ~n;
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

long long RequestBuffer::SeqLength()
{
	return writeiter.PointerReadableLength(readiter);
}

long long RequestBuffer::length()
{
	return readiter - writeiter;
}

long long Http::RequestBuffer::indexof(const char * buffer, long long length)
{
	RequestBufferIterator res = std::search(writeiter, readiter, buffer, buffer + length);
	return res != readiter ? (res - writeiter) : -1;
}

long long Http::RequestBuffer::indexof(std::string string)
{
	RequestBufferIterator res = std::search(writeiter, readiter, string.begin(), string.end());
	return res != readiter ? (res - writeiter) : -1;
}

long long RequestBuffer::indexof(const char * buffer, long long  length, long long  offset)
{
	RequestBufferIterator res = std::search(writeiter + offset, readiter, buffer, buffer + length);
	return res != readiter ? (res - writeiter) : -1;
}

long long RequestBuffer::indexof(std::string string, long long  offset)
{
	RequestBufferIterator res = std::search(writeiter + offset, readiter, string.begin(), string.end());
	return res != readiter ? (res - writeiter) : -1;
}

const std::experimental::filesystem::path & RequestBuffer::RootPath()
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

const RequestBufferIterator &Http::RequestBuffer::begin()
{
	return writeiter;
}

const RequestBufferIterator &Http::RequestBuffer::end()
{
	return readiter;
}

RequestBufferIterator std::operator+(const RequestBufferIterator & left, long long right)
{
	RequestBufferIterator val = left;
	val += right;
	return val;
}

RequestBufferIterator std::operator+(long long left, const RequestBufferIterator & right)
{
	RequestBufferIterator val = right;
	val += left;
	return val;
}

Http::RequestBufferIterator std::operator-(const Http::RequestBufferIterator & left, long long right)
{
	RequestBufferIterator val = left;
	val -= right;
	return val;
}

RequestBufferIterator::difference_type std::operator-(const RequestBufferIterator & left, const RequestBufferIterator & right)
{
	return RequestBufferIterator::difference(left, right);
}