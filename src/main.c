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
    if(chars_read!=-1){
    buffer[strcspn(buffer,"\n")]='\0';
    printf("%s: command not found\n",buffer);
  }
    else{
      printf("$ exit\n");
      exit(1);
    }
  }
  free(buffer);
  return 0;
}
