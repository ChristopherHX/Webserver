#include "DualByteArray.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

DualByteArray::DualByteArray(DualByteArray && _array) : _arrays(std::move(_array._arrays))
{

}

DualByteArray::DualByteArray(ByteArray && _array)
{
	_arrays.push_back(std::move(_array));
}

DualByteArray::DualByteArray(ByteArray && _array1, ByteArray && _array2)
{
	_arrays.push_back(std::move(_array1));
	_arrays.push_back(std::move(_array2));
}

ByteArray DualByteArray::GetRange(int offset, int length)
{
	int l1 = _arrays[0].Length();
	if (l1 > offset)
	{
		if ((offset + length) > l1)
		{
			ByteArray newarray(length);
			int lp1 = l1 - offset;
			memcpy(newarray.Data(), _arrays[0].Data() + offset, lp1);
			memcpy(newarray.Data() + (l1 - offset), _arrays[1].Data(), length - (l1 - offset));
			return std::move(newarray);
		}
		else
		{
			return ByteArray(_arrays[0].Data(), offset, length);
		}
	}
	else
	{
		return ByteArray(_arrays[1].Data(), offset - l1, length);
	}
}

int DualByteArray::Length()
{
	//return _arrays[0].Length() + ((_arrays.size() > 1) ? _arrays[1].Length() : 0);
	return _arrays[0].Length();
}


int DualByteArray::IndexOf(const char * buffer, int length, int offset, int &mlength)
{
	int l1 = _arrays[0].Length();
	int i1 = _arrays[0].IndexOf(buffer, length, offset, mlength);
	if (length == mlength || _arrays.size() < 2)
	{
		return i1;
	}
	if (_arrays.size() > 1)
	{
		if (mlength < length && mlength > 0 && memcmp(_arrays[1].Data(), buffer + mlength, length - mlength) == 0)
		{
			mlength = length;
			return i1;
		}
		mlength = 0;
		//return _arrays[1].IndexOf(buffer, length, offset - l1, mlength);
	}
	return -1;
}

int DualByteArray::IndexOf(const char * string, int offset, int &mlength)
{
	return IndexOf(string, strlen(string), offset, mlength);
}

int DualByteArray::IndexOf(std::string string, int offset, int &mlength)
{
	return IndexOf(string.data(), string.length(), offset, mlength);
}

//char & DualByteArray::operator[](int i)
//{
//	return i < _arrays[0].Length() ? _arrays[0][i] : _arrays[1][i - _arrays[0].Length()];
//}
