#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace Hydro
{
	// A section tag from four letters ("MAP "), as it reads in a hex dump.
	constexpr uint32_t SaveTag(const char (&letters)[5])
	{
		return (uint32_t)(uint8_t)letters[0] | ((uint32_t)(uint8_t)letters[1] << 8) | ((uint32_t)(uint8_t)letters[2] << 16)
			| ((uint32_t)(uint8_t)letters[3] << 24);
	}

	// Writes a save file's bytes: plain values as the machine holds them (little-endian on every platform the game ships
	// on), strings and vectors with their length first, and sections (a tag, a length, a version) a reader can skip.
	class SaveWriter
	{
	public:
		template<typename T>
		void Value(T value)
		{
			static_assert(std::is_trivially_copyable_v<T>, "SaveWriter::Value: plain values only");
			const size_t at = m_Bytes.size();
			m_Bytes.resize(at + sizeof(T));
			std::memcpy(m_Bytes.data() + at, &value, sizeof(T));
		}

		void Text(const std::string& text)
		{
			Value((uint32_t)text.size());
			m_Bytes.insert(m_Bytes.end(), text.begin(), text.end());
		}

		template<typename T>
		void Values(const std::vector<T>& values)
		{
			static_assert(std::is_trivially_copyable_v<T>, "SaveWriter::Values: plain values only");
			Value((uint32_t)values.size());
			if (values.empty() == false)
			{
				const size_t at = m_Bytes.size();
				m_Bytes.resize(at + values.size() * sizeof(T));
				std::memcpy(m_Bytes.data() + at, values.data(), values.size() * sizeof(T));
			}
		}

		// A section: its tag, then its length (filled in by End), then its own version. Sections don't nest.
		void Begin(uint32_t tag, uint32_t version)
		{
			Value(tag);
			m_Open = m_Bytes.size();
			Value((uint64_t)0);
			Value(version);
		}

		void End()
		{
			const uint64_t length = (uint64_t)(m_Bytes.size() - m_Open - sizeof(uint64_t));
			std::memcpy(m_Bytes.data() + m_Open, &length, sizeof(uint64_t));
		}

		const std::vector<uint8_t>& GetBytes() const
		{
			return m_Bytes;
		}

	private:
		std::vector<uint8_t> m_Bytes;
		size_t m_Open = 0;
	};

	// Reads what SaveWriter wrote. Reading past the end or a length that can't be right marks it bad (IsGood) and gives
	// defaults from then on, so a damaged file never reads out of bounds.
	class SaveReader
	{
	public:
		SaveReader() = default;

		SaveReader(const uint8_t* data, size_t size)
			: m_Data(data), m_Size(size)
		{
		}

		bool IsGood() const
		{
			return m_Good;
		}

		bool AtEnd() const
		{
			return m_At >= m_Size;
		}

		template<typename T>
		T Value(T fallback = T{})
		{
			static_assert(std::is_trivially_copyable_v<T>, "SaveReader::Value: plain values only");
			if (m_Good == false || m_Size - m_At < sizeof(T))
			{
				m_Good = false;
				return fallback;
			}
			T value;
			std::memcpy(&value, m_Data + m_At, sizeof(T));
			m_At += sizeof(T);
			return value;
		}

		std::string Text()
		{
			const uint32_t length = Value<uint32_t>();
			if (m_Good == false || m_Size - m_At < length)
			{
				m_Good = false;
				return std::string();
			}
			std::string text((const char*)(m_Data + m_At), length);
			m_At += length;
			return text;
		}

		template<typename T>
		std::vector<T> Values()
		{
			static_assert(std::is_trivially_copyable_v<T>, "SaveReader::Values: plain values only");
			const uint32_t count = Value<uint32_t>();
			std::vector<T> values;
			if (m_Good == false || (m_Size - m_At) / sizeof(T) < count)
			{
				m_Good = false;
				return values;
			}
			values.resize(count);
			if (count > 0)
			{
				std::memcpy(values.data(), m_Data + m_At, (size_t)count * sizeof(T));
			}
			m_At += (size_t)count * sizeof(T);
			return values;
		}

		// The next section: its tag and version, and a reader over its content (which this reader skips). False at the end
		// or when the section doesn't fit.
		bool NextSection(uint32_t& tag, uint32_t& version, SaveReader& content)
		{
			if (AtEnd() || m_Good == false)
			{
				return false;
			}
			tag = Value<uint32_t>();
			const uint64_t length = Value<uint64_t>();
			if (m_Good == false || length < sizeof(uint32_t) || (uint64_t)(m_Size - m_At) < length)
			{
				m_Good = false;
				return false;
			}
			content = SaveReader(m_Data + m_At, (size_t)length);
			version = content.Value<uint32_t>();
			m_At += (size_t)length;
			return true;
		}

	private:
		const uint8_t* m_Data = nullptr;
		size_t m_Size = 0;
		size_t m_At = 0;
		bool m_Good = true;
	};
}
