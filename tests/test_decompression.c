#include "lazperf_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define LAS_HEADER_SIZE 227
#define VLR_HEADER_SIZE 54
#define OFFSET_TO_LASZIP_VLR_DATA (LAS_HEADER_SIZE + VLR_HEADER_SIZE)
#define LASZIP_VLR_DATA_SIZE 52
#define OFFSET_TO_POINT_DATA (OFFSET_TO_LASZIP_VLR_DATA + LASZIP_VLR_DATA_SIZE)
#define SIZEOF_CHUNK_TABLE_OFFSET 8
#define POINT_COUNT 1065

int main(int argc, char *argv[]) {
	hello();

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

	char* decompressed_points = lazperf_decompress_points((uint8_t *)compressed_points, point_data_size, laszip_vlr_data, POINT_COUNT, 34);
	hello();

	FILE *decompresed_points_file = fopen("./tests/data/simple_points_uncompressed.bin", "rb");
	if (decompresed_points_file == NULL) {
		perror("fopen() failed");
		return EXIT_FAILURE;
	}
	char *expected_points = malloc(POINT_COUNT * 34 * sizeof(char));
	fread(expected_points, sizeof(char), POINT_COUNT * 34, decompresed_points_file);

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