#include "HttpHeader.h"
#include "RecvBuffer.h"
#include "HttpServer.h"
#include "ByteArray.h"
#include <sstream>
#include <experimental/filesystem>
#include <list>

using namespace Http;
using namespace HttpServer;
namespace fs = std::experimental::filesystem;

bool compare_nocase(const std::string& first, const std::string& second)
{
	unsigned int i = 0;
	while ((i<first.length()) && (i<second.length()))
	{
		if (tolower(first[i])<tolower(second[i])) return true;
		else if (tolower(first[i])>tolower(second[i])) return false;
		++i;
	}
	return (first.length() < second.length());
}

int seite(RequestHeader& requestHeader, ResponseHeader& responseHeader, RecvBuffer* buffer, DualByteArray& array)
{
	std::string serverPath(ParameterValues(String::Replace(std::move(requestHeader.putPath), "%2F", "/"))["folder"]);
	fs::path folderPath = "../" + serverPath;
	if (!fs::is_directory(folderPath))
	{
		throw ServerException("Ordner existiert nicht");
	}
	std::list<std::string> folder;
	std::list<std::string> files;
	for (fs::directory_entry entry : fs::directory_iterator(folderPath))
	{
		std::string name(entry.path().filename().u8string());
		fs::file_status status(entry.status());
		switch (status.type())
		{
		case fs::file_type::regular:
			files.push_back(name);
			break;
		case fs::file_type::directory:
			folder.push_back(name);
			break;
		default:
			break;
		}
	}
	std::stringstream contentstream;
	contentstream << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/><link rel=\"stylesheet\" href=\"/css/folder.css\" type=\"text/css\"/></head><body><div id=\"path\">" << serverPath << "</div>";
	if (serverPath != "/") contentstream << "<div><a class=\"button button3\" href=\"/folderview.html?folder=" << serverPath.substr(0, serverPath.find_last_of('/', serverPath.length() - 2)) << "/\" target=\"_self\">..</button></div>";
	folder.sort(compare_nocase);
	for (auto &name : folder)
	{
		contentstream << "<div><a class=\"button button3\" href=\"/folderview.html?folder=" << serverPath << name << "/\" target=\"_self\">" << name << "</button></div>";
	}
	files.sort(compare_nocase);
	for (auto &name : files)
	{
		contentstream << "<div><a class=\"button button1\" href=\"" << serverPath << name << "\" target=\"_top\">" << name << "</a><a class=\"button button2\" href=\"" << serverPath << name << "?download\"></a></div>";
	}
	contentstream << "</body>";
	std::string content(contentstream.str());
	responseHeader.status = Ok;
	ByteArray buf(10);
	snprintf(buf.Data(), buf.Length(), "%zu", content.length());
	responseHeader["Content-Type"] = "text/html; charset=utf-8";
	responseHeader["Content-Length"] = buf.Data();
	responseHeader["Cache-Control"] = "no-store, must-revalidate";
	buffer->Send(responseHeader.toString());
	if (requestHeader.httpMethode != "HEAD")
	{
		buffer->Send(content);
	}
	return 0;
}