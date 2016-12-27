#pragma once
#include <string>
#include <memory>
#include <algorithm>

namespace Utility {

	template<class Type>
	struct Ranges
	{
		Type * begin;
		Type * end;
		std::size_t length;
	};

	template<class Type>
	class RotateIterator;

	template <class Type>
	class Array
	{
	private:
		Ranges<Type> ranges;
		bool _owner;
	public:
		Array(Array && obj);
		Array(Array & obj);
		Array(std::size_t length);
		Array(Array array, std::size_t offset, std::size_t length);
		Array(Type * array, std::size_t offset, std::size_t length);
		Array();
		~Array();
		Type* data();
		Type &operator[](std::size_t i);
		const std::size_t &length();
		std::size_t indexof(const Type * buffer, std::size_t length, std::size_t offset);
		std::size_t indexof(std::string string, std::size_t offset);
		const Array & operator=(Array &&right);
		RotateIterator<Type> begin();
		RotateIterator<Type> end();
	};

	template<class Type>
	class RotateIterator : public std::iterator<std::random_access_iterator_tag, Type, long long, Type*, Type&>
	{
	private:
		pointer pos;
		long long counter;
		Ranges<Type> * ranges;
	public:
		RotateIterator(Ranges<Type> &ranges, pointer pos);
		RotateIterator(Ranges<Type> &ranges);
		RotateIterator();
		Type& operator*();
		Type* operator->();
		bool operator==(const RotateIterator & right);
		bool operator!=(const RotateIterator & right);
		RotateIterator & operator++();
		RotateIterator & operator--();
		RotateIterator operator++(int);
		RotateIterator operator--(int);
		RotateIterator & operator+=(long long right);
		RotateIterator & operator-=(long long right);
		bool operator<(const RotateIterator & right);
		bool operator>(const RotateIterator & right);
		bool operator<=(const RotateIterator & right);
		bool operator>=(const RotateIterator & right);
		Type& operator[](std::size_t index);
		Type* Pointer();
		uint64_t PointerReadableLength(const RotateIterator & other);
		uint64_t PointerWriteableLength(const RotateIterator & other);
		int64_t operator-(const RotateIterator & right);
		RotateIterator<Type> operator-(long long right);
		RotateIterator<Type> operator+(long long right);
	};


	template<class Type>
	inline Array<Type>::Array(Array && obj)
	{
		obj._owner = false;
		ranges = obj.ranges;
		_owner = obj._owner;
	}

	template<class Type>
	inline Array<Type>::Array(Array & obj)
	{
		ranges = obj.ranges;
		ranges.begin = new Type[ranges.length];
		_owner = true;
		ranges.end = ranges.begin + ranges.length;
		memcpy(ranges.begin, obj.ranges.begin, obj.ranges.length);
	}

	template<class Type>
	inline Array<Type>::Array(std::size_t length)
	{
		ranges.begin = new Type[length];
		_owner = true;
		ranges.end = ranges.begin + length;
		ranges.length = length;
	}

	template<class Type>
	inline Array<Type>::Array(Array<Type> array, std::size_t offset, std::size_t length)
	{
		_owner = false;
		ranges = array.ranges;
		ranges.begin += offset;
		ranges.end = ranges.begin + length;
		ranges.length = length;
	}

	template<class Type>
	inline Array<Type>::Array(Type * array, std::size_t offset, std::size_t length)
	{
		_owner = false;
		ranges.begin = array + offset;
		ranges.end = ranges.begin + length;
		ranges.length = length;
	}

	template<class Type>
	inline Array<Type>::Array() : ranges({ nullptr, nullptr, 0 }), _owner(false)
	{
	}

	template<class Type>
	inline Array<Type>::~Array()
	{
		if (_owner && ranges.begin != nullptr) delete[] ranges.begin;
	}

	template<class Type>
	inline Type * Array<Type>::data()
	{
		return ranges.begin;
	}

	template<class Type>
	inline Type & Array<Type>::operator[](std::size_t i)
	{
		return ranges.begin[i];
	}

	template<class Type>
	inline const std::size_t & Array<Type>::length()
	{
		return ranges.length;
	}

	template<class Type>
	inline std::size_t Array<Type>::indexof(const Type * buffer, std::size_t length, std::size_t offset)
	{
		Type * res = std::search(_array + offset, _array + _length, buffer, buffer + length);
		return res != (_array + _length) ? (res - _array) : -1;
	}

	template<class Type>
	inline std::size_t Array<Type>::indexof(std::string string, std::size_t offset)
	{
		Type * res = std::search(_array + offset, _array + _length, string.begin(), string.end());
		return res != (_array + _length) ? (res - _array) : -1;
	}

	template<class Type>
	inline const Array<Type> & Array<Type>::operator=(Array && right)
	{
		ranges = right.ranges;
		ranges.begin = new Type[ranges.length];
		ranges.end = ranges.begin + ranges.length;
		_owner = true;
		memcpy(ranges.begin, right.ranges.begin, right.ranges.length);
		return *this;
	}

	template<class Type>
	inline RotateIterator<Type> Array<Type>::begin()
	{
		return RotateIterator<Type>(ranges);
	}

	template<class Type>
	inline RotateIterator<Type> Array<Type>::end()
	{
		return RotateIterator<Type>(ranges, ranges.end);
	}

	template<class Type>
	RotateIterator<Type>::RotateIterator(Ranges<Type> & ranges) : ranges(&ranges), pos(ranges.begin), counter(0)
	{
	}

	template<class Type>
	RotateIterator<Type>::RotateIterator() : ranges(nullptr), pos(nullptr), counter(0)
	{
	}

	template<class Type>
	RotateIterator<Type>::RotateIterator(Ranges<Type> & ranges, pointer pos) : ranges(&ranges), pos(pos), counter(0)
	{

	}

	template<class Type>
	Type& RotateIterator<Type>::operator*()
	{
		return *pos;
	}

	template<class Type>
	Type* RotateIterator<Type>::operator->()
	{
		return pos;
	}

	template<class Type>
	bool RotateIterator<Type>::operator==(const RotateIterator &right)
	{
		return pos == right.pos && counter == right.counter;
	}

	template<class Type>
	bool RotateIterator<Type>::operator!=(const RotateIterator & right)
	{
		return pos != right.pos || counter != right.counter;
	}

	template<class Type>
	RotateIterator<Type> &RotateIterator<Type>::operator++()
	{
		if (++pos == ranges->end)
		{
			pos = ranges->begin;
			++counter;
		}
		return *this;
	}

	template<class Type>
	RotateIterator<Type> & RotateIterator<Type>::operator--()
	{
		if (pos == ranges->begin)
		{
			pos = ranges->end - 1;
			--counter;
		}
		else
		{
			--pos;
		}
		return *this;
	}

	template<class Type>
	RotateIterator<Type> RotateIterator<Type>::operator++(int)
	{
		RotateIterator val = *this;
		++(*this);
		return val;
	}

	template<class Type>
	RotateIterator<Type> RotateIterator<Type>::operator--(int)
	{
		RotateIterator val = *this;
		--(*this);
		return val;
	}


	template<class Type>
	RotateIterator<Type> & RotateIterator<Type>::operator+=(long long n)
	{
		pos += n;
		while (pos >= ranges->end)
		{
			pos -= ranges->length;
			++counter;
		}
		while (pos < ranges->begin)
		{
			pos += ranges->length;
			--counter;
		}
		return *this;
	}

	template<class Type>
	RotateIterator<Type> & RotateIterator<Type>::operator-=(long long n)
	{
		pos -= n;
		while (pos >= ranges->end)
		{
			pos -= ranges->length;
			++counter;
		}
		while (pos < ranges->begin)
		{
			pos += ranges->length;
			--counter;
		}
		return *this;
	}

	template<class Type>
	Type& RotateIterator<Type>::operator[](std::size_t index)
	{
		unsigned char * ipos = pos + index;
		while (ipos >= ranges->end)
		{
			ipos -= ranges->length;
		}
		while (ipos < ranges->begin)
		{
			ipos += ranges->length;
		}
		return *ipos;
	}

	template<class Type>
	bool RotateIterator<Type>::operator<(const RotateIterator<Type>& right)
	{
		return counter < right.counter || (pos < right.pos && counter == right.counter);
	}

	template<class Type>
	bool RotateIterator<Type>::operator>(const RotateIterator& right)
	{
		return counter > right.counter || (pos > right.pos && counter == right.counter);
	}

	template<class Type>
	bool RotateIterator<Type>::operator<=(const RotateIterator& right)
	{
		return counter < right.counter || (pos <= right.pos && counter == right.counter);
	}

	template<class Type>
	bool RotateIterator<Type>::operator>=(const RotateIterator& right)
	{
		return counter > right.counter || (pos >= right.pos && counter == right.counter);
	}

	template<class Type>
	Type* RotateIterator<Type>::Pointer()
	{
		return pos;
	}

	template<class Type>
	uint64_t RotateIterator<Type>::PointerReadableLength(const RotateIterator& other)
	{
		return other.pos <= pos && (counter + 1) == other.counter ? ranges->end - pos : (pos < other.pos && counter == other.counter ? other.pos - pos : 0);
	}

	template<class Type>
	uint64_t RotateIterator<Type>::PointerWriteableLength(const RotateIterator& other)
	{
		return other.pos <= pos && counter == other.counter ? ranges->end - pos : (pos < other.pos && counter == (other.counter + 1) ? other.pos - pos : 0);
	}

	template<class Type>
	int64_t RotateIterator<Type>::operator-(const RotateIterator & right)
	{
		return (counter - right.counter) * ranges->length + pos - right.pos;
	}

	template<class Type>
	RotateIterator<Type> RotateIterator<Type>::operator-(long long right)
	{
		RotateIterator<Type> val = *this;
		val -= right;
		return val;
	}

	template<class Type>
	RotateIterator<Type> RotateIterator<Type>::operator+(long long right)
	{
		RotateIterator<Type> val = *this;
		val += right;
		return val;
	}

}