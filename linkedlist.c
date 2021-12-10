#include <stdlib.h>     
#include <stdio.h>      
#include <stdbool.h>    
#include <stdint.h>     
#include <string.h>     
#include <unistd.h>   
#include "helper.h"
#include "linkedlist.h"


//create a new node
LNode *newLNode(int index, int page, int frame)
{ 
    LNode *temp = (LNode *)malloc(sizeof(LNode));
    temp->index = index;
	temp->page = page;
	temp->frame = frame;
    temp->next = NULL;
    return temp;
}
//create a empty linked list
List *createList()
{
	List *l = (List *)malloc(sizeof(List));
	l->front = NULL;
	return l;
}




//add an index, page, and frame to given linked list.
void addListElement(List *l, int index, int page, int frame)
{
	LNode *temp = newLNode(index, page, frame);

	if(l->front == NULL)
	{
		l->front = temp;
		return;
	}
	
	LNode *next = l->front;
	while(next->next != NULL)
	{
		next = next->next;
	}
	next->next = temp;
}



//remove the first element from given linked list.
void deleteListFirst(List *l) 
{
    if(l->front == NULL)
    {
        return;
    }
    
    LNode *temp = l->front;
    l->front = l->front->next;
    free(temp);
}


//remove a specific index, page, and frame from given linked list.
int deleteListElement(List *l, int index, int page, int frame)
{
	LNode *curr = l->front;
    LNode *prev = NULL;
    
    if(curr == NULL)
    {
        return -1;
    }
    
    while(curr->index != index || curr->page != page || curr->frame != frame)
    {
        if(curr->next == NULL)
        {
            return -1;
        }
        else
        {
            prev = curr;
            curr = curr->next;
        }
    }
    
    if(curr == l->front)
    {
		int x = curr->frame;
		free(curr);
        l->front = l->front->next;
		return x;
    }
    else
    {
		int x = prev->next->frame;
		free(prev->next);
        prev->next = curr->next;
		return x;
    }
}

//returns a string representation of the linked list
char *getList(const List *l) 
{
	char buf[4096];
    LNode ptr;
    ptr.next = l->front;
    
    if(ptr.next == NULL) 
    {
        return strduplicate(buf);
    }
    
	sprintf(buf, "Linked List: ");
    while(ptr.next != NULL) 
    {
        sprintf(buf, "%s(%d | %d| %d)", buf, ptr.next->index, ptr.next->page, ptr.next->frame);
        
        ptr.next = (ptr.next->next != NULL) ? ptr.next->next : NULL;
		if(ptr.next != NULL)
		{
			sprintf(buf, "%s, ", buf);
		}
    }
	sprintf(buf, "%s\n", buf);

	return strduplicate(buf);
}

