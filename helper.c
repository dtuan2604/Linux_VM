#include <stdlib.h>     
#include <stdio.h>      
#include <stdbool.h>    
#include <stdint.h>     
#include <string.h>     
#include <unistd.h>     
#include "helper.h"

//duplicate a string
char *strduplicate(const char *src) 
{
	size_t len = strlen(src) + 1;       // add one for '\0'
	char *dst = malloc(len);            
	if (dst == NULL) return NULL;       
	memcpy (dst, src, len);             
	return dst;                        
}
