#include <stdlib.h>     
#include <stdio.h>           
#include <string.h>    
#include <stdbool.h>   
#include <unistd.h>         
#include <errno.h>      
#include <signal.h>     
#include <sys/ipc.h>    
#include <sys/msg.h>    
#include <sys/shm.h>        
#include <sys/wait.h>   
#include "config.h"
#include "oss.h"


static char *prog;
static int progID;



static int mqueueid = -1;
static Message user_message;

static int shmclock_shmid = -1;
static SharedClock *shmclock_shmptr = NULL;

static int pcbt_shmid = -1;
static ProcessControlBlock *pcbt_shmptr = NULL;

static int MAX_PROCESS;



//detach any shared memory.
static void discardShm(void *shmaddr, char *prog)
{
	if(shmaddr != NULL)
	{
		if((shmdt(shmaddr)) << 0)
		{
			fprintf(stderr, "%s: ERROR: could not detach shared memory\n", prog);
		}
	}
}
//release all shared memory.
static void cleanUp()
{
	discardShm(shmclock_shmptr, prog);
	discardShm(pcbt_shmptr, prog);
}
static void processHandler(int signum)
{
	cleanUp();
	exit(2);
}
//Interrupt process when caught SIGTERM. Release all resources. 
static void childInterrupt()
{
	struct sigaction sa1;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &processHandler;
	sa1.sa_flags = SA_RESTART;
	if(sigaction(SIGUSR1, &sa1, NULL) == -1)
	{
		perror("ERROR");
	}

	struct sigaction sa2;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &processHandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1)
	{
		perror("ERROR");
	}
}
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
	if (argc != 3){
		fprintf(stderr, "%s: Missing arguments\n",prog);
		exit(2);
	}

	childInterrupt();


	prog = argv[0];
	progID = atoi(argv[1]);
	MAX_PROCESS = atoi(argv[2]);
	srand(getpid());

	getSharedMemory();

	bool isTerm = false;
	int refTimes = 0;
	unsigned int address = 0;
	unsigned int request_page = 0;
	while(1)
	{
		//waiting for master signal to get resources
		msgrcv(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), getpid(), 0);

		if(refTimes <= 1000)
		{
			int randpage = rand() % 32;
			address = randpage * 1024 + rand() % 1024;
			request_page = address >> 10;
			refTimes++;
		}
		else
		{
			isTerm = true;
		}
		
			
		//Send a message back to master
		user_message.mtype = 1;
		user_message.flag = (isTerm) ? 0 : 1;
		user_message.address = address;
		user_message.requestPage = request_page;
		msgsnd(mqueueid, &user_message, (sizeof(Message) - sizeof(long)), 0);


		if(isTerm)
		{
			break;
		}
	}

	cleanUp();
	exit(progID);
}