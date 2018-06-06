#include "Header.h"
#include "V1/Http.h"

std::hash<std::string> Net::Http::Header::hash_fn;
size_t hscheme = Net::Http::Header::hash_fn(":scheme");
size_t hauthority = Net::Http::Header::hash_fn(":authority");
size_t hhost = Net::Http::Header::hash_fn("host");
size_t hcontenttype = Net::Http::Header::hash_fn("content-type");
size_t hcontentlength = Net::Http::Header::hash_fn("content-length");

void Net::Http::Header::Add(const std::pair<std::string, std::string>& pair)
{
	if (!Add(hash_fn(pair.first), pair))
	{
		headerlist.insert(pair);
	}
}

bool Net::Http::Header::Add(size_t hash, const std::pair<std::string, std::string>& pair)
{
	if (hash == hscheme)
	{
		scheme = pair.second;
	}
	else if (hash == hauthority || hash == hhost)
	{
		authority = pair.second;
	}
	else if(hash == hcontenttype)
	{
		contenttype = pair.second;
	}
	else if (hash == hcontentlength)
	{
		contentlength = std::stoull(pair.second);
	}
	else
	{
		return false;
	}
	return true;
}

void Net::Http::Header::EncodeHttp1(std::vector<uint8_t>::iterator & buffer) const
{
	for (auto &entry : headerlist)
	{
		std::string line = Net::Http::V1::KeyUpper(entry.first) + ": " + entry.second + "\r\n";
		buffer = std::copy(line.begin(), line.end(), buffer);
	}
	const char nl[] = "\r\n";
	buffer = std::copy(nl, std::end(nl), buffer);
}

void Net::Http::Header::EncodeHttp2(std::shared_ptr<Net::Http::V2::HPack::Encoder> encoder, std::vector<uint8_t>::iterator & buffer) const
{
	std::vector<std::pair<std::string, std::string>> el;
	if(!scheme.empty())
		el.push_back({ ":scheme", scheme });
	if (!authority.empty())
		el.push_back({ ":authority", authority });
	if (!contenttype.empty())
		el.push_back({ "content-type", contenttype });
	if(contentlength != 0)
		el.push_back({ "content-length", std::to_string(contentlength) });
	encoder->AddHeaderBlock(buffer, el);
	encoder->AddHeaderBlock(buffer, headerlist);
}

void Net::Http::Header::DecodeHttp1(std::vector<uint8_t>::const_iterator & buffer,const std::vector<uint8_t>::const_iterator & end)
{
	const char sep[] = ": ";
	const char rn[] = "\r\n";
	while (buffer < end)
	{
		auto ofr = std::search(buffer, end, rn, std::end(rn));
		auto osep = std::search(buffer, ofr, sep, std::end(sep));
		if (osep != ofr)
		{
			std::string key(buffer, osep);
			std::transform(key.begin(), key.end(), key.begin(), ::tolower);
			Add({ key, std::string(osep + 2, ofr) });
		}
		buffer = ofr + 2;
	}
}

void Net::Http::Header::DecodeHttp2(std::shared_ptr<Net::Http::V2::HPack::Decoder> decoder, std::vector<uint8_t>::const_iterator & buffer, const std::vector<uint8_t>::const_iterator & end)
{
	decoder->DecodeHeaderblock(this, buffer, end);
}
