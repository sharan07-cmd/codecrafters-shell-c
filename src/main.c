#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>

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
  
  else if (strncmp(buffer, "type ", 5) == 0) {
        char *cmd = buffer + 5; 
        if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "type") == 0) {
            printf("%s is a shell builtin\n", cmd);
        } 
       
        else {
            char *path_env = getenv("PATH");
            int found = 0;

            if (path_env != NULL) {
                
                char *path_copy = malloc(strlen(path_env) + 1);
                strcpy(path_copy, path_env);
                char *dir = strtok(path_copy, ":");

                while (dir != NULL) {
                    char file_path[1024];
                    snprintf(file_path, sizeof(file_path), "%s/%s", dir, cmd);

                    if (access(file_path, X_OK) == 0) {
                        printf("%s is %s\n", cmd, file_path);
                        found = 1;
                        break; 
                    }
                    
                    dir = strtok(NULL, ":"); 
                }
                free(path_copy);
            }

            if (found == 0) {
                printf("%s: not found\n", cmd);
            }
        }
    }

      else if(chars_read!=-1){
      printf("%s: command not found\n",buffer);
    }

  }
  free(buffer);
  return 0;
}
