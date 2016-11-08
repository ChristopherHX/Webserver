#pragma once
#include <cstring>
#include <map>

class String
{
public:
	static std::string &Replace(std::string &&str, const char * search, const char * replace);
};

namespace Http
{
	const std::string Ok("200 Ok");
	const std::string NotFound("404 NotFound");
	const std::string NotImplemented("501 Not Implemented");
	const std::string SeeOther("303 See Other");
	const std::string MovedPermanently("301 Moved Permanently");
	const std::string NotModified("304 Not Modified");
	const std::string PartialContent("206 Partial Content");
	const std::string RangeNotSatisfiable("416 Range Not Satisfiable");

	class ParameterValues
	{
	public:
		ParameterValues(std::string valuestr);
		std::string &operator[](const std::string &key);
		bool Exists(std::string key);
	private:
		std::map<std::string, std::string> values;
	};

	class Parameter
	{
	public:
		Parameter();
		Parameter(std::string paramstr);
		std::string &operator[](const std::string &key);
		bool Exists(std::string key);
		std::string toString();
	private:
		std::map<std::string, std::string> parameter;
	};

	class RequestHeader : public Parameter
	{
	public:
		RequestHeader();
		RequestHeader(const char * header, int length);
		RequestHeader(std::string headerstr);
		std::string toString();
		std::string httpMethode;
		std::string requestPath;
		std::string putPath;
	};

	class ResponseHeader : public Parameter
	{
	public:
		ResponseHeader();
		ResponseHeader(std::string headerstr);
		std::string toString();
		std::string status;
	};
}