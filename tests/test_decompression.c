#include "lazperf_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <lazperf_c.h>


#define LAS_HEADER_SIZE 227
#define VLR_HEADER_SIZE 54
#define OFFSET_TO_LASZIP_VLR_DATA (LAS_HEADER_SIZE + VLR_HEADER_SIZE)
#define LASZIP_VLR_DATA_SIZE 52
#define OFFSET_TO_POINT_DATA (OFFSET_TO_LASZIP_VLR_DATA + LASZIP_VLR_DATA_SIZE)
#define SIZEOF_CHUNK_TABLE_OFFSET 8
#define POINT_COUNT 1065


int test_successful_decompression() {
	FILE *laz_file = fopen("./tests/data/simple.laz", "rb");
	if (laz_file == NULL) {
		perror("fopen() of \"simple.laz\" failed");
		return EXIT_FAILURE;
	}

	if (fseek(laz_file, OFFSET_TO_LASZIP_VLR_DATA, SEEK_SET) != 0) {
		printf("Failed to seek to start of laszip vlr data\n");
		return EXIT_FAILURE;
	}
	assert(ftell(laz_file) == OFFSET_TO_LASZIP_VLR_DATA);

	char *laszip_vlr_data = malloc(LASZIP_VLR_DATA_SIZE * sizeof(char));
	size_t r = fread(laszip_vlr_data, sizeof(char), LASZIP_VLR_DATA_SIZE, laz_file);


	if(fseek(laz_file, SIZEOF_CHUNK_TABLE_OFFSET, SEEK_CUR) != 0) {
		perror("Failed to seek past chunk table offset");
		fclose(laz_file);
		free(laszip_vlr_data);
	}

	size_t point_data_size = 18217 - (LAS_HEADER_SIZE + VLR_HEADER_SIZE + LASZIP_VLR_DATA_SIZE + SIZEOF_CHUNK_TABLE_OFFSET);
	char *compressed_points = malloc(point_data_size * sizeof(char));
	r = fread(compressed_points, sizeof(char), point_data_size, laz_file);
	assert(ftell(laz_file) == 18217);

	struct LazPerf_Result result = lazperf_decompress_points((uint8_t *)compressed_points, point_data_size, laszip_vlr_data, POINT_COUNT, 34);
	if (result.is_error) {
		printf("Failed to decompress: %s\n", result.error.error_msg);
		return EXIT_FAILURE;
	}

	FILE *decompresed_points_file = fopen("./tests/data/simple_points_uncompressed.bin", "rb");
	if (decompresed_points_file == NULL) {
		perror("fopen() failed");
		return EXIT_FAILURE;
	}
	char *expected_points = malloc(POINT_COUNT * 34 * sizeof(char));
	fread(expected_points, sizeof(char), POINT_COUNT * 34, decompresed_points_file);

	char *decompressed_points = result.points_buffer.data;
	for (size_t i = 0; i < POINT_COUNT * 34; ++i) {
		assert(expected_points[i] == decompressed_points[i]);
	}


	fclose(laz_file);
	fclose(decompresed_points_file);
	free(compressed_points);
	free(expected_points);
	free(laszip_vlr_data);

	return EXIT_SUCCESS;
}


int test_record_schema() {
	LazPerf_RecordSchemaPtr record_schema = lazperf_new_record_schema();
	assert(lazperf_record_schema_size_in_bytes(record_schema) == 0);

	lazperf_record_schema_push_point(record_schema);
	assert(lazperf_record_schema_size_in_bytes(record_schema) == 20);

	lazperf_record_schema_push_gpstime(record_schema);
	assert(lazperf_record_schema_size_in_bytes(record_schema) == 28);

	lazperf_record_schema_push_rgb(record_schema);
	assert(lazperf_record_schema_size_in_bytes(record_schema) == 34);

	lazperf_record_schema_push_extrabytes(record_schema, 6);
	assert(lazperf_record_schema_size_in_bytes(record_schema) == 40);
	lazperf_delete_record_schema(record_schema);

	return EXIT_SUCCESS;
}

int test_compression() {
	FILE *uncompressed_points_file = fopen("./tests/data/simple_points_uncompressed.bin", "rb");
	if (uncompressed_points_file == NULL) {
		perror("fopen() of uncompressed points failed");
		return EXIT_FAILURE;
	}

	char *uncompressed_points = malloc(36210 * sizeof(char));
	fread(uncompressed_points, sizeof(char), 36210, uncompressed_points_file);

	LazPerf_RecordSchemaPtr record_schema = lazperf_new_record_schema();
	lazperf_record_schema_push_point(record_schema);
	lazperf_record_schema_push_gpstime(record_schema);
	lazperf_record_schema_push_rgb(record_schema);

	struct LazPerf_Result result = lazperf_compress_points(record_schema,
														  OFFSET_TO_POINT_DATA + SIZEOF_CHUNK_TABLE_OFFSET,
														  uncompressed_points,
														  POINT_COUNT);

	if (result.is_error) {
		printf("Error when compressing: %s\n", result.error.error_msg);
	}
	else {
	}

	free(uncompressed_points);
	fclose(uncompressed_points_file);
	return 0;
}

int test_streaming_compression() {
	FILE *uncompressed_points_file = fopen("./tests/data/simple_points_uncompressed.bin", "rb");
	if (uncompressed_points_file == NULL) {
		perror("fopen() of uncompressed points failed");
		return EXIT_FAILURE;
	}
	const size_t uncompressed_file_size = 36210;
	uint8_t *uncompressed_points = malloc(uncompressed_file_size * sizeof(uint8_t));
	fread(uncompressed_points, sizeof(uint8_t), uncompressed_file_size, uncompressed_points_file);

	LazPerf_RecordSchemaPtr record_schema = lazperf_new_record_schema();
	lazperf_record_schema_push_point(record_schema);
	lazperf_record_schema_push_gpstime(record_schema);
	lazperf_record_schema_push_rgb(record_schema);
	size_t point_size = lazperf_record_schema_size_in_bytes(record_schema);

	assert(uncompressed_file_size == point_size * POINT_COUNT);

	LazPerf_VlrCompressorPtr compressor = lazperf_new_vlr_compressor(record_schema, 0);

	uint8_t *compressed_output = malloc(sizeof(uint8_t) * uncompressed_file_size);
	size_t bsize = 0;
	for (uint8_t *current = uncompressed_points; current < uncompressed_points + uncompressed_file_size; current += point_size)
	{
		size_t compressed_size = lazperf_vlr_compressor_compress(compressor, (const char *) current);
		if (compressed_size != 0) {
			lazperf_vlr_compressor_copy_data_to(compressor, (uint8_t *) &compressed_output[bsize]);
			bsize += compressed_size;
			lazperf_vlr_compressor_reset_size(compressor);
		}
	}
	lazperf_vlr_compressor_done(compressor);
	lazperf_vlr_compressor_copy_data_to(compressor, (uint8_t *) &compressed_output[bsize]);
	lazperf_delete_vlr_compressor(compressor);

	free(compressed_output);
	free(uncompressed_points);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
	int err = test_successful_decompression();
	test_record_schema();
	err = test_streaming_compression();
	test_compression();
	return err;
}