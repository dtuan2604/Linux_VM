#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


static char* prog;
static int MAX_PROCESS = MAX_PROC;

int main(int argc, char** argv){
	prog = argv[0];
	srand(getpid());

	int opt;
	while ((opt = getopt(argc, argv, "p:")) > 0){
		switch (opt){
			case 'p':
				MAX_PROCESS = atoi(optarg);
				break;
			case '?':
				if(optopt == 'p')
					fprintf(stderr,"%s: ERROR: -%c without argument\n",prog,optopt);
				else
						fprintf(stderr, "%s: ERROR: Unrecognized option: -%c\n",prog,optopt);
				return -1;
		}	
	}
	
	if(MAX_PROCESS > MAX_PROC)
		MAX_PROCESS = MAX_PROC;
	printf("%d\n",MAX_PROCESS);


	return EXIT_SUCCESS;
}
