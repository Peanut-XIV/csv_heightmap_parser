#include <stdlib.h>
#include <stdio.h>

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
