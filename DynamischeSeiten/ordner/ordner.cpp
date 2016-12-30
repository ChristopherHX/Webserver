#include "Http.h"
#include "HttpRequestBuffer.h"
#include "HttpServer.h"
#include "Array.h"
#include "utility.h"
#include <sstream>
#include <experimental/filesystem>
#include <list>
#include <fstream>
#include <iostream>

using namespace Http;
using namespace Utility;
namespace fs = std::experimental::filesystem;

static const std::string htmltop("<!DOCTYPE html>\
<html>\
<head>\
	\
    <title>Dateien</title>\
    <link rel=\"stylesheet\" href=\"/css/folder.css\" type=\"text/css\" />\
    <script src=\"/js/folder.js\" type=\"text/javascript\"></script>\
</head>\
<body>\
    <div id=\"header\">\
		<input type=\"file\" id=\"filebox\" multiple=\"multiple\"/>\
		<button type=\"button\" onclick=\"uploadfiles();\">Hochladen</button>\
		<button type=\"button\" onclick=\"uploadabort();\">Abbrechen</button>\
		<progress id=\"progressbar\"></progress>\
		<span id=\"status\"></span>\
    </div>");

static const std::string htmlbottom("</body>\
</html>");

static std::unordered_map<std::string, std::string> benutzer;

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

int seite(RequestBuffer& buffer)
{
	auto& requestHeader = buffer.request();
	if (requestHeader.methode == Get || requestHeader.methode == Head)
	{
		std::string content;
		if (benutzer.size() > 0 && requestHeader.Exists("Cookie") && requestHeader["Cookie"].Exists("Sessionid") && benutzer.find(requestHeader["Cookie"]["Sessionid"]) != benutzer.end())
		{
			std::string serverPath(requestHeader.putValues["folder"]);
			fs::path folderPath = buffer.RootPath() / serverPath;
			if (!fs::is_directory(folderPath))
			{
				throw NotFoundError("Ordner existiert nicht");
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
			std::ostringstream contentstream;
			contentstream << htmltop;
			contentstream << "<div>Angemeldet als " << benutzer[requestHeader["Cookie"]["Sessionid"]] << "</div><br/>";
			contentstream << "<div id=\"path\">" << serverPath << "</div>";
			if (serverPath != "/") contentstream << "<div><a class=\"button button3\" href=\"/libordner.so?folder=" << serverPath.substr(0, serverPath.find_last_of('/', serverPath.length() - 2)) << "/\" target=\"_self\">..</button></div>";
			folder.sort(compare_nocase);
			for (auto &name : folder)
			{
				contentstream << "<div><a class=\"button button3\" href=\"/libordner.so?folder=" << serverPath << name << "/\" target=\"_self\">" << name << "</button></div>";
			}
			files.sort(compare_nocase);
			for (auto &name : files)
			{
				contentstream << "<div><a class=\"button button1\" href=\"" << serverPath << name << "\" target=\"_self\">" << name << "</a><a class=\"button button2\" href=\"" << serverPath << name << "?herunterladen\"></a></div>";
			}
			contentstream << htmlbottom;
			content = std::move(contentstream.str());
		}
		else
		{
			content = "<!DOCTYPE html><html><head><title>Zugriff verweigert</title></head><body><h1>Zugriff verweigert</h1>\
			<form method=\"POST\" action=\"/libordner.so?anmelden\" enctype=\"application/x-www-form-urlencoded\">\
				<input type=\"text\" placeholder=\"Benutzer\" name=\"Benutzer\"/>\
				<input type=\"password\" placeholder=\"Passwort\" name=\"Passwort\"/>\
				<button type=\"submit\">Anmelden</button>\
			</form></body></html>";
		}
		auto & responseHeader = buffer.response();
		responseHeader.status = Ok;
		responseHeader["Content-Type"] = "text/html; charset=utf-8";
		responseHeader["Content-Length"] = std::to_string(content.length());
		responseHeader["Cache-Control"] = "no-store, must-revalidate";
		buffer.Send(responseHeader.toString());
		if (requestHeader.methode != Head)
		{
			buffer.Send(content);
		}
	}
	else if (requestHeader.methode == Post)
	{
		Values& contenttype = requestHeader["Content-Type"];
		if (contenttype.Exists("multipart/form-data") && requestHeader.putValues.Exists("folder") && benutzer.size() > 0 && requestHeader.Exists("Cookie") && requestHeader["Cookie"].Exists("Sessionid") && benutzer.find(requestHeader["Cookie"]["Sessionid"]) != benutzer.end())
		{
			fs::path serverPath(buffer.RootPath() / requestHeader.putValues["folder"]);	
			if (!fs::is_directory(serverPath))
			{
				throw NotFoundError("Ziel Ordner nicht gefunden");
			}
			struct
			{
				std::string contentSeperator;
				std::ofstream fileStream;
				fs::path serverPath;
			} args;
			args.contentSeperator = "--" + contenttype["boundary"];
			args.serverPath = std::move(serverPath);
			buffer.RecvData([&args](RequestBuffer &buffer) -> long long
			{
				int contentSeperatorI = buffer.indexof(args.contentSeperator);
				if (args.fileStream.is_open() && (contentSeperatorI == -1 || contentSeperatorI > 2))
				{
					const int data = contentSeperatorI == -1 ? (buffer.length() - args.contentSeperator.length() + 1) : (contentSeperatorI - 2);
					buffer.MoveTo(args.fileStream, data);
					return contentSeperatorI == -1 ? 0 : 2;
				}
				else if (contentSeperatorI != -1)
				{
					int offset = contentSeperatorI + args.contentSeperator.length(), fileHeader = buffer.indexof("\r\n\r\n", 4, offset);
					if (fileHeader != -1)
					{
						if (args.fileStream.is_open())
						{
							args.fileStream.close();
						}
						std::string name = Parameter(std::string(buffer.begin() + offset + 2, buffer.begin() + fileHeader))["Content-Disposition"]["name"];
						args.fileStream.open((args.serverPath / name.substr(1, name.length() - 2)), std::ios::binary);
						return fileHeader + 4;
					}
					fileHeader = buffer.indexof("--\r\n", 4, offset);
					if (fileHeader != -1)
					{
						if (args.fileStream.is_open())
						{
							args.fileStream.close();
						}
						auto &responseHeader = buffer.response();
						responseHeader.status = Ok;
						buffer.Send(responseHeader.toString());
						return ~(fileHeader + 4);
					}
				}
				return 0;
			});
			return 0;
		}
		else if (requestHeader["Content-Type"].Exists("application/x-www-form-urlencoded") && requestHeader.putValues.Exists("anmelden"))
		{
			unsigned long long contentlength = std::stoull(requestHeader["Content-Length"].toString());
			int un = buffer.indexof("Benutzer=");
			std::string username(buffer.begin() + un + 9, buffer.begin() + buffer.indexof("&", 1, un + 9));
			int up = buffer.indexof("Passwort=", 9, un + 9);
			std::string password(buffer.begin() + up + 9, buffer.begin() + contentlength);
			Array<char> buf(33);
			for (int i = 0; i < 32; i++)
			{
				buf[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand() % 62];
			}
			buf[32] = 0;
			std::string sessionid(buf.data());
			auto &responseHeader = buffer.response();
			responseHeader["Set-Cookie"]["Sessionid"] = sessionid;
			responseHeader["Set-Cookie"]["HttpOnly"];
			responseHeader["Set-Cookie"]["Secure"];
			responseHeader.status = SeeOther;
			responseHeader["Location"] = "/libordner.so?folder=/";
			buffer.Send(responseHeader.toString());
			benutzer[sessionid] = Utility::urlDecode(String::Replace(username, "+", " "));
			return contentlength;
		}
		else
		{
			throw ServerError("Nicht berechtigt");
		}
	}
	return 0;
}