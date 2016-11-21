#include "HttpHeader.h"
#include "HttpRequestBuffer.h"
#include "HttpServer.h"
#include "ByteArray.h"
#include "utility.h"
#include <sstream>
#include <experimental/filesystem>
#include <list>
#include <fstream>
#include <iostream>

using namespace Http;
namespace fs = std::experimental::filesystem;

static const std::string htmltop("<!DOCTYPE html>\
<html>\
<head>\
	<meta charset=\"utf-8\"/>\
    <title>Datei Explorer</title>\
    <link rel=\"stylesheet\" href=\"/css/folder.css\" type=\"text/css\" />\
    <script src=\"/js/folder.js\" type=\"text/javascript\"></script>\
</head>\
<body>\
    <div id=\"header\">\
        <div>\
            <input type=\"file\" id=\"filebox\" multiple=\"multiple\" onchange=\"fileChange();\" />\
            <button type=\"button\" onclick=\"uploadfiles();\">Hochladen</button>\
            <button type=\"button\" onclick=\"uploadabort();\">Abbrechen</button>\
        </div>\
        <div>\
            <progress id=\"progressbar\"></progress>\
            <span id=\"status\"></span>\
        </div>\
    </div>");

static const std::string htmlbottom("</body>\
</html>");

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

std::unordered_map<std::string, std::string> benutzer;

int seite(RequestBuffer& buffer)
{
	auto& requestHeader = buffer.Request();
	if (requestHeader.methode == Get || requestHeader.methode == Head)
	{
		std::string serverPath(requestHeader.putValues["folder"]);
		fs::path folderPath = buffer.RootPath() / serverPath;
		if (!fs::is_directory(folderPath))
		{
			throw NotFoundException("Ordner existiert nicht");
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
		contentstream << htmltop;
		if (benutzer.size() > 0 && requestHeader.Exists("Cookie") && requestHeader["Cookie"].Exists("Sessionid") && benutzer.find(requestHeader["Cookie"]["Sessionid"]) != benutzer.end())
		{
			contentstream << "<div>Angemeldet als " << benutzer[requestHeader["Cookie"]["Sessionid"]] << "</div><br/>";
		}
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
			contentstream << "<div><a class=\"button button1\" href=\"" << serverPath << name << "\" target=\"_top\">" << name << "</a><a class=\"button button2\" href=\"" << serverPath << name << "?herunterladen\"></a></div>";
		}
		contentstream << htmlbottom;
		std::string content(contentstream.str());
		auto & responseHeader = buffer.Response();
		responseHeader.status = Ok;
		ByteArray buf(10);
		snprintf(buf.Data(), buf.Length(), "%zu", content.length());
		responseHeader["Content-Type"] = "text/html; charset=utf-8";
		responseHeader["Content-Length"] = buf.Data();
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
		if (contenttype.Exists("multipart/form-data"))
		{
			fs::path serverPath(buffer.RootPath() / requestHeader.putValues["folder"]);
			if (!fs::is_directory(serverPath))
			{
				throw NotFoundException("Ziel Ordner nicht gefunden");
			}
			struct
			{
				std::string contentSeperator;
				std::ofstream fileStream;
				fs::path serverPath;
			} args;
			args.contentSeperator = "--" + contenttype["boundary"];
			args.serverPath = std::move(serverPath);
			buffer.RecvData([&args](RequestBuffer &buffer) -> int
			{
				int contentSeperatorI = buffer.IndexOf(args.contentSeperator, 0);
				if (args.fileStream.is_open() && (contentSeperatorI == -1 || contentSeperatorI > 2))
				{
					const int data = contentSeperatorI == -1 ? (buffer.Length() - args.contentSeperator.length() + 1) : (contentSeperatorI - 2);
					buffer.CopyTo(args.fileStream, data);
					return contentSeperatorI == -1 ? 0 : 2;
				}
				else if (contentSeperatorI != -1)
				{
					int offset = contentSeperatorI + args.contentSeperator.length() + 2, fileHeader = buffer.IndexOf("\r\n\r\n", offset);
					if (fileHeader != -1)
					{
						buffer.Free(offset);
						if (args.fileStream.is_open())
						{
							args.fileStream.close();
						}
						std::string name = Parameter(std::string(buffer.begin(), buffer.begin() + fileHeader))["Content-Disposition"]["name"];
						args.fileStream.open((args.serverPath / name.substr(1, name.length() - 2)), std::ios::binary);
						return fileHeader + 4 - offset;
					}
					fileHeader = buffer.IndexOf("\r\n", offset);
					if (fileHeader != -1)
					{
						buffer.Free(offset);
						if (args.fileStream.is_open())
						{
							args.fileStream.close();
						}
						auto &responseHeader = buffer.Response();
						responseHeader.status = Ok;
						buffer.Send(responseHeader.toString());
						return ~(fileHeader + 2 - offset);
					}
				}
				return 0;
			});
			return 0;
		}
		else if (requestHeader["Content-Type"].Exists("application/x-www-form-urlencoded"))
		{
			unsigned long long contentlength = std::stoull(requestHeader["Content-Length"].toString());
			int un = buffer.IndexOf("UserName=", 0);
			std::string username(buffer.begin() + un + 9, buffer.begin() + buffer.IndexOf("&", un + 9));
			int up = buffer.IndexOf("Password=", un + 9);
			std::string password(buffer.begin() + up + 9, buffer.begin() + contentlength);
			ByteArray buf(33);
			for (int i = 0; i < 32; i++)
			{
				buf[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand() % 62];
			}
			buf[32] = 0;
			std::string sessionid(buf.Data());
			auto &responseHeader = buffer.Response();
			responseHeader["Set-Cookie"] = "Sessionid=" + sessionid + "; Secure; HttpOnly";
			responseHeader.status = SeeOther;
			responseHeader["Location"] = "/libordner.so";
			buffer.Send(responseHeader.toString());
			benutzer[sessionid] = Utility::urlDecode(username);
			return contentlength;
		}
	}
	return 0;
}