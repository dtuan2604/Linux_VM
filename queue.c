#include <stdlib.h>     
#include <stdio.h>      
#include <stdbool.h>    
#include <stdint.h>     
#include <string.h>     
#include <unistd.h>     
#include "helper.h"
#include "queue.h"

//create a empty queue
Queue *createQueue()
{
	Queue *q = (Queue *)malloc(sizeof(Queue));
	q->front = NULL;
	q->rear = NULL;
	q->count = 0;
	return q;
}


//create a new linked list node
QNode *newQNode(int index)
{ 
    QNode *temp = (QNode *)malloc(sizeof(QNode));
    temp->index = index;
    temp->next = NULL;
    return temp;
} 


//add an index onto given queue
void enQueue(Queue *q, int index) 
{ 
	QNode *temp = newQNode(index);

	//increase queue count
	q->count = q->count + 1;

	//if queue is empty, then new node is front and rear both
	if(q->rear == NULL)
	{
		q->front = q->rear = temp;
		return;
	}

	//add the new node at the end of queue and change rear 
	q->rear->next = temp;
	q->rear = temp;
}


//remove an index from given queue
QNode *deQueue(Queue *q) 
{
	//If queue is empty, return NULL
	if(q->front == NULL) 
	{
		return NULL;
	}

	QNode *temp = q->front;
	free(temp);
	q->front = q->front->next;

	//If front becomes NULL, then change rear also as NULL
	if(q->front == NULL)
	{
		q->rear = NULL;
	}

	//Decrease queue count
	q->count = q->count - 1;
	return temp;
} 


//checking if the queue is empty 
bool isQueueEmpty(Queue *q)
{
	if(q->rear == NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}


//get string representation for queue
char *getQueue(const Queue *q)
{
	char buf[4096];
	QNode next;
	next.next = q->front;

	sprintf(buf, "Queue: ");
	while(next.next != NULL)
	{
		sprintf(buf, "%s%d", buf, next.next->index);
		
		next.next = (next.next->next != NULL) ? next.next->next : NULL;
		if(next.next != NULL)
		{
			sprintf(buf, "%s, ", buf);
		}
	}
	sprintf(buf, "%s\n", buf);

	return strduplicate(buf);
}

