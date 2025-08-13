#ifndef __CUSTOM_DTYPES_H
#define __CUSTOM_DTYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>


#if defined(__APPLE__) || defined(__LINUX__)
#include <limits.h>
#include <stddef.h>
#elif defined(_WIN32)
#include <windows.h>
typedef union {
		DWORDLONG full;
		DWORD parts[2];
	} BIG_WORD; // e.g. "F*CK"
#endif


typedef enum {EOL_AUTO = 0, EOL_UNIX = 1, EOL_DOS = 2} Eol_flag;

typedef struct {
	// statistics about a row of a csv
	char* string;
	int32_t count;
	int32_t length;
	Eol_flag eol_flag;
} RowInfo;

typedef struct {
	// parameters from the regular c_parser usage
	// should be merged with Config struct
	unsigned short tile_width;
	unsigned short tile_height;
	unsigned  char min_field_size;
	unsigned  char max_field_size;
	Eol_flag eol_flag;
	char source[PATH_MAX];
	char dest[PATH_MAX];
} Params;

typedef struct{
	// parameters extracted from a config file
	// should be merged with Params struct
	unsigned short tile_width;
	unsigned short tile_height;
	unsigned  char min_field_size;
	unsigned  char max_field_size;
	unsigned  char output_field_size;
	Eol_flag eol_flag;
	char source[PATH_MAX];
	char dest[PATH_MAX];
} Config;

typedef struct{
	char* start;
	char* end;
} Segment;

typedef struct {
	char eol_size;
	char sep_size;
	char max_field_size;
	char min_field_size;
	int32_t field_count;
	int64_t max_size;
} RowLayout;

typedef struct {
	int32_t page_bytesize;
	int32_t page_count;
	int64_t bytesize;
	char *start;
} ReadBuffer;

typedef struct {
	int32_t row_length;
	int32_t row_count;
	int64_t bytesize;
	float *start;
} CompBuffer;

typedef struct {
	int32_t row_length;
	int32_t row_count;
	int64_t bytesize;
	float* start;
} ProcValBuffer;

typedef struct {
	char* buffer;
	int32_t row_length;
	int32_t row_size;
	int64_t bytesize;
} FileBuffer;

typedef struct {
	char* buffer;
	int64_t bytesize;
	FileBuffer* file_buffers;
	int32_t file_buffer_count;
	char sep_size;
	short field_size;
	char eol_size;
} WriteBuffer;

typedef struct {
	size_t field_count;
	Eol_flag eol;
} LineInfo;

typedef struct {
	short min;
	short max;
} FieldInfo;

typedef struct {
	LineInfo line;
	FieldInfo field;
} ParserConfig;

typedef struct {
	char* buffer;
	int64_t bytesize;
	int32_t row_length;
	int32_t row_bytesize;
	int32_t row_count;
	int eol_size;
} FullFileBuffer;

typedef struct {
	uint64_t fstart_to_page;
	uint64_t page_to_readptr;
	uint64_t fstart_to_readptr;
} MapOffsets;
#endif
