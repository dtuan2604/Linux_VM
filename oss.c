#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


static FILE *fptr = NULL;
static char *prog;
static char log_file[256] = "output.txt";
static int MAX_PROCESS = MAX_PROC;

static int mqueueid = -1;
static Message master_message;

static int shmclock_shmid = -1;
static int clockCounter; 
static SharedClock *shmclock_shmptr = NULL;

static int semid = -1;
static struct sembuf sema_operation;

static int pcbt_shmid = -1;
static ProcessControlBlock *pcbt_shmptr = NULL;
static void initOSS()
{
	//initialize log file
	fptr = fopen(log_file, "w");
	if(fptr == NULL)
	{
		fprintf(stderr, "%s: ERROR: unable to write the output file.\n", prog);
		exit(EXIT_FAILURE);
	}

	mqueueid = msgget(key_msg, IPC_CREAT | 0600);
	if(mqueueid < 0)
	{
		fprintf(stderr, "%s: ERROR: could not allocate message queue\n", prog);
		cleanUp();
		exit(EXIT_FAILURE);
	}


	shmclock_shmid = shmget(key_shmlock, sizeof(SharedClock), IPC_CREAT | 0600);
	if(shmclock_shmid < 0)
	{
		fprintf(stderr, "%s: ERROR: could not allocate shared memory\n", prog);
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

	shmclock_shmptr->second = 0;
	shmclock_shmptr->nanosecond = 0;
	forkclock.second = 0;
	forkclock.nanosecond = 0;
	clockCounter = 0;

	semid = semget(key_sem, 1, IPC_CREAT | IPC_EXCL | 0600);
	if(semid == -1)
	{
		fprintf(stderr, "%s: ERROR: failed to create semaphore\n", prog);
		cleanUp();
		exit(EXIT_FAILURE);
	}
	
	//initialize the semaphore in our set to 1
	semctl(semid, 0, SETVAL, 1);	
	

	size_t process_table_size = sizeof(ProcessControlBlock) * MAX_PROCESS;
	pcbt_shmid = shmget(key_pcb, process_table_size, IPC_CREAT | 0600);
	if(pcbt_shmid < 0)
	{
		fprintf(stderr, "%s: ERROR: could not allocate shared memory\n", prog);
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
	
	initOSS();


	return EXIT_SUCCESS;
}
