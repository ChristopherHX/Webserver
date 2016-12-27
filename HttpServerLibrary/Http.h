#pragma once
#include <string>
#include <cstring>
#include <unordered_map>
#include <iterator>

class String
{
public:
	static std::string Replace(const std::string &str, const char * search, const char * replace);
};

namespace Http
{
	const std::string Ok("200 Ok");
	const std::string PartialContent("206 Partial Content");
	const std::string MovedPermanently("301 Moved Permanently");
	const std::string SeeOther("303 See Other");
	const std::string NotModified("304 Not Modified");
	const std::string NotFound("404 NotFound");
	const std::string RangeNotSatisfiable("416 Range Not Satisfiable");
	const std::string InternalServerError("500 Internal Server Error");
	const std::string NotImplemented("501 Not Implemented");

	const std::string Get("GET");
	const std::string Head("HEAD");
	const std::string Post("POST");

	void CreateServerSocket(uintptr_t & serverSocket, int port);
	void CloseSocket(uintptr_t & socket);

	class Values
	{
	private:
		std::unordered_map<std::string, std::string> values;
	public:
		Values();
		Values(const std::string &values);
		Values(const char* values);
		std::string &operator[](const std::string &key);
		bool Exists(std::string key);
		std::string toString();
	};

	class Parameter
	{
	private:
		std::unordered_map<std::string, Values> parameter;
	public:
		Parameter();
		Parameter(std::string paramstr);
		Values &operator[](const std::string &key);
		bool Exists(std::string key);
		void Clear();
		std::string toString();
	};

	class Request : public Parameter
	{
	public:
		Request();
		Request(std::string headerstr);
		std::string toString();
		std::string methode;
		std::string request;
		Values putValues;
		double version;
	};

	class Response : public Parameter
	{
	public:
		Response();
		Response(std::string headerstr);
		std::string toString();
		std::string status;
	};
}