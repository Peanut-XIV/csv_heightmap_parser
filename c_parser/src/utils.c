#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <windows.h>
#include "../include/custom_dtypes.h"
#endif
#include "../include/utils.h"


#if defined(_WIN32)
size_t getpagesize(void) {
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwAllocationGranularity;
}
#endif


#if defined(__APPLE__) || defined(__LINUX__)
size_t file_size_from_fd(int fildes) {
	struct stat st;
	if (fstat(fildes, &st)) {
		return 0;
	}
	return st.st_size;
}
#endif

#if defined(_WIN32)
uint64_t file_size_from_handle(HANDLE file) {
	BIG_WORD size = {0};
	size.parts[0] = GetFileSize(file, &size.parts[1]);
	if (size.parts[0] == INVALID_FILE_SIZE) return 0;
	return size.full;
}
#endif

void _die(const char e_msg[], int excode, char USAGE[MAX_USAGE]) {
		printf("Error: %s\n", e_msg);
		printf("%s", USAGE);
		exit(excode);
}
