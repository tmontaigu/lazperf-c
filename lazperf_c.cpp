#include "lazperf_c.h"

#include <iostream>

#include <iostream>
#include <istream>

#include <laz-perf/common/common.hpp>
#include <laz-perf/compressor.hpp>
#include <laz-perf/decompressor.hpp>

#include <laz-perf/encoder.hpp>
#include <laz-perf/decoder.hpp>
#include <laz-perf/formats.hpp>
#include <laz-perf/io.hpp>
#include <laz-perf/las.hpp>

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


class VlrDecompressor
{
public:
	VlrDecompressor(
			const uint8_t *compressedData,
			size_t dataLength,
			size_t pointSize,
			const char *vlr_data)
			: m_stream(compressedData, dataLength), m_chunksize(0), m_chunkPointsRead(0)
	{
		laszip::io::laz_vlr zipvlr(vlr_data);
		m_chunksize = zipvlr.chunk_size;
		m_schema = laszip::io::laz_vlr::to_schema(zipvlr, pointSize);
	}

	size_t getPointSize() const
	{ return (size_t) m_schema.size_in_bytes(); }

	void decompress(char *out)
	{
		if (m_chunkPointsRead == m_chunksize || !m_decoder || !m_decompressor)
		{
			resetDecompressor();
			m_chunkPointsRead = 0;
		}
		m_decompressor->decompress(out);
		m_chunkPointsRead++;
	}

private:
	void resetDecompressor()
	{
		m_decoder.reset(new Decoder(m_stream));
		m_decompressor =
				laszip::factory::build_decompressor(*m_decoder, m_schema);
	}


	typedef laszip::formats::dynamic_decompressor Decompressor;
	typedef laszip::factory::record_schema Schema;
	typedef laszip::decoders::arithmetic<ReadOnlyStream> Decoder;

	ReadOnlyStream m_stream;

	std::unique_ptr<Decoder> m_decoder;
	Decompressor::ptr m_decompressor;
	Schema m_schema;
	uint32_t m_chunksize;
	uint32_t m_chunkPointsRead;
};



static RawPointsBuffer _lazperf_decompress_points(const uint8_t *compressed_points_buffer,
										size_t buffer_size,
										const char *lazsip_vlr_data,
										size_t num_points,
										size_t point_size)
{
	VlrDecompressor decompressor(compressed_points_buffer, buffer_size, point_size, lazsip_vlr_data);
	std::unique_ptr<char []> decompressed_points(new char[point_size * num_points]);

	for (char *current_point = decompressed_points.get();
		 current_point < decompressed_points.get() + (point_size * num_points);
		 current_point += point_size
			)
	{
		decompressor.decompress(current_point);
	}
	RawPointsBuffer buffer {decompressed_points.release(), point_size * num_points};
	return buffer;
}

LazPerfResult lazperf_decompress_points(
		const uint8_t *compressed_points_buffer,
		size_t buffer_size,
		const char *lazsip_vlr_data,
		size_t num_points,
		size_t point_size)
{
	LazPerfResult result{};
	try
	{
		RawPointsBuffer points = _lazperf_decompress_points(compressed_points_buffer, buffer_size, lazsip_vlr_data, num_points, point_size);
		result.is_error = 0;
		result.points_buffer = points;
	} catch (std::exception &e) {
		result.is_error = 1;
		result.error.error_msg = e.what();
	}
	catch (...) {
		result.is_error = 1;
		result.error.error_msg = "unknown error";
	}
	return result;
}
