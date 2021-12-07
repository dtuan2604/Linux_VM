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

//invoke semaphore lock 
static void ossSemWait(int sem_index)
{
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = -1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}
//release semaphore lock of the given semaphore and index.
static void ossSemRelease(int sem_index)
{	
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = 1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}

static void initPCBT(ProcessControlBlock *pcbt)
{
	int i, j;
	for(i = 0; i < MAX_PROCESS; i++)
	{
		pcbt[i].pidIndex = -1;
		pcbt[i].actualPid = -1;
		pcbt[i].ossIndex = -1;
		for(j = 0; j < MAX_PAGE; j++)
		{
			pcbt[i].page_table[j].frameNo = -1;
			pcbt[i].page_table[j].ref = rand() % 2;
			pcbt[i].page_table[j].dirty = 0;
			pcbt[i].page_table[j].valid = 0;
		}
	}		
}

//initialize process control block.
static void initPCB(ProcessControlBlock *pcb, int ossID, int pindex, pid_t pid)
{
	int i;
	pcb->pidIndex = pindex;
	pcb->actualPid = pid;
	pcb->ossIndex = ossID;

	for(i = 0; i < MAX_PAGE; i++)
	{
		pcb->page_table[i].frameNo = -1;
		pcb->page_table[i].ref = rand() % 2;
		pcb->page_table[i].dirty = 0;
		pcb->page_table[i].valid = 0;
	}
}

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
