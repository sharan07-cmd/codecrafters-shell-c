#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<sys/types.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);
  char *buffer=NULL;
  size_t size=0;

  while (1)
  {
    printf("$ ");
    fflush(stdout);
    ssize_t chars_read=getline(&buffer,&size,stdin);
    buffer[strcspn(buffer,"\n")]='\0';

    if(strcmp(buffer,"exit")==0){
    break;
  }

    if(chars_read==-1){
        printf("exit\n");
        exit(0);
      }

  else if(strncmp(buffer, "echo ", 5) == 0){
      printf("%s\n",buffer+5);
    }
  
  else if(strncmp(buffer, "type ",5)==0){
    if(strncmp(buffer+5,"echo",4)==0){
      printf("echo is a shell builtin\n");
    }
    else if(strncmp(buffer+5,"exit",4)==0){
      printf("exit is a shell builtin\n");
    }
    else if(strncmp(buffer+5,"type",4)==0){
      printf("type is a shell builtin\n");
    }
    else{
      printf("invalid_command: not found\n");
    }
  }

      else if(chars_read!=-1){
      printf("%s: command not found\n",buffer);
    }

  }
  free(buffer);
  return 0;
}
