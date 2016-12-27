#include "Http2.h"
#include "Http.h"
#include "hpack.h"
#include "utility.h"

#include <thread>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <atomic>
#include <openssl/ssl.h>
#include <openssl/err.h>

using namespace Http2;
using namespace Http2::HPack;
using namespace Utility;
using namespace std::chrono_literals;
namespace fs = std::experimental::filesystem;

std::pair<fs::path, std::string> Http2::MimeTypeTable[] = {
	{ ".html" , "text/html; charset=utf-8" },
	{ ".css" , "text/css; charset=utf-8" },
	{ ".js", "text/javascript; charset=utf-8" },
	{ ".xml" , "text/xml; charset=utf-8" },
	{ ".svg", "text/svg; charset=utf-8" },
	{ ".txt" , "text/plain; charset=utf-8" },
	{ ".png" , "image/png" },
	{ ".jpg" , "image/jpeg" },
	{ ".tiff" , "image/tiff" },
	{ ".fif", "image/fif" },
	{ ".ief" , "image/ief" },
	{ ".gif", "image/gif" },
	{ ".pdf", "application/pdf" },
	{ ".mpg", "video/mpeg"},
	{"", "application/octet-stream" }
};

bool StreamSearch(const Stream & stream, uint32_t streamIndentifier)
{
	return stream.streamIndentifier == streamIndentifier;
}

//std::string Http2::HuffmanEncoder(std::string source)
//{
//	std::ostringstream strs;
//	long long  i = 0, blength = source.length() << 3;
//	uint32_t buf = 0;
//	for (auto&ch : source)
//	{
//		HuffmanEntry & res = StaticHuffmanTable[ch];
//		buf |= res.sequenz << (32 - res.length - (i % 8));
//		for (int j = 0; j < (((i % 8) + res.length) >> 3); ++j)
//		{
//			strs.put((char)(buf >> 24));
//			buf <<= 8;
//		}
//		i += res.length;
//	}
//	if ((i % 8) > 0)
//	{
//		buf |= (uint32_t)(std::numeric_limits<int32_t>::min() >> (7 - (i % 8))) >> (i % 8);
//		strs.put((char)(buf >> 24));
//	}
//	return strs.str();
//}

//Stream::Stream(uintptr_t csocket, uintptr_t cssl) : _array(256), ranges({ _array.data(), _array.data() + _array.length(), _array.length()}), inputiterator(ranges), outputiterator(ranges), running(true), csocket(csocket), cssl(cssl)
//{
//	receiver = (void*)new std::thread([this]() {
//		long long length;
//		fd_set socketfd;
//		timeval timeout{ 0, 0 };
//		FD_ZERO(&socketfd);
//		while (running)
//		{
//			FD_SET(this->csocket, &socketfd);
//			select(this->csocket + 1, &socketfd, nullptr, nullptr, &timeout);
//			length = inputiterator.PointerWriteableLength(outputiterator);
//			if ((FD_ISSET(this->csocket, &socketfd) || SSL_pending(this->cssl)) && length > 0)
//			{
//				if ((length = SSL_read(this->cssl, inputiterator.Pointer(), length)) <= 0) break;
//				inputiterator += length;
//				timeout = { 0, 0 };
//			}
//			else
//			{
//				timeout = { 0, 1 };
//			}
//		}
//		running = false;
//	});
//	const char http2preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
//	RotateIterator end, res;
//	do {
//		end = inputiterator;
//		res = std::search(outputiterator, end, http2preface, http2preface + 24);
//		if (res == end && !running) throw std::runtime_error("Stream Error");
//	} while (res == end);
//	outputiterator = res + 24;
//}
//
//Http2::Stream::~Stream()
//{
//	running = false;
//	(*(std::thread*)receiver).join();
//	SSL_free(cssl);
//	delete (std::thread*)receiver;
//}
//
//Stream & Http2::Stream::operator >> (Frame & frame)
//{
//	while(outputiterator.PointerReadableLength(inputiterator) < 9 && running) std::this_thread::sleep_for(1ms);
//	if (!running) throw std::runtime_error("Can't get frame");
//	frame = { (uint32_t)((outputiterator[0] << 0xff | outputiterator[1] << 0xf | outputiterator[2]) & 0x0fff), (Frame::Type)outputiterator[3], (Frame::Flags)outputiterator[4], (uint32_t)((outputiterator[5] << 0xfff | outputiterator[6] << 0xff | outputiterator[7] << 0xf | outputiterator[8]))};
//	outputiterator += 9;
//	return *this;
//}
//
//Stream & Http2::Stream::operator<<(const Frame & frame)
//{
//	Array<unsigned char> frameb(9);
//	unsigned char * data = frameb.data();
//	memcpy(data, &frame.length + 1, 3);
//	data[3] = frame.Type;
//	data[4] = frame.flags;
//	*(uint32_t*)(data + 5) = (frame.r << 0x7fff) | (frame.streamIndentifier & 0x7fff);
//	SSL_write(cssl, data, 9);
//	return *this;
//}
//
//Stream & Http2::Stream::operator >> (Headers &headers)
//{
//	*this >> (Frame)headers;
//	headers.headerBlockFragment = outputiterator;
//	return *this;
//}
//
//Stream & Http2::Stream::operator >> (HeadersPadded &headers)
//{
//	headers.padLength = (headers.frame.flags & 0x8) != 0 ? (*outputiterator++) : 0;
//	if ((headers.frame.flags & 0x20) != 0)
//	{
//		headers.e = (*outputiterator >> 7) & 1;
//		headers.streamDependency = (uint32_t)(((outputiterator[0] << 24) | (outputiterator[1] << 16) | (outputiterator[2] << 8) | outputiterator[3]) & std::numeric_limits<int32_t>::max());
//		headers.weight = *(outputiterator += 4);
//		++outputiterator;
//	}
//	else
//	{
//		headers.e = 0;
//		headers.streamDependency = 0;
//		headers.weight = 0;
//	}
//	return *this;
//}
//
//Stream & Http2::Stream::operator<<(const Headers & headers)
//{
//	Array<unsigned char> headerb(headers.frame.length);
//	unsigned char * data = headerb.data();
//	long long n = 0;
//	if ((headers.frame.flags & 0x8) != 0)
//	{
//		data[n++] = headers.padLength;
//	}
//	if ((headers.frame.flags & 0x20) != 0)
//	{
//		*(uint32_t*)(data + n) = ((headers.e & 1) << 31) | headers.streamDependency;
//		n += sizeof(uint32_t);
//		data[n++] = headers.weight;
//	}
//	auto hblen = (headers.frame.length - ((headers.frame.flags & 0x8) != 0 ? (headers.padLength + 1) : 0) - ((headers.frame.flags & 0x20) == 0 ? 0 : 5));
//	std::copy(headers.headerBlockFragment, headers.headerBlockFragment + hblen, data + n);
//	n += hblen;
//	if ((headers.frame.flags & 0x8) != 0)
//	{
//		std::copy(headers.padding, headers.headerBlockFragment + headers.padLength, data + n);
//		n += headers.padLength;
//	}
//	SSL_write(cssl, data, n);
//	return *this;
//}
//
//Stream & Http2::Stream::operator >> (Setting & setting)
//{
//	setting.id = (Setting::Id)(outputiterator[0] << 8 | outputiterator[1]);
//	setting.value = (uint32_t)((outputiterator[2] << 24) | (outputiterator[3] << 16) | (outputiterator[4] << 8) | outputiterator[5]);
//	outputiterator += 6;
//	return *this;
//}
//
//Stream & Http2::Stream::operator<<(const Setting & setting)
//{
//	Array<unsigned char> settingb(6);
//	unsigned char * data = settingb.data();
//	*(uint32_t*)data = setting.id;
//	*(uint32_t*)(data + 2) = setting.value;
//	SSL_write(cssl, data, 6);
//	return *this;
//}
//
//void Http2::Stream::Release(long long count)
//{
//	outputiterator += count;
//}
//
//RotateIterator &Http2::Stream::begin()
//{
//	return outputiterator;
//}
//
//RotateIterator Http2::Stream::end()
//{
//	return inputiterator;
//}

Server::Server() : conhandler(std::thread::hardware_concurrency()), running(true), connections(20), connectionsbegin(connections.begin()), connectionsend(connections.begin())
{

}

void Server::Start(const fs::path &certroot)
{
#ifdef _WIN32
	{
		WSADATA wsaData;
		WSAStartup(WINSOCK_VERSION, &wsaData);
	}
#endif
	{		 //=
			//"./"

			//"/etc/letsencrypt/live/p4fdf5699.dip0.t-ipconnect.de/"
		;
		fs::path publicchain = certroot / "fullchain.pem";
		fs::path privatekey = certroot / "privkey.pem";
		if (!fs::is_regular_file(publicchain) || !fs::is_regular_file(privatekey))
		{
			throw std::runtime_error("Zertifikat(e) nicht gefunden\nServer kann nicht gestartet werden");
		}
		OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
		sslctx = SSL_CTX_new(TLSv1_2_server_method());
		if (SSL_CTX_use_certificate_chain_file(sslctx, publicchain.u8string().data()) < 0) {
			throw std::runtime_error(u8"Der Öffentlicher Schlüssel konnte nicht gelesen werden");
		}
		if (SSL_CTX_use_PrivateKey_file(sslctx, privatekey.u8string().data(), SSL_FILETYPE_PEM) < 0) {
			throw std::runtime_error(u8"Der Privater Schlüssel konnte nicht gelesen werden");
		}
	}
	SSL_CTX_set_alpn_select_cb(sslctx, [](SSL * ssl, const unsigned char ** out, unsigned char * outlen, const unsigned char * in, unsigned int inlen, void * args) -> int
	{
		return SSL_select_next_proto((unsigned char **)out, outlen, (const unsigned char *)"\x2h2", 3, in, inlen) == OPENSSL_NPN_NEGOTIATED ? 0 : 1;
	}, nullptr);
	ssocket = socket(AF_INET6, SOCK_STREAM, 0);
	if (ssocket == -1)
	{
		throw std::runtime_error(u8"Der Server Socket konnte nicht erstellt werden");
	}
	{
		DWORD val = 0;
		if (setsockopt(ssocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&val, sizeof(val)) != 0)
		{
			throw std::runtime_error(u8"IPV6_V6ONLY konnte nicht deaktiviert werden");
		}
	}
	{
		sockaddr_in6 serverAdresse;
		memset(&serverAdresse, 0, sizeof(sockaddr_in6));
		serverAdresse.sin6_family = AF_INET6;
		serverAdresse.sin6_addr = in6addr_any;
		serverAdresse.sin6_port = htons(443);
		if (bind(ssocket, (sockaddr *)&serverAdresse, sizeof(serverAdresse)) == -1)
		{
			throw std::runtime_error(u8"Der Server Socket kann nicht gebunden werden");
		}
	}
	if (listen(ssocket, 10) == -1)
	{
		throw std::runtime_error(u8"Der Server Socket kann nicht gelauscht werden");
	}
	FD_ZERO(&sockets);
	FD_SET(ssocket, &sockets);
	for (int i = 0; i < conhandler.length(); ++i)
	{
		conhandler[i] = std::thread(&Server::connectionshandler, this);
	}
}

Server::~Server()
{
	running = false;
	for (int i = 0; i < conhandler.length(); ++i)
	{
		if (conhandler[i].joinable())
		{
			conhandler[i].join();
		}
	}
	for (auto conit = connectionsbegin, end = connectionsend; conit != end; ++conit)
	{
		Connection & con = *conit;
		if (con.csocket != -1 && con.rmtx.try_lock())
		{
			SSL_free(con.cssl);
			Http::CloseSocket(con.csocket);
			con.rmtx.unlock();
		}
	}
	connectionsbegin = connectionsend;
}

void Server::connectionshandler()
{
	timeval timeout = { 0,0 }, poll = { 0,0 };
	while (running)
	{
		{
			fd_set tmp = sockets;
			select(1024, &tmp, nullptr, nullptr, &timeout);
			active = std::move(tmp);
		}
		if (FD_ISSET(ssocket, &active) && connectionsend->rmtx.try_lock())
		{
			Connection & con = *connectionsend;
			socklen_t sockaddr_len = sizeof(sockaddr_in6);
			con.csocket = accept(ssocket, (sockaddr *)&con.adresse, &sockaddr_len);
			if (con.csocket == -1)
			{
				con.rmtx.unlock();
				continue;
			}
			con.cssl = SSL_new(sslctx);
			SSL_set_fd(con.cssl, con.csocket);
			if (SSL_accept(con.cssl) != 1)
			{
				SSL_free(con.cssl);
				Http::CloseSocket(con.csocket);
				con.cssl = 0;
				con.rmtx.unlock();
				continue;
			}
			con.pending = 0;
			con.rbuf = Array<uint8_t>(1024);
			con.routput = con.rinput = con.rbuf.begin();
			con.wbuf = Array<uint8_t>(1024);
			con.woutput = con.winput = con.wbuf.begin();
			con.streams = Array<Stream>(100);
			con.streamsend = con.streams.begin();
			FD_SET(con.csocket, &sockets);
			++connectionsend;
			con.rmtx.unlock();
			timeout = { 0,0 };
		}
		else
		{
			timeout = { 0,1000 };
			for (auto conit = connectionsbegin, end = connectionsend; conit != end; ++conit)
			{
				int64_t length;
				Connection & con = *conit;
				if (con.cssl != nullptr && (length = con.rinput.PointerWriteableLength(con.routput)) > 0 && con.rmtx.try_lock())
				{
					if ((con.pending || (select(1024, &active, nullptr, nullptr, &poll) && FD_ISSET(con.csocket, &active))))
					{
						if ((length = SSL_read(con.cssl, con.rinput.Pointer(), length)) <= 0)
						{
							con.wmtx.lock();
							if (con.csocket != -1)
							{
								SSL_free(con.cssl);
								FD_CLR(con.csocket, &sockets);
								Http::CloseSocket(con.csocket);
								con.cssl = 0;
							}
							for (; connectionsbegin != connectionsend && connectionsbegin->csocket == -1; ++connectionsbegin);
							con.wmtx.unlock();
						}
						else
						{
							FD_CLR(con.csocket, &active);//can evntl crash __i=155 ???
							con.pending = SSL_pending(con.cssl);
							con.rinput += length;
						}
						timeout = { 0,0 };
					}
					con.rmtx.unlock();
				}
				else if (con.cssl != nullptr && (length = con.rinput - con.routput) >= 9 && con.wmtx.try_lock())
				{
					const char http2preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
					if (std::search(con.routput, con.rinput, http2preface, http2preface + 24) == con.routput)
					{
						con.routput += 24;
						if ((length = con.rinput - con.routput) < 9)
						{
							con.wmtx.unlock();
							continue;
						}
					}
					Frame frame;
					*std::reverse_copy(con.routput, con.routput + 3, (unsigned char*)&frame.length) = 0;
					con.routput += 3;
					frame.type = (Frame::Type)*con.routput++;
					frame.flags = (Frame::Flags)*con.routput++;
					std::reverse_copy(con.routput, con.routput + 4, (unsigned char*)&frame.streamIndentifier);
					con.routput += 4;
					if (frame.type > Frame::Type::CONTINUATION)
					{
						con.wmtx.unlock();
						__debugbreak();
						continue;
					}
					while ((con.rinput - con.routput) < frame.length)
					{
						std::this_thread::sleep_for(1ms);
					}
					auto frameend = con.routput + frame.length;
					switch (frame.type)
					{
					case Frame::Type::DATA:
					{
						con.routput += frame.length;
						break;
					}
					case Frame::Type::HEADERS:
					{
						Stream &stream = *con.streamsend++;
						uint32_t hl = frame.length;
						if (((uint8_t)frame.flags & (uint8_t)Frame::Flags::PADDED) == (uint8_t)Frame::Flags::PADDED)
						{
							uint8_t padlength = *con.routput++;
							hl--;
						}
						if (((uint8_t)frame.flags & (uint8_t)Frame::Flags::PRIORITY) == (uint8_t)Frame::Flags::PRIORITY)
						{
							std::reverse_copy(con.routput, con.routput + 4, (unsigned char*)&stream.priority.streamDependency);
							con.routput += 4;
							stream.priority.weight = *con.routput++;
							hl -= 5;
						}
						if (frame.length < hl)
						{
							__debugbreak();
						}
						stream.headerlist = Array<std::pair<std::string, std::string>>(50);
						stream.headerlistend = stream.headerlist.begin();
						con.routput = con.hdecoder.Headerblock(con.routput, con.routput + hl, stream.headerlistend);
						std::string search = ":path";
						auto res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
						if (res != stream.headerlistend)
						{
							fs::path filepath = "C:\\", args;
							{
								size_t pq = res->second.find('?');
								if (pq != std::string::npos)
								{
									args = Utility::urlDecode(String::Replace(res->second.substr(pq + 1), "+", " "));
									filepath /= Utility::urlDecode(res->second.substr(0, pq));
								}
								else
								{
									filepath /= Utility::urlDecode(res->second);
								}
							}
							con.winput += 3;
							*con.winput++ = (unsigned char)Frame::Type::HEADERS;
							*con.winput++ = (unsigned char)Frame::Flags::END_HEADERS;
							con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + sizeof(frame.streamIndentifier), con.winput);
							
							Utility::Array<std::pair<std::string, std::string>> headerlist(20);
							Utility::RotateIterator<std::pair<std::string, std::string>> headerlistend = headerlist.begin();

							search = ":method";
							res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
							if ((res->second == Http::Get || res->second == Http::Head) && fs::is_regular_file(filepath))
							{
								{
									Utility::Array<char> buf(128);
									time_t date = fs::file_time_type::clock::to_time_t(fs::last_write_time(filepath));
									strftime(buf.data(), buf.length(), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&date));
									search = "if-modified-since";
									res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
									if (res != stream.headerlistend && res->second == buf.data())
									{
										*headerlistend++ = { ":status", "304" };
										con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlistend);
										//*con.winput++ = 0x80 | 11;
										{
											uint32_t l = (con.winput - con.woutput) - 9;
											std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
										}
										con.woutput[4] |= (unsigned char)Frame::Flags::END_STREAM;
										do
										{
											con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
										} while (con.woutput != con.winput);
										break;
									}
									*headerlistend++ = { ":status", "200" };
									//*con.winput++ = 0x80 | 8;//Http Ok
									*headerlistend++ = { "last-modified", buf.data() };
									//*con.winput++ = 0x40 | 44;//Last Modified Inc indexing
									//con.winput = HPack::Encoder::StringH(con.winput, buf.data());
									time(&date);
									strftime(buf.data(), buf.length(), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&date));
									*headerlistend++ = { "date", buf.data() };
									//*con.winput++ = 0x40 | 33;//Date Inc indexing
									//con.winput = HPack::Encoder::StringH(con.winput, buf.data());
									*headerlistend++ = { "accept-ranges", "bytes" };
									//*con.winput++ = 0x40 | 18;
									//con.winput = HPack::Encoder::StringH(con.winput, "bytes");
								}
								uintmax_t offset = 0, length = fs::file_size(filepath);
								search = "range";
								res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
								if (res != stream.headerlistend)
								{
									uintmax_t lpos = length - 1;
									std::istringstream iss(res->second.substr(6));
									iss >> offset;
									iss.ignore(1, '-');
									iss >> lpos;
									if (offset <= lpos && lpos < length)
									{
										length = lpos + 1 - offset;
										*headerlist.begin() = { ":status", "206" };
										//*con.winput++ = 0x80 | 10;
										//*con.winput++ = 0x40 | 30;
										//con.winput = HPack::Encoder::StringH(con.winput, res->second);
									}
									else
									{
										con.woutput[5] |= (unsigned char)Frame::Flags::END_STREAM;
										*headerlist.begin() = { ":status", "206" };
										//*con.winput++ = 8;//Never indexed 0000
										//con.winput = HPack::Encoder::StringH(con.winput, std::to_string(416));
										uint32_t l = (con.winput - con.woutput) - 9;
										std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
										do
										{
											con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
										} while (con.woutput != con.winput);
										break;
									}
								}
								*headerlistend++ = { "content-length", std::to_string(length) };
								//*con.winput++ = 0x40 | 28;
								//con.winput = HPack::Encoder::StringH(con.winput, std::to_string(length));
								{
									fs::path ext = filepath.extension();
									*headerlistend++ = { "content-type", std::search(MimeTypeTable, MimeTypeTable + (sizeof(MimeTypeTable) / sizeof(std::pair<fs::path, std::string>) - 1), &ext, &ext + 1, HPack::KeySearch<fs::path, std::string>)->second };
									//*con.winput++ = 0x40 | 31;//Content-Type
									//std::string &res = std::search(MimeTypeTable, MimeTypeTable + (sizeof(MimeTypeTable) / sizeof(std::pair<fs::path, std::string>) - 1), &ext, &ext + 1, HPack::KeySearch<fs::path, std::string>)->second;
									//con.winput = HPack::Encoder::StringH(con.winput, res);
								}
								if (args == "herunterladen")
								{
									*headerlistend++ = { "content-disposition", "attachment" };
									//*con.winput++ = 0x40 | 25;//Content-Disposition
									//con.winput = HPack::Encoder::StringH(con.winput, "attachment");
								}
								{
									std::ifstream stream = std::ifstream(filepath, std::ios::binary);
									if (!stream.is_open()) throw std::runtime_error(u8"Datei kann nicht geöffnet werden");
									con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlistend);
									uint32_t l = (con.winput - con.woutput) - 9;
									std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
									do
									{
										con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
									} while (con.woutput != con.winput);
									for (long long proc = 0; proc < length;)
									{
										long long count = std::min(length - proc, (con.winput + 9).PointerWriteableLength(con.woutput));
										con.winput = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, con.winput);
										*con.winput++ = (unsigned char)Frame::Type::DATA;
										*con.winput++ = count == (length - proc) ? (unsigned char)Frame::Flags::END_STREAM : 0;
										con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + 4, con.winput);
										stream.read((char*)con.winput.Pointer(), count);
										con.winput += count;
										proc += count;
										do
										{
											con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
										} while (con.woutput != con.winput);
									}

									//{
									//	long long read = 0, sent = 0;
									//	std::atomic<bool> interrupt(true);
									//	stream.seekg(offset, std::ios::beg);
									//	std::thread readfileasync([streamid = &frame.streamIndentifier, &con, &stream, &length, &read, &interrupt]() {
									//		try {
									//			long long count;
									//			while (read < length && interrupt.load())
									//			{
									//				if ((count = std::min(length - read, (uint64_t)(con.winput + 9).PointerWriteableLength(con.woutput))) > 0)
									//				{
									//					con.winput = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, con.winput);
									//					*con.winput++ = (unsigned char)Frame::Type::DATA;
									//					*con.winput++ = count == (length - read) ? (unsigned char)Frame::Flags::END_STREAM : 0;
									//					con.winput = std::reverse_copy((unsigned char*)streamid, (unsigned char*)streamid + 4, con.winput);
									//					stream.read((char*)con.winput.Pointer(), count);
									//					con.winput += count;
									//					read += count;
									//				}
									//			}
									//		}
									//		catch (const std::exception &ex)
									//		{

									//		}
									//		interrupt.store(false);
									//	});
									//	long long count;
									//	while (sent < length && interrupt.load() || con.woutput != con.winput)
									//	{
									//		if ((count = con.woutput.PointerReadableLength(con.winput)) > 0)
									//		{
									//			if ((count = SSL_write((SSL*)con.cssl, con.woutput.Pointer(), count)) <= 0)
									//			{
									//				interrupt.store(false);
									//				readfileasync.join();
									//				throw std::runtime_error("RequestBuffer::Send(std::istream &stream, long long offset, long long length) Verbindungs Fehler");
									//			}
									//			con.woutput += count;
									//			sent += count;
									//		}
									//	}
									//	interrupt.store(false);
									//	readfileasync.join();
									//}
								}
							}
							else
							{
								*headerlistend++ = { ":status", "404" };
								con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlistend);
								//*con.winput++ = 0x80 | 11;
								{
									uint32_t l = (con.winput - con.woutput) - 9;
									std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
								}
								con.woutput[4] |= (unsigned char)Frame::Flags::END_STREAM;
								do
								{
									con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
								} while (con.woutput != con.winput);
								break;
							}
						}
						else
						{
							__debugbreak();
						}
						break;
					}
					case Frame::Type::PRIORITY:
					{
						Stream::Priority &priority = frame.streamIndentifier == 0 ? con.priority : std::search(con.streams.begin(), con.streamsend, &frame.streamIndentifier, &frame.streamIndentifier + 1, StreamSearch)->priority;
						std::reverse_copy(con.routput, con.routput + 4, (unsigned char*)&priority.streamDependency);
						con.routput += 4;
						priority.weight = *con.routput++;
						break;
					}
					case Frame::Type::RST_STREAM:
					{
						con.streams[0].state = Stream::State::closed;
						con.routput += frame.length;
						break;
					}
					case Frame::Type::SETTINGS:
					{
						con.routput += frame.length;
						uint32_t l = 0;
						con.winput = std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.winput);
						*con.winput++ = (unsigned char)Frame::Type::SETTINGS;
						*con.winput++ = (unsigned char)Frame::Flags::ACK;
						con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + 4, con.winput);
						do
						{
							con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
						} while (con.woutput != con.winput);
						break;
					}
					case Frame::Type::PUSH_PROMISE:
					{
						con.routput += frame.length;
						break;
					}
					case Frame::Type::PING:
					{
						if (frame.flags == Frame::Flags::ACK)
						{
							con.routput += frame.length;
						}
						else
						{
							con.winput = std::reverse_copy((unsigned char*)&frame.length, (unsigned char*)&frame.length + 3, con.winput);
							*con.winput++ = (unsigned char)Frame::Type::PING;
							*con.winput++ = (unsigned char)Frame::Flags::ACK;
							con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + sizeof(frame.streamIndentifier), con.winput);
							con.winput = std::copy(con.routput, con.routput + frame.length, con.winput);
							con.routput += frame.length;
							do
							{
								con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
							} while (con.woutput != con.winput);
						}
						break;
					}
					case Frame::Type::GOAWAY:
					{
						con.routput += frame.length;
						con.rmtx.lock();
						if (con.csocket != -1)
						{
							SSL_free(con.cssl);
							FD_CLR(con.csocket, &sockets);
							Http::CloseSocket(con.csocket);
							con.cssl = 0;
						}
						for (; connectionsbegin != connectionsend && connectionsbegin->csocket == -1; ++connectionsbegin);
						con.rmtx.unlock();
						break;
					}
					case Frame::Type::WINDOW_UPDATE:
					{
						uint32_t WindowSizeIncrement = (con.routput[0] << 24) | (con.routput[1] << 16) | (con.routput[2] << 8) | con.routput[3];
						con.routput += frame.length;
						break;
					}
					case Frame::Type::CONTINUATION:
					{
						con.routput += frame.length;
						break;
					}
					default:
					{
						con.routput += frame.length;
					}
					}
					if (con.routput != frameend)
					{
						__debugbreak();
					}
					timeout = { 0,0 };
					con.wmtx.unlock();
				}
			}
		}
	}
}