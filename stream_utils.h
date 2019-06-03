#ifndef LAZPERF_C_STREAM_UTILS_H
#define LAZPERF_C_STREAM_UTILS_H

#include <vector>
#include <iostream>

template<typename CTYPE = unsigned char>
class TypedLazPerfBuf
{
	typedef std::vector<CTYPE> LazPerfRawBuf;

public:
	LazPerfRawBuf &m_buf;
	size_t m_idx;
	size_t total;

	explicit TypedLazPerfBuf(LazPerfRawBuf &buf) : m_buf(buf), m_idx(0), total(0)
	{}

	void putBytes(const unsigned char *b, size_t len)
	{
		m_buf.insert(m_buf.end(), (const CTYPE *) b, (const CTYPE *) (b + len));
		total += len;
	}

	void putByte(const unsigned char b)
	{
		m_buf.push_back((CTYPE) b);
		total++;
	}

	size_t totalWritten() const {
		return total;
	}
};


class ReadOnlyStream
{
public:
	const uint8_t *m_data;
	size_t m_dataLength;
	size_t m_idx;

	ReadOnlyStream(const uint8_t *data, size_t dataLen)
			: m_data(data), m_dataLength(dataLen), m_idx(0)
	{}


	unsigned char getByte()
	{
		if (m_idx >= m_dataLength)
		{
			throw std::runtime_error("Tried to read past buffer bounds");
		}
		return (unsigned char) m_data[m_idx++];
	}

	void getBytes(unsigned char *b, int len)
	{
		if (m_idx + len > m_dataLength)
		{
			throw std::runtime_error("Tried to read past buffer bounds");
		}
		memcpy(b, (unsigned char *) &m_data[m_idx], len);
		m_idx += len;
	}
};


#endif //LAZPERF_C_STREAM_UTILS_H
