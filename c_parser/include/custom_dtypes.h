#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

#define __CUSTOM_DTYPES_H

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
	size_t field_count;
	size_t max_size;
} RowLayout;

typedef struct {
	size_t page_bytesize;
	size_t page_count;
	size_t bytesize;
	char *start;
} ReadBuffer;

typedef struct {
	size_t row_length;
	size_t row_count;
	size_t bytesize;
	float *start;
} CompBuffer;

typedef struct {
	size_t row_length;
	size_t row_count;
	size_t bytesize;
	float* start;
} ProcValBuffer;

typedef struct {
	char* buffer;
	size_t row_length;
	size_t row_size;
	size_t bytesize;
} FileBuffer;

typedef struct {
	char* buffer;
	size_t bytesize;
	FileBuffer* file_buffers;
	size_t file_buffer_count;
	size_t sep_size;
	size_t field_size;
	size_t eol_size;
} WriteBuffer;

typedef struct {
	size_t field_count;
	Eol_flag eol;
} LineInfo;

typedef struct {
	size_t min;
	size_t max;
} FieldInfo;

typedef struct {
	LineInfo line;
	FieldInfo field;
} ParserConfig;

typedef struct {
	char* buffer;
	size_t bytesize;
	size_t row_length;
	size_t row_bytesize;
	size_t row_count;
	int eol_size;
} FullFileBuffer;
