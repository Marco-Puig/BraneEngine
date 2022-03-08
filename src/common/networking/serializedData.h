#pragma once
#include <cstdint>
#include <vector>
#include <sstream>
#include <byte.h>
#include <assets/assetID.h>
#include <cstring>
#include <regex>
#include <typeinfo>


class SerializationError : virtual public std::runtime_error
{
public:
	explicit SerializationError(const std::type_info& t) : std::runtime_error(std::string("type: ") + t.name() + " could not be serialized")
	{

	}
};

class ISerializedData
{
	size_t _ittr = 0;
public:
	std::vector<byte> data;

	[[nodiscard]] size_t size() const
	{
		return data.size();
	}

	friend std::ostream& operator << (std::ostream& os, const ISerializedData& msg)
	{
		os << " Serialized Data: ";
		for (int i = 0; i < msg.data.size(); ++i)
		{
			if(i % 80 == 0)
				std::cout << "\n";
			std::cout << msg.data[i];
		}
		std::cout << std::endl;
		return os;
	}

	template <typename T>
	friend ISerializedData& operator >> (ISerializedData& msg, T& data)
	{
		if constexpr(!std::is_trivially_copyable<T>::value)
			throw SerializationError(typeid(T));
		if (msg._ittr + sizeof(T) > msg.data.size())
			throw std::runtime_error("Tried to read past end of serialized data");

		std::memcpy(&data, msg.data.data() + msg._ittr, sizeof(T));

		msg._ittr += sizeof(T);

		return msg;
	}

	template<typename T>
	void readSafeArraySize(T& index) // Call this instead of directly reading sizes to prevent buffer overruns
	{
		*this >> index;
		if(index + _ittr > data.size())
			throw std::runtime_error("invalid array length in serialized data");
	}

	template <typename T>
	friend ISerializedData& operator >> (ISerializedData& msg, std::vector<T>& data)
	{
		if constexpr(!std::is_trivially_copyable<T>::value)
			throw SerializationError(typeid(T));
		uint32_t size;
		msg.readSafeArraySize(size);

		data.resize(size / sizeof(T));
		if(size > 0)
			std::memcpy(data.data(), &msg.data[msg._ittr], size);

		msg._ittr += size;

		return msg;
	}

	friend ISerializedData& operator >> (ISerializedData& msg, std::string& data)
	{
		uint32_t size;
		msg.readSafeArraySize(size);

		data.resize(size);
		if(size > 0)
			std::memcpy(data.data(), &msg.data[msg._ittr], size);

		msg._ittr += size;

		return msg;
	}

	friend ISerializedData& operator >> (ISerializedData& msg, AssetID& id)
	{
		std::string idString;
		msg >> idString;
		id.parseString(idString);

		return msg;
	}

	friend ISerializedData& operator >> (ISerializedData& msg, std::vector<AssetID>& ids)
	{
		uint32_t size;
		msg.readSafeArraySize(size);
		ids.resize(size);
		for (uint32_t i = 0; i < size; ++i)
		{
			std::string idString;
			msg >> idString;
			ids[i].parseString(idString);
		}


		return msg;
	}



	void read(void* dest, size_t size)
	{
		if (_ittr + size <= data.size())
			throw std::runtime_error("Tried to read past end of serialized data");

		std::memcpy(dest, &data[_ittr], size);
		_ittr += size;

	}

	template<typename T>
	T peek()
	{
		if constexpr(!std::is_trivially_copyable<T>::value)
			throw SerializationError(typeid(T));
		if (_ittr + sizeof(T) > data.size())
			throw std::runtime_error("Tried to read past end of serialized data");
		size_t ittrPos = _ittr;
		T o;
		*this >> o;
		_ittr = ittrPos;
		return o;
	}

	[[nodiscard]] bool endOfData() const
	{
		return _ittr >= data.size();
	}

	void restart()
	{
		_ittr = 0;
	}
};

class OSerializedData
{
public:
	std::vector<byte> data;

	[[nodiscard]] size_t size() const
	{
		return data.size();
	}

	friend std::ostream& operator << (std::ostream& os, const OSerializedData& msg)
	{
		os << " Serialized Data: ";
		for (int i = 0; i < msg.data.size(); ++i)
		{
			if(i % 80 == 0)
				std::cout << "\n";
			std::cout << msg.data[i];
		}
		std::cout << std::endl;
		return os;
	}

	template <typename T>
	friend OSerializedData& operator << (OSerializedData& msg, const T& data)
	{
		if constexpr(!std::is_trivially_copyable<T>::value)
			throw SerializationError(typeid(T));

		size_t index = msg.data.size();
		msg.data.resize(index + sizeof(T));
		std::memcpy(&msg.data[index], &data, sizeof(T));

		return msg;
	}

	template <typename T>
	friend OSerializedData& operator << (OSerializedData& msg, const std::vector<T>& data)
	{
		if constexpr(!std::is_trivially_copyable<T>::value)
			throw SerializationError(typeid(T));

		uint32_t arrLength = data.size() * sizeof(T);
		msg << arrLength;
		size_t index = msg.data.size();
		msg.data.resize(index + arrLength);
		if(arrLength > 0)
			std::memcpy(&msg.data[index], data.data(), arrLength);


		return msg;
	}

	friend OSerializedData& operator << (OSerializedData& msg, const std::string& data)
	{
		uint32_t arrLength = data.size();
		msg << arrLength;
		size_t index = msg.data.size();
		msg.data.resize(index + arrLength);
		if(arrLength > 0)
			std::memcpy(&msg.data[index], data.data(), data.size());

		return msg;
	}

	friend OSerializedData& operator << (OSerializedData& msg, const AssetID& id)
	{
		msg << id.string();

		return msg;
	}

	friend OSerializedData& operator << (OSerializedData& msg, const std::vector<AssetID>& ids)
	{
		msg << (uint32_t)ids.size();
		for (uint32_t i = 0; i < ids.size(); ++i)
		{
			msg << ids[i].string();
		}


		return msg;
	}

	void write(const void* src, size_t size)
	{
		size_t index = data.size();
		data.resize(index + size);
		std::memcpy(&data[index], src, size);
	}

	[[nodiscard]] ISerializedData toIMessage() const
	{
		ISerializedData o{};
		o.data.resize(data.size());
		std::memcpy(o.data.data(), data.data(), data.size());
		return o;
	}
};