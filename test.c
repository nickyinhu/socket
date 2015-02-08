#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <pthread.h>

int main (int argc, char *argv[]){
	char str[80] = "Get OK 11";
    char s[2] = " ";
   	char *token[2];
   
   /* get the first token */
   /* walk through other tokens */
   	strtok(str, s);
   	int i = 0;
   	while( i < 2 ) 
   {

      
    
      token[i] = strtok(NULL, s);
      printf( " %s\n", token[i] );
      i++;
   }

   
  	return(0);
}