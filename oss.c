#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


static FILE *fptr = NULL;
static char *prog;
static char log_file[256] = "output.txt";
static int MAX_PROCESS = MAX_PROC;
static char debugMode = false;

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
//print message to console and write to a file.
static void printWrite(FILE *fptr, char *fmt, ...)
{
	char buf[BUFFER_LENGTH];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);


	if(fptr != NULL)
	{
		fprintf(fptr, buf);
		fflush(fptr);
	}
}

//send a SIGUSR1 signal to all the child process and user process.
static void killAllChild()
{
	kill(0, SIGUSR1);
	pid_t p = 0;
	while(p >= 0)
	{
		p = waitpid(-1, NULL, WNOHANG);
	}
}
//detach and remove any shared memory.
static void discardShm(int shmid, void *shmaddr, char *shm_name , char *prog, char *process_type)
{
	if(shmaddr != NULL)
	{
		if((shmdt(shmaddr)) << 0)
		{
			fprintf(stderr, "%s (%s) ERROR: could not detach %s shared memory\n", prog, process_type, shm_name);
		}
	}
	
	if(shmid > 0)
	{
		if((shmctl(shmid, IPC_RMID, NULL)) < 0)
		{
			fprintf(stderr, "%s (%s) ERROR: could not delete %s shared memory\n", prog, process_type, shm_name);
		}
	}
}
//release all shared memory and delete all message queue, shared memory, and semaphore.
static void cleanUp()
{
	if(mqueueid > 0)
	{
		msgctl(mqueueid, IPC_RMID, NULL);
	}

	//release and delete [shmclock] shared memory
	discardShm(shmclock_shmid, shmclock_shmptr, "shmclock", prog, "Master");

	//delete semaphore
	if(semid > 0)
	{
		semctl(semid, 0, IPC_RMID);
	}

	//release and delete process block control
	discardShm(pcbt_shmid, pcbt_shmptr, "pcbt", prog, "Master");
}


//create a timer that decrement in real time. Once the timer end, send out SIGALRM.
static void timer(int seconds)
{
	//a timer which is set to zero stops.
	struct itimerval value;
	value.it_value.tv_sec = seconds;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	
	if(setitimer(ITIMER_REAL, &value, NULL) == -1)
	{
		perror("ERROR");
	}
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
	if(debugMode)
	{
		fprintf(stderr, "DEBUG mode is ON\n");
	}
	//main function
	while(1)
	{ 
		int next_fork = (debugMode) ? 100 : rand() % 500000000 + 1000000;
		if(forkclock.nanosecond >= next_fork)
		{
			//reset forkclock
			forkclock.nanosecond = 0;
		
			//checking if bitmap have any available spots
			bool is_bitmap_open = false;
			int count_process = 0;
			while(1)
			{
				last_index = (last_index + 1) % MAX_PROCESS;
				uint32_t bit = bitmap[last_index / 8] & (1 << (last_index % 8));
				if(bit == 0)
				{
					is_bitmap_open = true;
					break;
				}

				if(count_process >= MAX_PROCESS - 1)
				{
					break;
				}
				count_process++;
			}


			if(is_bitmap_open == true)
			{
				pid = fork();

				if(pid == -1)
				{
					fprintf(stderr, "%s: ", prog);
					perror("Error");
					killAllChild();
					cleanUp();
					exit(0);
				}
		
				if(pid == 0) //Child
				{
					//handler for mis-synchronization when timer fire off
					signal(SIGUSR1, exitHandler);

					char exec_index[BUFFER_LENGTH];
					char maxproc[5];
					sprintf(maxproc, "%d", MAX_PROCESS);
					sprintf(exec_index, "%d", last_index);
					int exect_status = execl("./user", "./user", exec_index, maxproc, NULL);
					if(exect_status == -1)
					{	
						fprintf(stderr, "%s: ERROR: execl fail to execute \n", prog);
					}
				
					killAllChild();
					cleanUp();
					exit(EXIT_FAILURE);
				}
				else //parent
				{	
					//set the current index to one bit 
					bitmap[last_index / 8] |= (1 << (last_index % 8));
					
					//initialize user process information to the process control block table
					initPCB(&pcbt_shmptr[last_index], fork_number, last_index, pid);
					
					//increment the total number of fork in execution
					fork_number++;

					//add the process to highest queue
					enQueue(queue, last_index);

					printWrite(fptr, "MASTER: generating process with PID %d and putting it in queue at time %d:%d\n", pcbt_shmptr[last_index].ossIndex, 
							shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				}
			}
		}
	}


	return EXIT_SUCCESS;
}
