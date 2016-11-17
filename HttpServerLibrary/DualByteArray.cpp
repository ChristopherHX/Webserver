#include "DualByteArray.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

DualByteArray::DualByteArray(DualByteArray && _array) : _arrays(std::move(_array._arrays)), offset(0)
{

}

DualByteArray::DualByteArray(ByteArray && _array) : offset(0)
{
	_arrays.push_back(std::move(_array));
}

DualByteArray::DualByteArray(ByteArray && _array1, ByteArray && _array2) : offset(0)
{
	_arrays.push_back(std::move(_array1));
	_arrays.push_back(std::move(_array2));
}

ByteArray DualByteArray::GetRange(int offset, int length)
{
	const int off = offset + this->offset, l1 = _arrays[0].Length();
	if (l1 > off)
	{
		if ((off + length) > l1)
		{
			ByteArray newarray(length);
			int lp1 = l1 - off;
			memcpy(newarray.Data(), _arrays[0].Data() + off, lp1);
			memcpy(newarray.Data() + (l1 - off), _arrays[1].Data(), length - (l1 - off));
			return std::move(newarray);
		}
		else
		{
			return ByteArray(_arrays[0].Data(), off, length);
		}
	}
	else
	{
		return ByteArray(_arrays[1].Data(), off - l1, length);
	}
}

int DualByteArray::Length()
{
	return _arrays[0].Length() + (_arrays.size() < 2 ? 0 : _arrays[1].Length()) - offset;
}

const int &DualByteArray::SeqLength()
{
	return _arrays[0].Length() - offset;
}

void DualByteArray::Free(int count)
{
	offset += count;
}

int DualByteArray::IndexOf(const char * buffer, int length, int offset, int &mlength)
{
	const int off = offset + this->offset, &l1 = SeqLength();
	bool farray = off < l1 || _arrays.size() < 2;
	int i = _arrays[farray ? 0 : 1].IndexOf(buffer, length, off - (farray ? 0 : l1), mlength);
	if (_arrays.size() > 1 && farray)
	{
		if (mlength > 0)
		{
			if (memcmp(_arrays[1].Data(), buffer + mlength, length - mlength) == 0)
			{
				mlength = length;
				return i;
			}
			else
			{
				mlength = 0;
				return -1;
			}
		}
		else
		{
			i = _arrays[1].IndexOf(buffer, length, 0, mlength);
			return (i < 0 ? 0 : l1) + i;
		}
	}
	else
	{
		return i;
	}
}

int DualByteArray::IndexOf(const char * string, int offset, int &mlength)
{
	return IndexOf(string, strlen(string), offset, mlength);
}

int DualByteArray::IndexOf(std::string string, int offset, int &mlength)
{
	return IndexOf(string.data(), string.length(), offset, mlength);
}