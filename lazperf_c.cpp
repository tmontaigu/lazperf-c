#include "lazperf_c.h"
#include "stream_utils.h"

#include <iostream>
#include <utility>
#include <istream>

#include <laz-perf/common/common.hpp>
#include <laz-perf/compressor.hpp>
#include <laz-perf/decompressor.hpp>

#include <laz-perf/encoder.hpp>
#include <laz-perf/decoder.hpp>
#include <laz-perf/formats.hpp>
#include <laz-perf/io.hpp>
#include <laz-perf/las.hpp>

static_assert(sizeof(char) == sizeof(uint8_t), "The 'char' type needs to have the same size as the 'uint8_t'");

/***********************************************************************************************************************
 * Compression
 **********************************************************************************************************************/

typedef laszip::factory::record_schema Schema;


class VlrCompressor
{
public:
	VlrCompressor(Schema s, uint64_t offset_to_data)
			: m_stream(m_data_vec), m_encoder(nullptr), m_chunkPointsWritten(0), m_offsetToData(offset_to_data),
			  m_chunkInfoPos(0), m_chunkOffset(0), m_schema(std::move(std::move(s))),
			  m_vlr(laszip::io::laz_vlr::from_schema(m_schema)), m_chunksize(m_vlr.chunk_size)
	{
	}

	const std::vector<uint8_t> *data() const;

	size_t compress(const char *inbuf);

	void done();

	size_t vlrDataSize() const
	{ return m_vlr.size(); }

	void extractVlrData(char *out_data)
	{ return m_vlr.extract(out_data); }

	size_t getPointSize() const
	{ return (size_t) m_schema.size_in_bytes(); }


	size_t copy_data_to(uint8_t *dst) const
	{
		std::copy(m_data_vec.begin(), m_data_vec.end(), dst);
		return std::distance(m_data_vec.begin(), m_data_vec.end());
	}

	void resetStreamPosition() {
		m_data_vec.resize(0);
	}

private:
	typedef laszip::encoders::arithmetic<TypedLazPerfBuf<uint8_t>> Encoder;

	typedef laszip::formats::dynamic_compressor Compressor;

	void resetCompressor();

	void newChunk();

	std::vector<uint8_t> m_data_vec;
	TypedLazPerfBuf<uint8_t> m_stream;
	std::unique_ptr<Encoder> m_encoder;
	Compressor::ptr m_compressor;
	uint32_t m_chunkPointsWritten;
	uint64_t m_offsetToData;
	uint64_t m_chunkInfoPos;
	uint64_t m_chunkOffset;
	Schema m_schema;
	laszip::io::laz_vlr m_vlr;
	uint32_t m_chunksize;

	std::vector<uint32_t> m_chunkTable;
};


size_t VlrCompressor::compress(const char *inbuf)
{
	// First time through.
	if (!m_encoder || !m_compressor)
	{
		// Get the position, which is 0 since we
		// are just starting to write
		m_chunkInfoPos = m_stream.m_buf.size();

		// Seek over the chunk info offset value
		unsigned char skip[sizeof(uint64_t)] = {0};
		m_stream.putBytes(skip, sizeof(skip));
		m_chunkOffset = m_chunkInfoPos + sizeof(uint64_t);

		resetCompressor();
	}
	else if (m_chunkPointsWritten == m_chunksize)
	{
		resetCompressor();
		newChunk();
	}
	m_compressor->compress(inbuf);
	m_chunkPointsWritten++;
	return m_data_vec.size();
}


void VlrCompressor::done()
{
	// Close and clear the point encoder.
	m_encoder->done();
	m_encoder.reset();

	newChunk();

	// Save our current position.  Go to the location where we need
	// to write the chunk table offset at the beginning of the point data.

	uint64_t chunkTablePos = htole64((uint64_t) m_stream.m_buf.size());
	// We need to add the offset given a construction time since
	// we did not use a stream to write the header and vlrs
	uint64_t trueChunkTablePos = htole64((uint64_t) m_stream.m_buf.size() + m_offsetToData);

	// Equivalent of stream.seekp(m_chunkInfoPos); stream << chunkTablePos
	memcpy(&m_stream.m_buf[m_chunkInfoPos], (char *) &trueChunkTablePos, sizeof(uint64_t));

	// Move to the start of the chunk table.
	// Which in our case is the end of the m_stream vector

	// Write the chunk table header in two steps
	// 1. Push bytes into the stream
	// 2. memcpy the data into the pushed bytes
	unsigned char skip[2 * sizeof(uint32_t)] = {0};
	m_stream.putBytes(skip, sizeof(skip));
	uint32_t version = htole32(0);
	uint32_t chunkTableSize = htole32((uint32_t) m_chunkTable.size());

	memcpy(&m_stream.m_buf[chunkTablePos], &version, sizeof(uint32_t));
	memcpy(&m_stream.m_buf[chunkTablePos + sizeof(uint32_t)], &chunkTableSize, sizeof(uint32_t));

	// Encode and write the chunk table.
	// OutputStream outputStream(m_stream);
	TypedLazPerfBuf<uint8_t> outputStream(m_stream);
	Encoder encoder(outputStream);
	laszip::compressors::integer compressor(32, 2);
	compressor.init();

	uint32_t predictor = 0;
	for (uint32_t chunkSize : m_chunkTable)
	{
		chunkSize = htole32(chunkSize);
		compressor.compress(encoder, predictor, chunkSize, 1);
		predictor = chunkSize;
	}
	encoder.done();
}

void VlrCompressor::resetCompressor()
{
	if (m_encoder)
		m_encoder->done();
	m_encoder.reset(new Encoder(m_stream));
	m_compressor = laszip::factory::build_compressor(*m_encoder, m_schema);
}

void VlrCompressor::newChunk()
{
	size_t offset = m_stream.m_buf.size();
	m_chunkTable.push_back((uint32_t) (offset - m_chunkOffset));
	m_chunkOffset = offset;
	m_chunkPointsWritten = 0;
}

const std::vector<uint8_t> *VlrCompressor::data() const
{
	return &m_stream.m_buf;
}


/***********************************************************************************************************************
 * Decompression
 **********************************************************************************************************************/

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

/***********************************************************************************************************************
 * Purely C API
 **********************************************************************************************************************/

static LazPerf_SizedBuffer _lazperf_decompress_points(const uint8_t *compressed_points_buffer,
											  size_t buffer_size,
											  const char *lazsip_vlr_data,
											  size_t num_points,
											  size_t point_size)
{
	VlrDecompressor decompressor(compressed_points_buffer, buffer_size, point_size, lazsip_vlr_data);
	std::unique_ptr<char[]> decompressed_points(new char[point_size * num_points]);

	for (char *current_point = decompressed_points.get();
		 current_point < decompressed_points.get() + (point_size * num_points);
		 current_point += point_size
			)
	{
		decompressor.decompress(current_point);
	}
	LazPerf_SizedBuffer buffer{decompressed_points.release(), point_size * num_points};
	return buffer;
}

LazPerf_Result lazperf_decompress_points(
		const uint8_t *compressed_points_buffer,
		size_t buffer_size,
		const char *lazsip_vlr_data,
		size_t num_points,
		size_t point_size)
{
	LazPerf_Result result{};
	try
	{
		LazPerf_SizedBuffer points = _lazperf_decompress_points(compressed_points_buffer, buffer_size, lazsip_vlr_data,
														num_points, point_size);
		result.is_error = 0;
		result.points_buffer = points;
	} catch (std::exception &e)
	{
		result.is_error = 1;
		result.error.error_msg = strdup(e.what());
	}
	catch (...)
	{
		result.is_error = 1;
		result.error.error_msg = strdup("unknown error");
	}
	return result;
}


LazPerf_RecordSchemaPtr lazperf_new_record_schema(void)
{
	return reinterpret_cast<void *>(new laszip::factory::record_schema);
}

void lazperf_record_schema_push_point(LazPerf_RecordSchemaPtr schema)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	record_schema->push(laszip::factory::record_item::point());
}

void lazperf_record_schema_push_gpstime(LazPerf_RecordSchemaPtr schema)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	record_schema->push(laszip::factory::record_item::gpstime());
}

void lazperf_record_schema_push_rgb(LazPerf_RecordSchemaPtr schema)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	record_schema->push(laszip::factory::record_item::rgb());
}

void lazperf_record_schema_push_extrabytes(LazPerf_RecordSchemaPtr schema, size_t count)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	record_schema->push(laszip::factory::record_item::eb(count));
}

int lazperf_record_schema_size_in_bytes(LazPerf_RecordSchemaPtr schema)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	return record_schema->size_in_bytes();
}

void lazperf_delete_record_schema(LazPerf_RecordSchemaPtr schema)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	delete record_schema;
}

LazPerf_LazVlrPtr lazperf_laz_vlr_from_schema(LazPerf_RecordSchemaPtr schema)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	auto vlr = laszip::io::laz_vlr::from_schema(*record_schema);
	auto *vlr_on_heap = new laszip::io::laz_vlr;
	*vlr_on_heap = vlr;
	return reinterpret_cast<void *>(vlr_on_heap);
}


size_t las_vlr_size(LazPerf_LazVlrPtr laz_vlr)
{
	auto vlr = reinterpret_cast<laszip::io::laz_vlr *>(laz_vlr);
	return vlr->size();
}


LazPerf_SizedBuffer lazperf_laz_vlr_raw_data(LazPerf_LazVlrPtr laz_vle)
{
	LazPerf_SizedBuffer raw_vlr_data{};
	auto vlr = reinterpret_cast<laszip::io::laz_vlr *>(laz_vle);
	char *data = new char[vlr->size()];
	vlr->extract(data);
	raw_vlr_data.size = vlr->size();
	raw_vlr_data.data = data;
	return raw_vlr_data;
}

LazPerf_Result lazperf_compress_points(LazPerf_RecordSchemaPtr schema, size_t offset_to_point_data, const char *points, size_t num_points)
{
	LazPerf_Result result{};
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	size_t point_size = record_schema->size_in_bytes();
	try
	{
		result.is_error = 0;
		VlrCompressor vlr_compressor(*record_schema, offset_to_point_data);
		const char* current_point = points;
		for (size_t i = 0; i < num_points; ++i)
		{
			vlr_compressor.compress(current_point);
			current_point += point_size;
		}

		vlr_compressor.done();
		char *compressed_points = new char[vlr_compressor.data()->size()];
		vlr_compressor.copy_data_to(reinterpret_cast<uint8_t *>(compressed_points));
	}
	catch (const std::exception &e)
	{
		result.is_error = 1;
		result.error.error_msg = strdup(e.what());
	}
	catch (...)
	{
		result.is_error = 1;
		result.error.error_msg = strdup("Unknown error");
	}
	return result;
}


LazPerf_VlrCompressorPtr lazperf_new_vlr_compressor(
		LazPerf_RecordSchemaPtr schema, size_t offset_to_point_data
)
{
	auto record_schema = reinterpret_cast<laszip::factory::record_schema *>(schema);
	auto vlr_compressor = new VlrCompressor(*record_schema, offset_to_point_data);
	return reinterpret_cast<void *>(vlr_compressor);
}

void lazperf_delete_vlr_compressor(LazPerf_VlrCompressorPtr compressor)
{
	auto vlr_compressor = reinterpret_cast<VlrCompressor *>(compressor);
	delete vlr_compressor;
}

size_t lazperf_vlr_compressor_compress(LazPerf_VlrCompressorPtr compressor, const char *inbuf)
{
	auto vlr_compressor = reinterpret_cast<VlrCompressor *>(compressor);
	try
	{
		size_t compressed_size = vlr_compressor->compress(inbuf);
		return compressed_size;
	}
	catch (const std::exception& e) {
		std::cout << e.what() << '\n';
		return 0;
	}
}

size_t lazperf_vlr_compressor_copy_data_to(LazPerf_VlrCompressorPtr compressor, uint8_t *dst)
{
	auto vlr_compressor = reinterpret_cast<VlrCompressor *>(compressor);
	return vlr_compressor->copy_data_to(dst);
}

void lazperf_vlr_compressor_reset_size(LazPerf_VlrCompressorPtr compressor)
{
	auto vlr_compressor = reinterpret_cast<VlrCompressor *>(compressor);
	vlr_compressor->resetStreamPosition();
}

void lazperf_vlr_compressor_done(LazPerf_VlrCompressorPtr compressor)
{
	auto vlr_compressor = reinterpret_cast<VlrCompressor *>(compressor);
	vlr_compressor->done();
}