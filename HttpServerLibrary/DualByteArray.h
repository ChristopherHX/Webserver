#include "ByteArray.h"

#include <vector>

class DualByteArray
{
private:
	std::vector<ByteArray> _arrays;
public:
	DualByteArray(DualByteArray &&_array);
	DualByteArray(ByteArray && _array);
	DualByteArray(ByteArray && _array1, ByteArray && _array2);
	ByteArray GetRange(int offset, int length);
	//char &operator[](int i);

	int Length();
	int IndexOf(const char * buffer, int blength, int offset, int &mlength);
	int IndexOf(const char * cstring, int offset, int & mlength);
	int IndexOf(std::string string, int offset, int & mlength);
};