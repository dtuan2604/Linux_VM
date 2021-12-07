#include <stdlib.h>     
#include <stdio.h>   


static void getSharedMemory()
{

	mqueueid = msgget(key_msg, 0600);
	if(mqueueid < 0)
	{
		fprintf(stderr, "%s ERROR: could not get message queue\n", prog);
		cleanUp();
		exit(EXIT_FAILURE);
	}


	shmclock_shmid = shmget(key_shmlock, sizeof(SharedClock), 0600);
	if(shmclock_shmid < 0)
	{
		fprintf(stderr, "%s: ERROR: could not get shared memory\n", prog);
		cleanUp();
		exit(EXIT_FAILURE);
	}


	shmclock_shmptr = shmat(shmclock_shmid, NULL, 0);
	if(shmclock_shmptr == (void *)( -1 ))
	{
		fprintf(stderr, "%s: ERROR: fail to attach shared memory\n", prog);
		cleanUp();
		exit(EXIT_FAILURE);	
	}


	size_t process_table_size = sizeof(ProcessControlBlock) * MAX_PROCESS;
	pcbt_shmid = shmget(key_pcb, process_table_size, 0600);
	if(pcbt_shmid < 0)
	{
		fprintf(stderr, "%s: ERROR: could not get shared memory\n", prog);
		cleanUp();
		exit(EXIT_FAILURE);
	}

	pcbt_shmptr = shmat(pcbt_shmid, NULL, 0);
	if(pcbt_shmptr == (void *)( -1 ))
	{
		fprintf(stderr, "%s: ERROR: fail to attach shared memory\n", prog);
		cleanUp();
		exit(EXIT_FAILURE);	
	}
}
int main(int argc, char *argv[]) 
{
	printf("Hello World\n");
    return EXIT_SUCCESS;
}