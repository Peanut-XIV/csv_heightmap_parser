#include <stdlib.h>
#ifdef __ARM_NEON__
#include <stdio.h>
#endif

#include "../include/arg_parse.h"


int main (int argc, char* argv[]) {
	# ifdef __ARM_NEON__
	printf("[Supports NEON]\n");
	# endif
	Params* paramsp = malloc(sizeof(Params));
	if (parse_args(argc, argv, paramsp)) {
		print_usage();
		return 0;
	}
	show_params(paramsp);
	free(paramsp);
	return 0;
}
