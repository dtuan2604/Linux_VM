#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


static char* prog;
static int num_proc = MAX_PROC;

int main(int argc, char** argv){
	prog = argv[0];
	int opt;
	while ((opt = getopt(argc, argv, "p:")) > 0){
    		switch (opt){
    			case 'p':
 	     			num_proc = atoi(optarg);
				break;
			case '?':
                                if(optopt == 'p')
                                	fprintf(stderr,"%s: ERROR: -%c without argument\n",prog,optopt);
                                else
                                        fprintf(stderr, "%s: ERROR: Unrecognized option: -%c\n",prog,optopt);
                                return -1;
    		}	
	}
	
	if(num_proc > MAX_PROC)
		num_proc = MAX_PROC;
	printf("%d\n",num_proc);


	return EXIT_SUCCESS;
}
