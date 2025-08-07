#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include "../include/arg_parse.h"

#define BUFFSIZE 20000

#define MAX_SEGMENT_COUNT 100

#define Params_Default__tile_width 1000
#define Params_Default__tile_height 1000
#define Params_Default__min_field_size 5
#define Params_Default__max_field_size 7

void print_usage(void){
	printf(
"usage: splitter [options] <source_file> <destination_dir>\n\n"
"options\n\n"
"--help   : print this message and exit.\n\n"
"-w <int> : desired width of output tiles.\n"
"           default value is 1000, goes from 1 to 65535, zero is default.\n\n"
"-h <int> : desired height of output tiles.\n"
"           default value is 1000, goes from 1 to 65535, zero is default.\n\n"
"-m <int> : minimum field size in bytes, must not include separator.\n"
"           default value is 5, goes from 1 to 255, zero is default.\n\n"
"-M <int> : maximum field size in bytes, must not include separator.\n"
"           default value is 7, goes from 1 to 255, zero is default\n\n"
"-d / -u  : newline format of the source file. use `-d` for dos style (\\r\\n)\n"
"           and `-u` for unix style (\\n). no flag means automatic detection.\n\n"
"notes:\n"
"the numeric values given in arguments must be separated from flags by a space.\n"
	);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int parse_args(int argc, char* argv[], Params* params) {
	params->tile_width = Params_Default__tile_width;
	params->tile_height = Params_Default__tile_height;
	params->min_field_size = Params_Default__min_field_size;
	params->max_field_size = Params_Default__max_field_size;
	params->eol_flag = EOL_AUTO;

	if (argc < 3) {
		printf("Error: Expected at least 2 arguments and got %d", argc);
		return 1;
	}

	// get source and destination
	strlcpy(params->source, argv[argc - 2], PATH_MAX);
	strlcpy(params->dest, argv[argc - 1], PATH_MAX);

	// skip program name and calc remaining number of args
	argv = argv + 1;
	argc -= 3;
	char* arg;

	// iterate over options
	while (argc > 0) {
		arg = argv[0];
		if (strcmp(arg, "--help") == 0) {
			printf("[Printing help]\n");
			return 1;
		}
		else if (strcmp(arg, "-h") == 0) {
			char* arg2 = argv[1];
			int val = -1;
			if (argc > 1) {
				val = atoi(arg2);
			} else {
				printf("Error: Missing argument for option `-h`\n");
				return 1;
			}
			if (val < 0 || val > USHRT_MAX) {
				printf("Error: Invalid value for option `-h`\n");
				return 1;
			} else if (val != 0) {
				params->tile_height = val;
			}
			argc -=2;
			argv +=2;
		}
		else if (strcmp(arg, "-w") == 0) {
			char* arg2 = argv[1];
			int val = -1;
			if (argc > 1) {
				val = atoi(arg2);
			} else {
				printf("Error: Missing argument for option `-w`\n");
				return 1;
			}
			if (val < 0 || val > USHRT_MAX) {
				printf("Error: Invalid value for option `-w`\n");
				return 1;
			} else if (val != 0) {
				params->tile_width = val;
			}
			argc -=2;
			argv +=2;
		}
		else if (strcmp(arg, "-m") == 0) {
			char* arg2 = argv[1];
			int val = -1;
			if (argc > 1) {
				val = atoi(arg2);
			} else {
				printf("Error: Missing argument for option `-m`\n");
				return 1;
			}
			if (val < 0 || val > UCHAR_MAX) {
				printf("Error: Invalid value for option `-m`\n");
				return 1;
			} else if (val != 0) {
				params->min_field_size = val;
			}
			argc -=2;
			argv +=2;
		}
		else if (strcmp(arg, "-M") == 0) {
			char* arg2 = argv[1];
			int val = -1;
			if (argc > 1) {
				val = atoi(arg2);
			} else {
				printf("Error: Missing argument for option `-M`\n");
				return 1;
			}
			if (val < 0 || val > UCHAR_MAX) {
				printf("Error: Invalid value for option `-M`\n");
				return 1;
			} else if (val != 0) {
				params->max_field_size = val;
			}
			argc -=2;
			argv +=2;
		}
		else if (strcmp(arg, "-d") == 0) {
			params->eol_flag = EOL_DOS;
			argc -=1;
			argv +=1;
		}
		else if (strcmp(arg, "-u") == 0) {
			params->eol_flag = EOL_UNIX;
			argc -=1;
			argv +=1;
		}
	}
	return 0;
}


void show_params(Params* paramsp) {
	printf("Tiles are of size %d x %d, file is of type ", paramsp->tile_width, paramsp->tile_height);
	if (paramsp->eol_flag == EOL_AUTO) printf("AUTO\n");
	else if (paramsp->eol_flag == EOL_DOS) printf("DOS (CR+LF)\n");
	else printf("UNIX (\\n)\n");
	printf("Fields have a size going between %u and %u.\n", paramsp->min_field_size, paramsp->max_field_size);
	printf("target file has path `%s`\n", paramsp->source);
	printf("destination directory has path `%s`\n", paramsp->dest);
}

int match_words(const char* s1, const char* s2, size_t len){
	for (size_t i = 0; i < len; i++){
		if (s1[i] != s2[i]) return 0;
		else if (s1[i] == '\0') break;
	}
	return 1;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int parse_config_file_line(const Segment* line, Config* conf){
	if (
		line->start==NULL
		|| line->end==NULL
		|| (line->end - line->start <= 0)
	) {
		printf(
			"reading line going from %p to %p (∆ = %li bytes)\n",
			(void *) line->start,
			(void *) line->end,
			line->end - line->start
		);
	}
	// find keyword delimitations
	int line_size = line->end - line->start;
	char* value_start = memchr(line->start, '=', line_size);
	if (value_start == NULL){
		printf("Error: invalid statement, equal sign missing\n");
		return 1;
	}
	value_start++;

	char minfield[] = "min_field_size";
	char maxfield[] = "max_field_size";
	char outfield[] = "output_field_size";
	char eol[] = "eol_flag";
	char tile_w[] = "tile_width";
	char tile_h[] = "tile_height";
	char source[] = "source";
	char dest[] = "dest";

	const char MAX_SPACE_EQ_TO_VAL = 100;

	if (match_words(line->start, minfield, sizeof(minfield) - 1)){
		conf->min_field_size = atoi(value_start);
	}
	else if (match_words(line->start, maxfield, sizeof(maxfield) - 1)){
		conf->max_field_size = atoi(value_start);
	}
	else if (match_words(line->start, outfield, sizeof(outfield) - 1)){
		conf->output_field_size = atoi(value_start);
	}
	else if (match_words(line->start, tile_w, sizeof(tile_w) - 1)){
		conf->tile_width = atoi(value_start);
	}
	else if (match_words(line->start, tile_h, sizeof(tile_h) - 1)){
		conf->tile_height = atoi(value_start);
	}
	else if (match_words(line->start, eol, sizeof(eol) - 1)){
		while(*value_start == ' ') value_start++;
		switch (*value_start) {
			case 'd':
				conf->eol_flag = EOL_DOS;
				break;
			case 'u':
				conf->eol_flag = EOL_UNIX;
				break;
			case 'a':
				conf->eol_flag = EOL_AUTO;
				break;
			default:
				printf("Unrecognized EOL Flag, fallback to AUTO\n");
				conf->eol_flag = EOL_AUTO;
		}
	}
	else if (match_words(line->start, source, sizeof(source) - 1)){
		char* first_quote = memchr(value_start, '"', MAX_SPACE_EQ_TO_VAL);
		if (first_quote == NULL) {
			printf("Error: first quotation mark around source path not found\n");
			return 1;
		}
		char* path_start = first_quote + 1;

		char* second_quote = memchr(path_start, '"', PATH_MAX);
		if (second_quote == NULL) {
			printf("Error: second quotation mark around source path not found\n");
			return 1;
		}
		char* path_end = second_quote - 1;

		int size = path_end - path_start + 1;
		if (size >= PATH_MAX) {
			printf("Error: source path too long\n");
			return 1;
		}

		strlcpy(conf->source, path_start, size + 1); // +1 for \0
	}
	else if (match_words(line->start, dest, sizeof(dest) - 1)){
		char* first_quote = memchr(value_start, '"', MAX_SPACE_EQ_TO_VAL);
		
		if (first_quote == NULL) {
			printf("Error: first quotation mark around dest path not found\n");
			return 1;
		}
		char *path_start = first_quote + 1;

		char* second_quote = memchr(path_start, '"', PATH_MAX);
		if (second_quote == NULL) {
			printf("Error: second quotation mark around dest path not found\n");
			return 1;
		}
		char* path_end = second_quote - 1;

		int size = path_end - path_start + 1;
		if (size >= PATH_MAX) {
			printf("Error: dest path too long\n");
			return 1;
		}

		strlcpy(conf->dest, path_start, size + 1); // +1 for \0
	}
	else {
		const short ERR_MSG_LEN = 1000;
		char* string = malloc(ERR_MSG_LEN);
		if (string != NULL) {
			strlcpy(string, line->start, line_size);
			string[line_size] = '\0';
			printf("Error: Unexpected keyword in line :\n%s\n", string);
			free(string);
			return 1;
		}
		printf("Error: Unexpected keyword\n");
		return 1;
	}
	return 0;
}

int get_config(const char* path, Config* conf){
	printf("READ CONFIG\n");
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("Error opening file. %d: %s\n", errno, strerror(errno));
		errno = 0;
		return 1;
	}

	printf("allocating memory for reading config file\n");
	char* buff = malloc(BUFFSIZE);
	if (buff==NULL){
		printf("Error Out of Memory :/\n");
		close(fd);
		return 1;
	}

	printf("pasting file content to buffer\n");
	ssize_t len = read(fd, buff, BUFFSIZE);

	close(fd);
	if (len < 0) {
		printf("Error reading file. %d: %s\n", errno, strerror(errno));
		errno = 0;
		free(buff);
		return 1;
	}

	if (len >= BUFFSIZE) {
		printf("Error config file too big (it's > 20 000 Bytes, why?)\n");
		errno = 0;
		free(buff);
		return 1;
	}

	// there shouldn't even be 100 real lines.
	Segment lines[MAX_SEGMENT_COUNT] = {0};
	// read lines
	char* p_start = buff;
	char* p_end;
	char* f_end = buff + len;

	int read_lines = 0;
	int saved_lines = 0;
	printf("reading buffer containing the config file's content\n");
	while(p_start < f_end){
		// ------------------- setup -------------------
		// printf("searching for eol on line %d\n", line_counter);
		p_end = memchr(p_start, '\n', buff + BUFFSIZE - p_start);
		if (p_end == NULL) p_end = buff + BUFFSIZE;

		// ----------------- code here -----------------
		// strip spaces
		// printf("stripping spaces on line %d\n", line_counter);
		while (*p_start == ' ' && p_start < f_end) p_start++;

		// save line if 1st char of identifier is a letter
		if (isalpha((int) *p_start)) {
			printf("saving line %d\n", read_lines);
			if (saved_lines >= MAX_SEGMENT_COUNT) {
				printf("too many lines to parse, skipping\n");
				break;
			}
			lines[saved_lines].start = p_start;
			lines[saved_lines].end = p_end;
			saved_lines++;
		} else {
			//printf("not saving line %d\n", line_counter);
		}
		// ------------------- setup -------------------
		p_start = p_end + 1;
		read_lines++;
	}
	// parse every identified line
	printf("parsing saved lines\n");
	for (int i=0; i<saved_lines; i++){
		if (parse_config_file_line(&lines[i], conf)) {
			printf("weird value detected\n");
			free(buff);
			return 1;
		}
	}
	free(buff);
	return 0;
}
