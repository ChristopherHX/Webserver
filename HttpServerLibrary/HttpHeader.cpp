#include "HttpHeader.h"
#include <sstream>

using namespace Http;

std::string &String::Replace(std::string &&str, const char * search, const char * replace)
{
	for (int pos = str.find(search); pos < str.length() && pos != std::string::npos; pos = str.find(search, pos))
	{
		str = str.replace(pos, strlen(search), replace);
		pos += strlen(replace);
	}
	return str;
}

Parameter::Parameter()
{
}

Parameter::Parameter(std::string paramstr)
{
	for (int i = 0; i != std::string::npos + 2;)
	{
		int lineend = paramstr.find("\r\n", i);
		int dpI = paramstr.find(": ", i);
		if (dpI != std::string::npos && (lineend == std::string::npos || dpI < lineend))
		{
			std::string key = paramstr.substr(i, dpI - i);
			if(parameter.find(key) == parameter.end())
				parameter[key] = paramstr.substr(dpI + 2, (lineend == std::string::npos ? paramstr.length() : lineend) - (dpI + 2));
		}
		i = lineend + 2;
	}
}

std::string & Parameter::operator[](const std::string &key)
{
	return parameter[key];
}

bool Http::Parameter::Exists(std::string key)
{
	return parameter.find(key) != parameter.end();
}

std::string Parameter::toString()
{
	std::stringstream ss;
	for (auto &pair : parameter)
	{
		ss << pair.first << ": " << pair.second << "\r\n";
	}
	return ss.str();
}

RequestHeader::RequestHeader()
{
}

RequestHeader::RequestHeader(const char * header, int length) : RequestHeader(std::string(header, length))
{
}

RequestHeader::RequestHeader(std::string headerstr) : Parameter(headerstr.substr(headerstr.find("\r\n") + 2))
{
	int patha = headerstr.find(" ");
	httpMethode = headerstr.substr(0, patha);
	int pathb = headerstr.find(" ", patha + 1);
	requestPath = String::Replace(headerstr.substr(patha + 1, pathb - (patha + 1)), "%20", " ");
	int putI = requestPath.find("?");
	putPath = putI == std::string::npos ? "" : requestPath.substr(putI + 1);
	requestPath = putI == std::string::npos ? requestPath : requestPath.substr(0, putI);
}

std::string RequestHeader::toString()
{
	return httpMethode + " " + String::Replace(requestPath + (putPath == "" ? "" : ("?" + putPath)), " ", "%20") + " HTTP/1.1\r\n" + Parameter::toString() + "\r\n";
}

ResponseHeader::ResponseHeader()
{
}

ResponseHeader::ResponseHeader(std::string headerstr) : Parameter(headerstr.substr(headerstr.find("\r\n") + 2))
{
	int spaceI = headerstr.find(" ");
	status = headerstr.substr(spaceI, headerstr.find("\r\n") - spaceI);
}

std::string ResponseHeader::toString()
{
	return "HTTP/1.1 " + status + "\r\n" + Parameter::toString() + "\r\n";
}

ParameterValues::ParameterValues(std::string valuestr)
{
	for (int i = 0; i != std::string::npos + 2;)
	{
		int lineend = valuestr.find("; ", i);
		int dpI = valuestr.find("=", i);
		if (dpI != std::string::npos && (lineend == std::string::npos || dpI < lineend))
		{
			std::string key = valuestr.substr(i, dpI - i);
			if(values.find(key) == values.end())
				values[key] = valuestr.substr(dpI + 1, (lineend == std::string::npos ? valuestr.length() : lineend) - (dpI + 1));
		}
		i = lineend + 2;
	}
}

std::string & ParameterValues::operator[](const std::string & key)
{
	return values[key];
}

bool Http::ParameterValues::Exists(std::string key)
{
	return values.find(key) != values.end();
}
