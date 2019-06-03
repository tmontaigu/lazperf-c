#ifndef FUCKTHIS_LIBRARY_H
#define FUCKTHIS_LIBRARY_H

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
struct LazPerf_Result
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
void lazperf_delete_result(struct LazPerf_Result *result);

void lazperf_delete_sized_buffer(struct LazPerf_SizedBuffer *buffer);

/* Record Schema */


/*
 * Record Schemas are used by compressors to know how the input (non-compressed points)
 * have their fields ordered, so you have to use the push methods in the right order.
 *
 */
typedef void *LazPerf_RecordSchemaPtr;

/*
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

/* Laszip VLR */

typedef void *LazPerf_LazVlrPtr;

LazPerf_LazVlrPtr lazperf_laz_vlr_from_schema(LazPerf_RecordSchemaPtr schema);

size_t lazperf_laz_vlr_size(LazPerf_LazVlrPtr laz_vlr);

struct LazPerf_SizedBuffer lazperf_laz_vlr_raw_data(LazPerf_LazVlrPtr laz_vle);


/* Decompression API */
/**
 * Decompress the points contained in the buffer
 *
 * @param compressed_points_buffer The buffer of points to decompress
 * @param lazsip_vlr_data The record data of the Laszip Vlr
 * @param num_points number of points stored (and to be decompressed) in the input buffer
 * @param point_size size of one point in bytes (each points have the same size)
 * @return The result of the decompression
 */
struct LazPerf_Result lazperf_decompress_points(
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

struct LazPerf_Result lazperf_compress_points(
		LazPerf_RecordSchemaPtr schema,
		size_t offset_to_point_data,
		const char *points,
		size_t num_points
);

typedef void *LazPerf_VlrCompressorPtr;

LazPerf_VlrCompressorPtr lazperf_new_vlr_compressor(LazPerf_RecordSchemaPtr schema);

size_t lazperf_vlr_compressor_compress(LazPerf_VlrCompressorPtr compressor, const char *inbuf);

size_t lazperf_vlr_compressor_copy_data_to(LazPerf_VlrCompressorPtr compressor, uint8_t *dst);

size_t lazperf_vlr_compressor_extract_data_to(LazPerf_VlrCompressorPtr compressor, uint8_t *dst);

void lazperf_delete_vlr_compressor(LazPerf_VlrCompressorPtr compressor);

const uint8_t *lazperf_vlr_compressor_internal_buffer(LazPerf_VlrCompressorPtr compressor);

size_t lazperf_vlr_compressor_internal_buffer_size(LazPerf_VlrCompressorPtr compressor);

void lazperf_vlr_compressor_reset_size(LazPerf_VlrCompressorPtr compressor);

uint64_t lazperf_vlr_compressor_done(LazPerf_VlrCompressorPtr compressor);

uint64_t lazperf_vlr_compressor_write_chunk_table(LazPerf_VlrCompressorPtr compressor);

struct LazPerf_SizedBuffer lazperf_vlr_compressor_vlr_data(LazPerf_VlrCompressorPtr compressor);

#ifdef __cplusplus
};
#endif

#endif //FUCKTHIS_LIBRARY_H