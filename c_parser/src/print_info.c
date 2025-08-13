#include <unistd.h>
#include <stdio.h>
#include <limits.h>

#include "../include/utils.h"

void print_sizes(void){
	printf("=========================" ENDL);
	printf("dtype         | byte size" ENDL);
	printf("char          | %llu" ENDL, (long long int) sizeof(char));
	printf("short         | %llu" ENDL, (long long int) sizeof(short));
	printf("int           | %llu" ENDL, (long long int) sizeof(int));
	printf("long int      | %llu" ENDL, (long long int) sizeof(long int));
	printf("long long int | %llu" ENDL, (long long int) sizeof(long long int));
	printf("float         | %llu" ENDL, (long long int) sizeof(float));
	printf("double        | %llu" ENDL, (long long int) sizeof(double));
	printf("=========================" ENDL);
}


int main(void){
	printf("page size is %lli bytes" ENDL, (long long int) getpagesize());
	printf("max path length is %d bytes" ENDL, PATH_MAX);
	printf("NULL pointer is %p" ENDL, NULL);
	print_sizes();
	return 0;
}
