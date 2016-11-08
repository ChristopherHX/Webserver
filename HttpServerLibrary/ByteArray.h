#pragma once
#include <string>
#include <memory>

class ByteArray
{
private:
	char * _array;
	const int _length;
	bool _owner;
public:
	ByteArray(ByteArray && obj);
	ByteArray(ByteArray & obj);
	ByteArray(int length);
	ByteArray(char * array, int offset, int length);
	~ByteArray();
	char* Data();
	char &operator[](int i);
	int Length();
	int IndexOf(const char * buffer, int blength, int offset, int &mlength);
	int IndexOf(const char * cstring, int offset, int & mlength);
	int IndexOf(std::string string, int offset, int & mlength);
};