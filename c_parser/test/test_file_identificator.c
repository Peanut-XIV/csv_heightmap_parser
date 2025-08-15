#include "../include/file_identificator.h"
#include "../include/ANSI_colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#endif


#define FAIL( str ) RED_BG BLK_FG str DEF_BG DEF_FG
#define PASS( str ) GRN_BG BLK_FG str DEF_BG DEF_FG

typedef enum {UNIX, DOS} EOL_t;

int test_EOL() {
	printf("\tTesting EOL detection\n");
	int fail_count = 0;

	char string_1[] = "qsmlmjhfqsljfmqslfhqmdljf,qsldfjhqsdfhjlqskdf,qsljfqsljif\n";
	RowInfo info = {string_1, 0, -1};
	if (identify_line(&info, sizeof(string_1))) {
		printf("\t\tSimple UNIX EOL detection: " FAIL("FAILED") "\n");
		fail_count += 1;
	} else {
		printf("\t\tSimple UNIX EOL detection: " PASS("PASSED") "\n");
	}

	char string_2[] = "qsmlmjhfqsljfmqslfhqmdljf,qsldfjhqsdfhjlqskdf,qsljfqsljif\r\n";
	RowInfo info2 = {string_1, 0, -1};
	if (identify_line(&info, sizeof(string_2))) {
		printf("\t\tSimple DOS EOL detection: " FAIL("FAILED") "\n");
		fail_count += 1;
	} else {
		printf("\t\tSimple DOS EOL detection: " PASS("PASSED") "\n");
	}

	if (fail_count > 0) return 1;
	else return 0;
}

int test_long_row(EOL_t ftype) {
	char test_name[] = "Long row comma counting and EOL detection";
	char path_dos[] = "/Users/louis/Programming/internship/c_parser/tests/inputs/sniffer_long_row_DOS.csv";
	char path_unix[] = "/Users/louis/Programming/internship/c_parser/tests/inputs/sniffer_long_row_UNIX.csv";
	char* type_name = ftype==UNIX ? "UNIX" : "DOS";
	char* path = ftype==UNIX ? path_unix : path_dos;
	const size_t MAXI_LINE = 400000;


	char* buffer_p = calloc(MAXI_LINE, 1);
	if (buffer_p == NULL) {
		printf("\t\t%s: " FAIL("Not enough Memory for buffer (400 kB)") "\n", test_name);
		printf("Errno %d: %s\n", errno, strerror(errno));
		return 1;
	}

	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		printf("\t\t%s: " FAIL("Could not open %s File") "\n", test_name, type_name);
		printf("Errno %d: %s\n", errno, strerror(errno));
		free(buffer_p);
		return 1;
	}

	int len = fread(buffer_p, 1, MAXI_LINE, fp);
	if (len <= 0) {
		printf("\t\t%s: " FAIL("An error occured while reading a %s file") "\n", test_name, type_name);
		printf("Errno %d: %s\n", errno, strerror(errno));
		free(buffer_p);
		return 1;
	}

	RowInfo info = {buffer_p, 0, -1};
	if (identify_line(&info, MAXI_LINE)) {
		printf("\t\t%s: " FAIL("Failed finding EOL of a %s file") "\n", test_name, type_name);
		printf("File length was %lli bytes.\n", (long long int) len);
		print_RowInfo(&info);
		free(buffer_p);
		return 1;
	}

	char failed = 0;
	const int EXPECTED_VALUE_COUNT = 47731;
	if (info.count != EXPECTED_VALUE_COUNT) {
		printf("\t\t%s: " FAIL("Failed counting number of newlines in %s file") "\n", test_name, type_name);
		print_RowInfo(&info);
		failed = 1;
	}

	if (info.length != len) {
		printf("\t\t%s: " FAIL("Failed counting number of bytes in %s file") "\n", test_name, type_name);
		printf("File length was %lli bytes.\n", (long long int) len);
		print_RowInfo(&info);
		failed = 1;
	}

	if (failed != 1) {
		printf("\t\tLong row comma counting and %s EOL detection: " PASS("PASSED") "\n", type_name);
	}

	free(buffer_p);
	return 0;
}

int main(int argc, char* argv[]){
	printf("starting tests on file_identificator.c\n");
	test_EOL();
	printf("\tTesting long row capabilities\n");
	test_long_row(UNIX);
	test_long_row(DOS);
	return 0;
}
