#ifndef FUCKTHIS_LIBRARY_H
#define FUCKTHIS_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct LazPerf_SizedBuffer {
	char *data;
	size_t size;
};

struct LazPerf_Error {
	const char *error_msg;
};


struct LazPerf_Result {
	int is_error;
	union {
		struct LazPerf_SizedBuffer points_buffer;
		struct LazPerf_Error error;
	};
};


struct LazPerf_Result lazperf_decompress_points(
	const uint8_t *compressed_points_buffer,
	size_t buffer_size,
	const char *lazsip_vlr_data,
	size_t num_points,
	size_t point_size
);


typedef void* LazPerf_RecordSchemaPtr;
LazPerf_RecordSchemaPtr lazperf_new_record_schema(void);
void lazperf_delete_record_schema(LazPerf_RecordSchemaPtr schema);
void lazperf_record_schema_push_point(LazPerf_RecordSchemaPtr schema);
void lazperf_record_schema_push_gpstime(LazPerf_RecordSchemaPtr schema);
void lazperf_record_schema_push_rgb(LazPerf_RecordSchemaPtr schema);
void lazperf_record_schema_push_extrabytes(LazPerf_RecordSchemaPtr schema, size_t count);
int lazperf_record_schema_size_in_bytes(LazPerf_RecordSchemaPtr schema);


typedef void* LazPerf_LazVlrPtr;
LazPerf_LazVlrPtr lazperf_laz_vlr_from_schema(LazPerf_RecordSchemaPtr schema);
size_t lazperf_laz_vlr_size(LazPerf_LazVlrPtr laz_vlr);
struct LazPerf_SizedBuffer lazperf_laz_vlr_raw_data(LazPerf_LazVlrPtr laz_vle);

struct LazPerf_Result lazperf_compress_points(LazPerf_RecordSchemaPtr schema, size_t offset_to_point_data, const char *points, size_t num_points);

typedef void* LazPerf_VlrCompressorPtr;
LazPerf_VlrCompressorPtr lazperf_new_vlr_compressor(LazPerf_RecordSchemaPtr schema, size_t offset_to_point_data);
void lazperf_delete_vlr_compressor(LazPerf_VlrCompressorPtr compressor);
size_t lazperf_vlr_compressor_compress(LazPerf_VlrCompressorPtr compressor, const char *inbuf);
size_t lazperf_vlr_compressor_copy_data_to(LazPerf_VlrCompressorPtr compressor, uint8_t *dst);
void lazperf_vlr_compressor_reset_size(LazPerf_VlrCompressorPtr compressor);
void lazperf_vlr_compressor_done(LazPerf_VlrCompressorPtr compressor);

#ifdef __cplusplus
};
#endif

#endif //FUCKTHIS_LIBRARY_H