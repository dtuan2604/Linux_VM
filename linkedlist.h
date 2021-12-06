#ifndef _LINKEDLIST_H
#define _LINKEDLIST_H


typedef struct NodeL
{ 
	int index;
	int page;
	int frame;
	struct NodeL *next;
}LNode; 


typedef struct
{ 
	LNode *front;
}List;


LNode *newLNode(int index, int page, int frame);
List *createList();

void addListElement(List *l, int index, int page, int frame);
void deleteListFirst(List *l);
int deleteListElement(List *l, int index, int page, int frame);
char *getList(const List *l);

#endif

