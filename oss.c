#include <stdlib.h>     
#include <stdio.h>      
#include <stdbool.h>    
#include <stdint.h>     
#include <string.h>     
#include <unistd.h>     
#include <stdarg.h>     
#include <errno.h>      
#include <signal.h>     
#include <sys/ipc.h>    
#include <sys/msg.h>    
#include <sys/shm.h>    
#include <sys/sem.h>    
#include <sys/time.h>   
#include <sys/types.h>  
#include <sys/wait.h>   
#include <time.h>       
#include "config.h"
#include "oss.h"
#include "queue.h"
#include "linkedlist.h"




static FILE *fptr = NULL;
static char *prog;
static char log_file[256] = "output.txt";
static Queue *queue;
static SharedClock forkclock;
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


static int fork_number = 0;
static pid_t pid = -1;




static int memoryaccess_number = 0;
static int pagefault_number = 0;
static unsigned int total_access_time = 0;
static unsigned char main_memory[MAX_FRAME];
static int last_frame = -1;
static List *refString;


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
//increment the logical clock in safe way
static int incShmclock(int increment)
{
	ossSemWait(0);
	int nano = (increment > 0) ? increment : rand() % 1000 + 1;

	forkclock.nanosecond += nano; 
	shmclock_shmptr->nanosecond += nano;

	while(shmclock_shmptr->nanosecond >= 1000000000)
	{
		shmclock_shmptr->second++;
		shmclock_shmptr->nanosecond = abs(1000000000 - shmclock_shmptr->nanosecond);
	}

	ossSemRelease(0);
	return nano;
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

static void printMainMemory()
{
	int i, j, k, found;
	ossSemWait(0);

	printWrite(fptr,"\tCurrent memory layout at time %d:%d\n",shmclock_shmptr->second, shmclock_shmptr->nanosecond);
	printWrite(fptr,"\t\tOccupied\tDirty Bit\n");
	for(i =0; i < MAX_FRAME; i++)
	{
		uint32_t frame = main_memory[i / 8] & (1 << (i % 8));
		found = 0;
		printWrite(fptr,"Frame %3d:\t",i);
		if(frame != 0)
			printWrite(fptr,"Yes\t\t");
		else	
			printWrite(fptr,"No\t\t");
		for(j = 0; j < MAX_PROCESS; j++)
		{
			for(k = 0; k < MAX_PAGE; k++)
			{
				if(pcbt_shmptr[j].page_table[k].frameNo == i)
				{
					printWrite(fptr,"%d\n",pcbt_shmptr[j].page_table[k].dirty);
					found = 1;
					break;
				}
			}
			if(found)
				break;
		}
		if(!found)
			printWrite(fptr,"0\n");
	}

	ossSemRelease(0);
}
//interrupt handling
static void masterHandler(int signum)
{
	fprintf(stderr,"%s: got interrupted. Finishing report and exiting...\n",prog);

	printMainMemory();
	killAllChild();

	double avgMem = (double)memoryaccess_number / (double)shmclock_shmptr->second;
	double avgPagefault = (double)pagefault_number / (double)memoryaccess_number;
	double avgSpeed = (double)total_access_time / (double)memoryaccess_number;
	avgSpeed /= 1000000.0;

	printWrite(fptr,"**************************************************************************\n");
	printWrite(fptr,"\t\tOSS REPORT\n");
	printWrite(fptr, "\tNumber of processes during this execution: %d\n", fork_number);
	printWrite(fptr, "\tNumber of memory accesses: %d\n", memoryaccess_number);
	printWrite(fptr, "\tNumber of page faults: %d\n", pagefault_number);
	printWrite(fptr, "\tNumber of memory accesses per second: %f accesses/sec\n", avgMem);
	printWrite(fptr, "\tNumber of page faults per memory access: %f page faults/access\n", avgPagefault);
	printWrite(fptr, "\tAverage memory access speed: %f ms/access\n", avgSpeed);
	printWrite(fptr,"**************************************************************************\n");

	cleanUp();

	//Final check for closing log file
	if(fptr != NULL)
	{
		fclose(fptr);
		fptr = NULL;
	}

	exit(EXIT_SUCCESS); 
}
static void segHandler(int signum)
{
	fprintf(stderr, "Segmentation Fault\n");
	masterHandler(0);
}
static void exitHandler(int signum)
{
	exit(EXIT_SUCCESS);
}
//interrupt signal set up
static void masterInterrupt(int seconds)
{
	//Invoke timer for termination
	timer(seconds);

	//Signal Handling for: SIGALRM
	struct sigaction sa1;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &masterHandler;
	sa1.sa_flags = SA_RESTART;
	if(sigaction(SIGALRM, &sa1, NULL) == -1)
	{
		perror("ERROR");
	}

	//Signal Handling for: SIGINT
	struct sigaction sa2;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &masterHandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1)
	{
		perror("ERROR");
	}

	//Signal Handling for: SIGUSR1
	signal(SIGUSR1, SIG_IGN);

	//Signal Handling for: SIGSEGV
	signal(SIGSEGV, segHandler);
}

//init process control block table
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

	//init process control block table variable
	initPCBT(pcbt_shmptr);


	//set up queue
	queue = createQueue();
	refString = createList();

	//set up signal handling
	masterInterrupt(TERMINATION_TIME);
}

int main(int argc, char *argv[]) 
{
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

	unsigned char bitmap[MAX_PROCESS];

	initOSS();
	printMainMemory();

	if(debugMode)
	{
		fprintf(stderr, "DEBUG mode is ON\n");
	}


	int last_index = -1;
	
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


		
		incShmclock(0);

		QNode qnext;
		Queue *queueTrack = createQueue();

		qnext.next = queue->front;
		while(qnext.next != NULL)
		{
			incShmclock(0);

			//sending a message to a specific child 
			int c_index = qnext.next->index;
			master_message.mtype = pcbt_shmptr[c_index].actualPid;
			master_message.index = c_index;
			master_message.childPid = pcbt_shmptr[c_index].actualPid;
			int ossID = pcbt_shmptr[master_message.index].ossIndex;

			msgsnd(mqueueid, &master_message, (sizeof(Message) - sizeof(long)), 0);


			msgrcv(mqueueid, &master_message, (sizeof(Message) - sizeof(long)), 1, 0);


			incShmclock(0);

			//if child want to terminate, skips the current iteration of the loop and continues with the next iteration
			if(master_message.flag == 0)
			{
				printWrite(fptr, "MASTER: process with PID %d has finish running at my time %d:%d\n",
					ossID, shmclock_shmptr->second, shmclock_shmptr->nanosecond);

				//return all allocated frame from this process
				int i;
				for(i = 0; i < MAX_PAGE; i++)
				{
					if(pcbt_shmptr[c_index].page_table[i].frameNo != -1)
					{
						int frame = pcbt_shmptr[c_index].page_table[i].frameNo;
						deleteListElement(refString, c_index, i, frame);
						main_memory[frame / 8] &= ~(1 << (frame % 8));
					}
				}
			}
			else
			{

				total_access_time += incShmclock(0);
				enQueue(queueTrack, c_index);

				unsigned int address = master_message.address;
				unsigned int request_page = master_message.requestPage;
				int ossID = pcbt_shmptr[master_message.index].ossIndex;
				if(pcbt_shmptr[c_index].page_table[request_page].ref == 0)
				{
 					printWrite(fptr, "MASTER: process %d requesting read of address %d at time %d:%d\n", ossID, address, 
						shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				}
				else
				{
					printWrite(fptr, "MASTER: process %d requesting write of address %d at time %d:%d\n", ossID, address, 
						shmclock_shmptr->second, shmclock_shmptr->nanosecond);
				}
				memoryaccess_number++;

				//checking valid bit of the current page
				if(pcbt_shmptr[c_index].page_table[request_page].valid == 0)
				{
 					printWrite(fptr, "MASTER: address %d is not in a frame, pagefault\n", address);
					pagefault_number++;


					total_access_time += incShmclock(14000000); //adding time for reading and writing reference

					//checking if main memory has open spot
					bool is_memory_open = false;
					int count_frame = 0;
					while(1)
					{
						last_frame = (last_frame + 1) % MAX_FRAME;
						uint32_t frame = main_memory[last_frame / 8] & (1 << (last_frame % 8));
						if(frame == 0)
						{
							is_memory_open = true;
							break;
						}

						if(count_frame >= MAX_FRAME - 1)
						{
							break;
						}
						count_frame++;
					}


					//continue if there are still space in the main memory
					if(is_memory_open == true)
					{
						//allocate frame to this page and change the valid bit
						pcbt_shmptr[c_index].page_table[request_page].frameNo = last_frame;
						pcbt_shmptr[c_index].page_table[request_page].valid = 1;
					
						//Set the current frame to one 
						main_memory[last_frame / 8] |= (1 << (last_frame % 8));

						addListElement(refString, c_index, request_page, last_frame);
						printWrite(fptr, "MASTER: allocated frame %d to PID %d\n", last_frame, ossID);

					

						//giving data to process or writing data to frame
						if(pcbt_shmptr[c_index].page_table[request_page].ref == 0)
						{
							printWrite(fptr, "MASTER: address %d in frame %d, giving data to process %d at time %d:%d\n",
								address, pcbt_shmptr[c_index].page_table[request_page].frameNo, ossID,
								shmclock_shmptr->second, shmclock_shmptr->nanosecond);

							pcbt_shmptr[c_index].page_table[request_page].dirty = 0;
						}
						else
						{
							printWrite(fptr, "MASTER: address %d in frame %d, writing data to frame at time %d:%d\n",
								address, pcbt_shmptr[c_index].page_table[request_page].frameNo,
								shmclock_shmptr->second, shmclock_shmptr->nanosecond);

							pcbt_shmptr[c_index].page_table[request_page].dirty = 1;
						}
					}
					else
					{
						//memory is already full, now calling fifo replacement
 						printWrite(fptr, "MASTER: address %d is not in a frame, memory is full. Invoking FIFO page replacement...\n", address);

						//FIFO Algorithm
						if(debugMode)
						{
							printWrite(fptr, getList(refString));
						}

						unsigned int fifo_index = refString->front->index;
						unsigned int fifo_page = refString->front->page;
						unsigned int fifo_address = fifo_page << 10;
						unsigned int fifo_frame = refString->front->frame;

						if(pcbt_shmptr[fifo_index].page_table[fifo_page].dirty == 1)
						{
							printWrite(fptr, "MASTER: address %d was modified. Modified information is written back to the disk\n", 
								fifo_address);
						}

						pcbt_shmptr[fifo_index].page_table[fifo_page].frameNo = -1;
						pcbt_shmptr[fifo_index].page_table[fifo_page].dirty = 0;
						pcbt_shmptr[fifo_index].page_table[fifo_page].valid = 0;

						pcbt_shmptr[c_index].page_table[request_page].frameNo = fifo_frame;
						pcbt_shmptr[c_index].page_table[request_page].dirty = 0;
						pcbt_shmptr[c_index].page_table[request_page].valid = 1;

						//update reference string
						deleteListFirst(refString);
						addListElement(refString, c_index, request_page, fifo_frame);

						if(debugMode)
						{
							printWrite(fptr, "After invoking FIFO algorithm...\n");
							printWrite(fptr, getList(refString));
						}


						//modify dirty bit when requesting write of address
						if(pcbt_shmptr[c_index].page_table[request_page].ref == 1)
						{
							printWrite(fptr, "MASTER: dirty bit of frame %d set, adding additional time to the clock\n", last_frame);
							printWrite(fptr, "MASTER: indicating to process %d that writing reference has happend to address %d\n", 
								ossID, address);
							pcbt_shmptr[c_index].page_table[request_page].dirty = 1;
						}
					}
				}
				else
				{

					//giving data to process or writing data to frame
					if(pcbt_shmptr[c_index].page_table[request_page].ref == 0)
					{
						printWrite(fptr, "MASTER: address %d is already in frame %d, giving data to process %d at time %d:%d\n",
							address, pcbt_shmptr[c_index].page_table[request_page].frameNo,
							ossID, shmclock_shmptr->second, shmclock_shmptr->nanosecond);
					}
					else
					{
						printWrite(fptr, "MASTER: address %d is already in frame %d, writing data to frame at time %d:%d\n",
							address, pcbt_shmptr[c_index].page_table[request_page].frameNo,
							shmclock_shmptr->second, shmclock_shmptr->nanosecond);
					}
				}
			}

			//point to the next queue element
			qnext.next = (qnext.next->next != NULL) ? qnext.next->next : NULL;

			//reset master message
			master_message.mtype = -1;
			master_message.index = -1;
			master_message.childPid = -1;
			master_message.flag = -1;
			master_message.requestPage = -1;
		}


		//Reassigned the current queue
		while(!isQueueEmpty(queue))
		{
			deQueue(queue);
		}
		while(!isQueueEmpty(queueTrack))
		{
			int i = queueTrack->front->index;
			enQueue(queue, i);
			deQueue(queueTrack);
		}
		free(queueTrack);
		
		incShmclock(0);

		//check if child has existed with no waiting 
		int child_status = 0;
		pid_t child_pid = waitpid(-1, &child_status, WNOHANG);

		//set the return index bit back to zero
		if(child_pid > 0)
		{
			int return_index = WEXITSTATUS(child_status);
			bitmap[return_index / 8] &= ~(1 << (return_index % 8));
		}

		if(shmclock_shmptr->second - clockCounter >= 1)
		{
			printMainMemory();
			clockCounter = shmclock_shmptr->second;
		}

		//end the infinite loop when reached maximum process
		if(fork_number >= TOTAL_PROCESS)
		{
			timer(0);
			masterHandler(0);
		}
		
	}

	return EXIT_SUCCESS; 
}


