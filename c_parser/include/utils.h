#ifndef __utils_H
#define __utils_H
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#define ENDL "\r\n"
# else
#define ENDL "\n"
#endif

#define MAX_USAGE 200
#define ERR_MSG_SIZE 1000 // Arbitrary value

typedef struct {
	int val;
	char msg[ERR_MSG_SIZE];
} ErrMsg;

void _die(const char e_msg[], int excode, char USAGE[MAX_USAGE]);

#if defined(__APPLE__) || defined(__LINUX__)
size_t get_file_size_fd(int fildes);
#endif

#if defined(_WIN32)
size_t getpagesize(void);

uint64_t file_size_from_handle(HANDLE file);
#endif

#endif
