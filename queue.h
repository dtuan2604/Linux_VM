#ifndef _QUEUE_H
#define _QUEUE_H


typedef struct NodeQ
{ 
	int index;
	struct NodeQ *next;
}QNode; 


typedef struct
{ 
	QNode *front;
	QNode *rear;
	int count;
}Queue; 


Queue *createQueue();
QNode *newQNode(int index);
void enQueue(Queue* q, int index);
QNode *deQueue(Queue *q);
char *getQueue(const Queue *q);

#endif

