#include "ByteArray.h"
#include <algorithm>
#include <cstring>

ByteArray::ByteArray(ByteArray && obj) : _array(obj._array), _length(obj._length), _owner(obj._owner)
{
	obj._owner = false;
}

ByteArray::ByteArray(ByteArray & obj) : _array(new char[obj._length]), _length(obj._length), _owner(true)
{
	memcpy(_array, obj._array, _length);
}

ByteArray::ByteArray(int length) : _array(new char[length]), _length(length), _owner(true)
{

}

ByteArray::ByteArray(char * array, int offset, int length) : _array(array + offset), _length(length), _owner(false)
{

}

ByteArray::~ByteArray()
{
	if(_owner) delete[] _array;
}

char* ByteArray::Data()
{
	return _array;
}

char & ByteArray::operator[](int i)
{
	return _array[i];
}

int ByteArray::Length()
{
	return _length;
}

int ByteArray::IndexOf(const char * buffer, int length, int offset, int &mlength)
{
	const char *address = nullptr;
	int i = offset;
	while (i < _length)
	{
		address = (const char *)memchr(_array + i, buffer[0], _length - i);
		if (address == nullptr) break;
		mlength = std::min<int>(length, _length + _array - address);
		if (mlength > 0 && memcmp(address, buffer, mlength) == 0)
		{
			return address - _array;
		}
		i = address + 1 - _array;
	}
	mlength = 0;
	return -1;
}

int ByteArray::IndexOf(const char * string, int offset, int &mlength)
{
	return IndexOf(string, strlen(string), offset, mlength);
}

int ByteArray::IndexOf(std::string string, int offset, int &mlength)
{
	return IndexOf(string.data(), string.length(), offset, mlength);
}