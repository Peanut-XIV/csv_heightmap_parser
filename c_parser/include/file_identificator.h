#include <stdint.h>
#include <stdio.h>

#if defined(__APPLE__) || defined(__LINUX__)
#include <stddef.h>
#endif

#include "custom_dtypes.h"

# ifdef __ARM_NEON__
#include <arm_neon.h>
# endif

// Max number of chars in a line is 2^31 Bytes or 2 GiB (arbitrary)
// Value chosen to fit in a signed 32 bit interger
#define MAX_LINE_SIZE 2147483648


int print_RowInfo(RowInfo* ri_p);

int identify_line(RowInfo* info, int64_t max_length);

#if defined(__APPLE__) || defined(__LINUX__)
int identify_L1(RowInfo *info, int fd);
#endif

int identify_L1_fp(RowInfo *info, FILE *fp);
