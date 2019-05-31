#ifndef FUCKTHIS_LIBRARY_H
#define FUCKTHIS_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RawPointsBuffer {
	char *points;
	size_t buffer_size;
};

struct LazPerfError {
	const char *error_msg;
};


struct LazPerfResult {
	int is_error;
	union {
		struct RawPointsBuffer points_buffer;
		struct LazPerfError error;
	};
};


struct LazPerfResult lazperf_decompress_points(
		const uint8_t *compressed_points_buffer,
		size_t buffer_size,
		const char *lazsip_vlr_data,
		size_t num_points,
		size_t point_size
);
#ifdef __cplusplus
};
#endif

#endif //FUCKTHIS_LIBRARY_H