#pragma once
#include "Http.h"
#include "Array.h"

#include <cstdint>
#include <functional>
#include <string>
#include <istream>
#include <vector>
#include <experimental/filesystem>
#include <iterator>


namespace Http
{
	class RequestBufferIterator : public std::iterator<std::random_access_iterator_tag, char, long long, char*, char&>
	{
	private:
		pointer begin;
		pointer end;
		pointer pos;
		long long capacity, counter;
	public:
		RequestBufferIterator(pointer begin, pointer end, pointer pos);
		RequestBufferIterator(pointer begin, pointer end);
		RequestBufferIterator(pointer begin, long long capacity);
		reference operator*();
		bool operator==(const RequestBufferIterator & right);
		bool operator!=(const RequestBufferIterator & right);
		RequestBufferIterator & operator++();
		RequestBufferIterator & operator--();
		RequestBufferIterator operator++(int);
		RequestBufferIterator operator--(int);
		RequestBufferIterator & operator+=(long long right);
		RequestBufferIterator & operator-=(long long right);
		bool operator<(const RequestBufferIterator & right);
		bool operator>(const RequestBufferIterator & right);
		bool operator<=(const RequestBufferIterator & right);
		bool operator>=(const RequestBufferIterator & right);
		reference operator[](long long index);
		pointer Pointer();
		RequestBufferIterator::difference_type PointerReadableLength();
		RequestBufferIterator::difference_type PointerReadableLength(const RequestBufferIterator & other);
		RequestBufferIterator::difference_type PointerWriteableLength(const RequestBufferIterator & other);
		static RequestBufferIterator::difference_type difference(const RequestBufferIterator & left, const RequestBufferIterator & right);
	};

	class RequestBuffer
	{
	private:
		uintptr_t socket;
		std::vector<char> buffer;
		long long capacity;
		std::string client;
		void * ssl;
		const std::experimental::filesystem::path & rootPath;
		Request _request;
		Response _response;
		RequestBufferIterator readiter, writeiter;
	public:
		RequestBuffer(uintptr_t socket, long long capacity, std::string client, void * ssl, const std::experimental::filesystem::path &rootPath);
		RequestBuffer(RequestBuffer && other);
		RequestBuffer(RequestBuffer&) = delete;
		~RequestBuffer();
		bool isSecure();
		long long RecvData(std::function<long long(RequestBuffer&)> callback);
		long long Free(long long count);
		long long Send(const char* data, long long length);
		long long Send(const std::string &str);
		long long Send(std::istream &stream, long long offset, long long length);
		const long long &Capacity();
		long long MoveTo(std::ostream &stream, long long length);
		const uintptr_t &GetSocket();
		const std::string &Client();
		long long SeqLength();
		long long length();
		long long indexof(const char * buffer, long long length);
		long long indexof(std::string string);
		long long indexof(const char * buffer, long long length, long long offset);
		long long indexof(std::string string, long long offset);
		const std::experimental::filesystem::path &RootPath();
		Request &request();
		Response &response();
		const RequestBufferIterator &begin();
		const RequestBufferIterator &end();
	};
}

namespace std {
	Http::RequestBufferIterator operator+(const Http::RequestBufferIterator &left, long long right);
	Http::RequestBufferIterator operator+(long long left, const Http::RequestBufferIterator &right);
	Http::RequestBufferIterator operator-(const Http::RequestBufferIterator &left, long long right);
	Http::RequestBufferIterator::difference_type operator-(const Http::RequestBufferIterator & left, const Http::RequestBufferIterator & right);
}