#ifndef LAZPERF_C_H
#define LAZPERF_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct LazPerf_SizedBuffer
{
	char *data;
	size_t size;
};

struct LazPerf_Error
{
	char *error_msg;
};

/**
 * Result of a compression / decompression.
 * It the result is an error "is_error" will be set to 1
 *
 * Both elements of the union (SizedBuffer & Error) owns memory,
 * so you will have to use 'lazperf_delete_result' one you are done with the result.
 * (unless you decide to take ownership)
 */
struct LazPerf_BufferResult
{
	int is_error;
	union
	{
		struct LazPerf_SizedBuffer points_buffer;
		struct LazPerf_Error error;
	};
};


/*
 * Frees the memory owned by either variant of the result union
 */
void lazperf_delete_result(struct LazPerf_BufferResult *result);

void lazperf_delete_sized_buffer(struct LazPerf_SizedBuffer buffer);

/* Record Schema */


/**
 * Record Schemas are used by compressors to know how the input (non-compressed points)
 * have their fields ordered, so you have to use the push methods in the right order.
 *
 */
typedef void *LazPerf_RecordSchemaPtr;

/**
 * Creates a new Record Schema
 */
LazPerf_RecordSchemaPtr lazperf_new_record_schema(void);

/**
 * Deletes the Record Schema
 */
void lazperf_delete_record_schema(LazPerf_RecordSchemaPtr schema);

/**
 * Push the 'point' record item in the Record Schema
 * 'point' being the dimensions of the Point Format 0 of the LAS format
 */
void lazperf_record_schema_push_point(LazPerf_RecordSchemaPtr schema);

/**
 * Push the gpstime record item.
 * To be used if the points to be compressed have gps time.
 */
void lazperf_record_schema_push_gpstime(LazPerf_RecordSchemaPtr schema);

/**
 * Push the rgb record item.
 * To be used if the points to be compressed have rgb.
 */
void lazperf_record_schema_push_rgb(LazPerf_RecordSchemaPtr schema);

/**
 * Push extra bytes record item.
 * To be used if the points to be compressed have extra bytes.
 *
 * count is number of extra bytes (as defined in the LAS format) in the point.
 */
void lazperf_record_schema_push_extrabytes(LazPerf_RecordSchemaPtr schema, size_t count);

/**
 * Returns the point size that the schema represents.
 */
int lazperf_record_schema_size_in_bytes(LazPerf_RecordSchemaPtr schema);

/**
 * Returns a copy of the LazVlr record data corresponding to the record schema
 * @param schema
 * @return
 */
struct LazPerf_SizedBuffer lazperf_record_schema_laz_vlr_data(LazPerf_RecordSchemaPtr schema);


/* laszip VLR */

/**
 *  LazVlr, only useful to get the record data of the vlr
 */
typedef void *LazPerf_LazVlrPtr;

/**
 * Creates a new LazVlr
 *
 * @param schema
 * @return the new instance
 */
LazPerf_LazVlrPtr lazperf_new_laz_vlr_from_schema(LazPerf_RecordSchemaPtr schema);

/**
 * Deletes the laz vlr instance
 *
 * @param vlr
 */
void lazperf_delete_laz_vlr(LazPerf_LazVlrPtr vlr);

/**
 * Returns the size of the LazVlr's record_data
 *
 * @param vlr
 * @return the size in bytes
 */
size_t lazperf_laz_vlr_record_data_size(LazPerf_LazVlrPtr vlr);

/**
 * copty the vlr's data to the buffer
 * @param vlr
 * @param out buffer where the data will be written, MUST be preallocated with the same size as the
 * laz_vlr_size
 */
void lazperf_laz_vlr_copy_record_data(LazPerf_LazVlrPtr vlr, char *out);



/* Decompression API */

/* Functions here are meant to be used by LAS/LAZ readers */

/**
 * Decompress the points contained in the buffer.
 *
 * @param compressed_points_buffer The buffer of points to decompress
 * @param lazsip_vlr_data The record data of the Laszip Vlr
 * @param num_points number of points stored (and to be decompressed) in the input buffer
 * @param point_size size of one point in bytes (each points have the same size)
 * @return The result of the decompression
 */
struct LazPerf_BufferResult lazperf_decompress_points(
		const uint8_t *compressed_points_buffer,
		size_t buffer_size,
		const char *lazsip_vlr_data,
		size_t num_points,
		size_t point_size
);

/**
 * Decompress the points contained in the buffer into the input buffer
 *
 * @param compressed_points_buffer The buffer of points to decompress
 * @param lazsip_vlr_data The record data of the Laszip Vlr
 * @param num_points number of points stored (and to be decompressed) in the input buffer
 * @param point_size size of one point in bytes (each points have the same size)
 * @return The result of the decompression
 */
void lazperf_decompress_points_into(
		const uint8_t *compressed_points_buffer,
		size_t buffer_size,
		const char *lazsip_vlr_data,
		size_t num_points,
		size_t point_size,
		uint8_t *out_buffer
);

/**
 * Structure able to decompress points taken from a LAZ file
 *
 * You use this structure if you want to achieve a semi-streaming decompression
 */
typedef void *LazPerf_VlrDecompressorPtr;

/**
 * Creates a VlrDecompressor
 *
 * @param compressed_buffer buffer containing compressed points
 * @param buffer_size size of the buffer
 * @param point_size size of one uncompressed point in bytes
 * @param laszip_vlr_data record data of the laszip vlr found in the LAZ file
 * @return
 */
LazPerf_VlrDecompressorPtr lazperf_new_vlr_decompressor(
		const uint8_t *compressed_buffer,
		size_t buffer_size,
		size_t point_size,
		const char *laszip_vlr_data
);

/**
 * Deletes the VlrDecompressor
 */
void lazperf_delete_vlr_decompressor(LazPerf_VlrDecompressorPtr decompressor);

/**
 * Decompress the next point in the buffer
 *
 * @param decompressor the decompressor instance
 * @param out where the decompressed point data will be written.
 * It is assumed that the 'out' buffer is at least point_size long otherwise its undefined behaviour
 */
void lazperf_vlr_decompressor_decompress_one_to(LazPerf_VlrDecompressorPtr decompressor, char *out);


/* Compression API */

/**
 * Compress the points of the 'points' buffer. Intended to be used by LAS/LAZ writers
 *
 * @param schema: record schema of the points contained in the buffer
 * @param offset_to_point_data: offset in bytes to the start of point records(see LAS specification)
 * this is required because the first 8 bytes of the compressed points is an offset to the chunk table
 * that is written after the points are compressed, the chunk table offest is relative to the start of the LAZ
 * file
 * @param points: buffer of points to compress
 * @param num_points: number of points in the buffer
 * @return buffer of compressed points, with the offset to chunk table and the chunk table included,
 * so the returned buffre can directly be dumbed into a LAZ file (after headers & vlrs are written of course)
 */
struct LazPerf_BufferResult lazperf_compress_points(
		LazPerf_RecordSchemaPtr schema,
		size_t offset_to_point_data,
		const char *points,
		size_t num_points
);


/**
 * Structure used to compress points to write them in a LAZ file.
 *
 * Use this to achieve a semi-streaming compression.
 * However correctly using this is a bit involved.
 *
 * Important:
 *    This compressor maintains an internal buffer, where compressed points will be written.
 *    The laszip encoder used by the compressor is 'buffered', meaning that when a point is compressed,
 *    it is not directly written to the internal buffer.
 *
 *    most of the functions returns the internal buffer size, so you know when compressed
 *    data was written to it, to achieve semi-streaming and save memory, you'll have
 *    to 'extract' (copy then reset size) the data of the internal buffer
 *
 * How to use:
 *  1) Create an instance of the VlrCompressor
 *  2) call compress while you have points to compress
 *     on each call if compress returns 0 continue else extract the compressed data
 *     wherever you like
 *   3) call done when you are done compressing points
 *   4) extract data
 *   5) write chunk_table
 *   6) extract data
 *   7) delete compressor
 *
 *   You will also have to update the first 8 bytes of the compressed data, as it is the offset the chunk table
 */
typedef void *LazPerf_VlrCompressorPtr;

/**
 * Creates a new VlrCompressor
 *
 * @param schema : schema of the points to be compressed
 * @return the new instance
 */
LazPerf_VlrCompressorPtr lazperf_new_vlr_compressor(LazPerf_RecordSchemaPtr schema);

/**
 * Delete the compressor instance
 *
 * @param compressor the instance to delete
 */
void lazperf_delete_vlr_compressor(LazPerf_VlrCompressorPtr compressor);

/**
 * Compress one point
 *
 * @param compressor the compressor instance
 * @param inbuf point to compress, if the 'inbuf''s size is less than the point size specified by the
 * record schema used when creating the instance, you'll get UB
 * @return the size of the internal buffer after the point was compressed
 */
size_t lazperf_vlr_compressor_compress(LazPerf_VlrCompressorPtr compressor, const char *inbuf);

/**
 * Returns the size (in bytes) of the compressor's internal buffer
 *
 * @param compressor the instance
 * @return the size in bytes
 */
size_t lazperf_vlr_compressor_internal_buffer_size(LazPerf_VlrCompressorPtr compressor);

/**
 * Copies the data contained in the compressor's internal buffer to 'dst'
 *
 * @param compressor the instance
 * @param dst where the data will be copied, dst MUST be a valid pointer pre-allocated with
 * at least the same size as the compressor's internal buffer
 * @return the number of bytes copied
 */
size_t lazperf_vlr_compressor_copy_data_to(LazPerf_VlrCompressorPtr compressor, uint8_t *dst);

/**
 * Copies the data contained in the compressor's internal buffer to 'dst'
 * and then empties the internal buffer of the compressor
 *
 * @param compressor
 * @param dst
 * @return the number of bytes copied
 */
size_t lazperf_vlr_compressor_extract_data_to(LazPerf_VlrCompressorPtr compressor, uint8_t *dst);

/**
 * Returns the pointer to the internal buffer of the compressor
 *
 * @param compressor
 * @return
 */
const uint8_t *lazperf_vlr_compressor_internal_buffer(LazPerf_VlrCompressorPtr compressor);

/**
 * Resets the size of the compressor's internal buffer to 0.
 * calls to 'compress', 'done', 'write_chunk_table' made after a call to this method
 * will effectively overwrite data.
 *
 * @param compressor
 */
void lazperf_vlr_compressor_reset_size(LazPerf_VlrCompressorPtr compressor);

/**
 * Tells the compressor is done compressing points.
 * To be called when all points when you have compressed all the points you wanted
 *
 * @param compressor
 * @return size of the internal buffer
 */
uint64_t lazperf_vlr_compressor_done(LazPerf_VlrCompressorPtr compressor);

/**
 * Writes the chunk table to the internal buffer of the compressor
 *
 * @param compressor
 * @return size of the internal buffer
 */
uint64_t lazperf_vlr_compressor_write_chunk_table(LazPerf_VlrCompressorPtr compressor);

/**
 * Returns the record data of the LASZIP vlr
 *
 * @param compressor
 * @return
 */
struct LazPerf_SizedBuffer lazperf_vlr_compressor_vlr_data(LazPerf_VlrCompressorPtr compressor);

#ifdef __cplusplus
};
#endif

#endif //LAZPERF_C_H