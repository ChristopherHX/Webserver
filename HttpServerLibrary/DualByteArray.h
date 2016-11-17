#include "ByteArray.h"

#include <vector>

class DualByteArray
{
private:
	std::vector<ByteArray> _arrays;
	int offset;
public:
	DualByteArray(DualByteArray &&_array);
	DualByteArray(ByteArray && _array);
	DualByteArray(ByteArray && _array1, ByteArray && _array2);
	ByteArray GetRange(int offset, int length);

	int Length();
	const int &SeqLength();
	void Free(int count);
	int IndexOf(const char * buffer, int blength, int offset, int &mlength);
	int IndexOf(const char * string, int offset, int & mlength);
	int IndexOf(std::string string, int offset, int & mlength);
};