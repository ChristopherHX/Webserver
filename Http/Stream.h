#pragma once
#include "Request.h"
#include <cstdint>
#include <vector>

class Stream
{
public:
	enum class State : uint8_t
	{
		idle = 0b0,
		reserved_local = 0b00000100,
		reserved_remote = 0b00001000,
		open = 0b00000011,
		half_closed_local = 0b00000001,
		half_closed_remote = 0b00000010,
		closed = 0b11110000
	};
	class Priority
	{
	public:
		bool exclusive;
		uint32_t dependency;
		uint8_t weight;
	};
	State state;
	Priority priority;
	std::shared_ptr<Request> request;
	//std::vector<std::pair<std::string, std::string>> headerlist;
};