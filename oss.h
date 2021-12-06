#ifndef MY_SHARED_H
#define MY_SHARED_H

#include "config.h"

const key_t key_msg = 4506;
const key_t key_shmlock = 7891;
const key_t key_sem = 6708;
const key_t key_pcb = 3615;


//new type variable
typedef unsigned int uint;


//define shared clock
typedef struct 
{
	unsigned int second;
	unsigned int nanosecond;
}SharedClock;


//define message 
typedef struct
{
	long mtype;
	int index;
	pid_t childPid;
	int flag;	//0 : isDone | 1 : isQueue
	unsigned int address;
	unsigned int requestPage;
	char message[BUFFER_LENGTH];
}Message;


//define page table
typedef struct
{
	uint frameNo;
	uint address: 8;
	uint ref: 1; 	//read or write reference
	uint dirty: 1;  //is set if the page has been modified or written into
	uint valid: 1;  //indicate wheter the page is in memory or not
}PageTable; 

//define process control block
typedef struct
{
	int pidIndex;
	int ossIndex;
	pid_t actualPid;
	PageTable page_table[MAX_PAGE];
}ProcessControlBlock;


#endif

