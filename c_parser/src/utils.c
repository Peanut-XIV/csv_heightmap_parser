#include <stdio.h>
#include <stdlib.h>
#include "../include/utils.h"

void _die(const char e_msg[], int excode, char USAGE[MAX_USAGE]) {
		printf("Error: %s\n", e_msg);
		printf("%s", USAGE);
		exit(excode);
}
