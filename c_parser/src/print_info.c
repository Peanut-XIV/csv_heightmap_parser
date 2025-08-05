#include <unistd.h>
#include <stdio.h>
#include <limits.h>

void print_sizes(){
	printf("=========================\n");
	printf("dtype         | byte size\n");
	printf("char          | %lu\n", sizeof(char));
	printf("short         | %lu\n", sizeof(short));
	printf("int           | %lu\n", sizeof(int));
	printf("long int      | %lu\n", sizeof(long int));
	printf("long long int | %lu\n", sizeof(long long int));
	printf("float         | %lu\n", sizeof(float));
	printf("double        | %lu\n", sizeof(double));
	printf("=========================\n");
}


int main(int argc, char* argv[]){
	printf("page size is %li bytes\n", sysconf(_SC_PAGE_SIZE));
	printf("max path length is %d bytes\n", PATH_MAX);
	printf("NULL pointer is %p\n", NULL);
	print_sizes();
	return 0;
}
