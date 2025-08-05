#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include "../include/file_identificator.h"

#define L1_BUFF_SIZE 1000000

int print_RowInfo(RowInfo* ri_p) {
	if (ri_p == NULL){
		printf("provided RowInfo pointer is a NULL Pointer\n");
		return -1;
	} else {
		return printf(
			"RowInfo @%p contains {string=%p, count=%i, length=%i}\n",
			(void *) ri_p, (void *) ri_p->string, ri_p->count, ri_p->length
		);
	}
}

int identify_line(RowInfo* info, size_t max_length) {
	if (max_length > MAX_LINE_SIZE) max_length = MAX_LINE_SIZE;
	info->eol_flag = EOL_AUTO;
	int32_t counter = 1; // Always at least 1 field in a row, even if empty
	char* i;
	for (i=info->string ; (i < info->string + max_length); ++i){
		counter += (*i == ',');
		if (*i == '\n'){
			// printf("last char values = [%hhu, %hhu, %hhu]\n", i[-1], i[0], i[1]);
			info->eol_flag = (*(i - 1) == '\r') ? EOL_DOS : EOL_UNIX;
			break;
		}
	}
	// if the last char was '\n', `i - info->counter` would equal 0, but there
	// would be 1 char, hence the `+1`.
	info->length = i - info->string + 1;
	// printf("line length is %d\n", info->length);
	info->count = counter;
	if (info->length < 0) return 1;
	else return 0;
}

int identify_L1(RowInfo *info, int fd){
	// get buffer for 1st line
	char* l1buff = malloc(L1_BUFF_SIZE);
	if (l1buff == NULL){
		printf("out of memory\n");
		return EX_OSERR; // Out of memory
	}
	// read first line without moving the file pointer
	int len = pread(fd, l1buff, L1_BUFF_SIZE, 0);
	if (len < 0) {
		printf("pread failed\n");
		free(l1buff);
		return EX_IOERR; // Error reading 1st line of source
	}

	// value was not initialised yet !!!
	info->string = l1buff;
	if (identify_line(info, L1_BUFF_SIZE)) {
		printf("identify_line failed\n");
		free(l1buff);
		return EX_DATAERR; // Could not find end of line!
	}

	info->string = NULL; // preventing reading later
	free(l1buff);
	return EX_OK;
}
