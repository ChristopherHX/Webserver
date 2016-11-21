#pragma once
#include "ByteArray.h"
#include "HttpHeader.h"

#include <cstdint>
#include <functional>
#include <string>
#include <istream>
#include <vector>
#include <experimental/filesystem>
#include <iterator>


namespace Http
{
	class RequestBufferIterator : public std::iterator<std::random_access_iterator_tag, char, int, char*, char&>
	{
	private:
		pointer begin;
		pointer end;
		pointer pos;
	public:
		RequestBufferIterator(pointer begin, pointer end, pointer pos);
		RequestBufferIterator(pointer begin, pointer end);
		RequestBufferIterator(pointer begin, int capacity);
		reference operator*();
		RequestBufferIterator & operator++();
		bool operator==(const RequestBufferIterator & right);
		bool operator!=(const RequestBufferIterator & right);
		RequestBufferIterator operator++(int);
		RequestBufferIterator operator--(int);
		RequestBufferIterator & operator--();
		RequestBufferIterator & operator+=(int right);
		RequestBufferIterator & operator-=(int right);
		reference operator[](int index);
		bool operator<(int right);
		bool operator>(int right);
		bool operator<=(int right);
		bool operator>=(int right);
		pointer Pointer();
		static RequestBufferIterator::difference_type difference(const RequestBufferIterator & left, const RequestBufferIterator & right);
	};

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
		Request request;
		Response response;
		RequestBufferIterator iterator;
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
		const int &Capacity();
		int CopyTo(std::ostream &stream, int length);
		const uintptr_t &GetSocket();
		const std::string &Client();
		const int SeqLength();
		const int Length();
		int IndexOf(const char * buffer, int length, int offset);
		int IndexOf(const char * string, int offset);
		int IndexOf(std::string string, int offset);
		//ByteArray GetRange(int offset, int length);
		std::experimental::filesystem::path &RootPath();
		Request &Request();
		Response &Response();
		RequestBufferIterator begin();
		RequestBufferIterator end();
	};
}

namespace std {
	Http::RequestBufferIterator operator+(const Http::RequestBufferIterator &left, const int right);
	Http::RequestBufferIterator operator+(const int left, const Http::RequestBufferIterator &right);
	Http::RequestBufferIterator::difference_type operator-(const Http::RequestBufferIterator & left, const Http::RequestBufferIterator & right);
	//template<>
	//void swap<Http::RequestBufferIterator &, void>(Http::RequestBufferIterator & left, Http::RequestBufferIterator & right);
}