#include <limits.h>
#include <stddef.h>

#ifndef __CUSTOM_DTYPES_H
#include "custom_dtypes.h"
#endif

void print_usage(void);

int parse_args (int argc, char* argv[], Params* params);

void show_params(Params* paramsp);

int match_words(const char* s1, const char* s2, size_t len);

int get_config(const char* path, Config* conf);
