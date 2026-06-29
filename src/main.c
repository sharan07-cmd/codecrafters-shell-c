#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>

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
        if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "type") == 0 || strcmp(cmd, "pwd")) {
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

        else if (strlen(buffer) > 0) { 
            
            char *args[1024];
            int i = 0;
            args[i] = strtok(buffer, " ");
            while (args[i] != NULL) {
                i++;
                args[i] = strtok(NULL, " ");
            }

            if (args[0] != NULL) {
                pid_t pid = fork();       /// The parent code is cloned

                if (pid < 0) {            /// The clone failed
                    printf("Error: Failed to fork process.\n");
                } 
                else if (pid == 0) {      /// The clone is executing the command
                    execvp(args[0], args);
                    
                    // CodeCrafters specific error formatting!
                    printf("%s: command not found\n", args[0]); 
                    exit(1);
                } 
                else if (pid > 0) {       /// You are in the parent code
                    wait(NULL);           /// The parent is put to sleep until the command executes
                }
            }
        }

  }
  free(buffer);
  return 0;
}
