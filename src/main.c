#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {

  setbuf(stdout, NULL);
  char *buffer=NULL;
  size_t size=0;

  while (1)
  {

    printf("$ ");
    fflush(stdout);
    ssize_t chars_read=getline(&buffer,&size,stdin);
    buffer[strcspn(buffer,"\n")]='\0';

    char *q_search=buffer;
    int s_flag=0;
    int d_flag=0;

    int saved_stderr=dup(2);
    int saved_stdout=dup(1);

    if(strcmp(buffer,"exit")==0){
    break;
    }
    
    while(*q_search!='\0'){
        int skip_len=0;

        if(*q_search=='\'' && d_flag==0){
            s_flag=!s_flag;
        }

        else if(*q_search=='"' && s_flag==0){
            d_flag=!d_flag;
        }

        else if((*q_search=='>' || (*q_search =='1' && *(q_search+1)=='>') ||(*q_search =='2' && *(q_search+1)=='>')) && s_flag==0 && d_flag == 0 ){

            char *anchor=q_search;
            int tar_fd=1;
            int append=0;

            if(*q_search=='>'){
                if(*(q_search+1)=='>'){
                    append=1;
                    anchor=q_search+2;
                    while(*(anchor)==' '){
                        anchor++;
                    }
                }
                else{
                    anchor=q_search+1;
                    while(*(anchor)==' '){
                        anchor++;
                    }
                }
            }
            
            else if(*q_search=='1'){
                if(*(q_search+2)=='>'){
                    append=1;
                    anchor=q_search+3;
                    while(*(anchor)==' '){
                        anchor++;
                    }
                }
                else{
                    anchor=q_search+2;
                    while(*anchor==' '){
                        anchor++;
                    }
                }
            }

            else if(*q_search=='2'){
                tar_fd=2;
                if(*(q_search+2)=='>'){
                    append=1;
                    anchor=q_search+3;
                    while(*(anchor)==' '){
                        anchor++;
                    }
                }
                else{
                    anchor=q_search+2;
                    while(*anchor==' '){
                        anchor++;
                    }
                }
            }
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);

            *q_search='\0';
            int fd=open(anchor, flags, 0644);
            if(fd!=-1){
                dup2(fd,tar_fd);
                close(fd);
            }
            break;
        }
        *q_search++;
    }

    if(chars_read==-1){
        printf("exit\n");
        exit(0);
    }       

    else if(strncmp(buffer, "echo ", 5) == 0){
        char *quote_find= buffer+5 ;
        int sin_flag=0;
        int dou_flag=0;

        while(*quote_find==' '){
            quote_find++;
        }
        while(*quote_find!='\0'){
            if (*quote_find == '\\' && sin_flag == 0 && dou_flag == 0) {
                quote_find++;
                putchar(*quote_find);
            }

            else if(*quote_find=='\\' && dou_flag==1){

                if(*(quote_find+1)=='"' || *(quote_find+1)=='\\'){
                    quote_find++ ;
                    putchar(*quote_find);
                }

                else{
                    putchar(*quote_find);
                }

            }

            else if (*quote_find == '\'' && dou_flag == 0) {
                sin_flag = !sin_flag;
            }

            else if (*quote_find == '"' && sin_flag == 0) {
                dou_flag = !dou_flag;
            }

            else if (*quote_find == ' ' && sin_flag == 0 && dou_flag == 0) {
                putchar(' ');
                while(*(quote_find + 1) == ' ') {
                    quote_find++;
                }
            }

            else {
                putchar(*quote_find);
            }

            quote_find++;
        }

        putchar('\n');
    }

    else if(strcmp(buffer, "pwd")==0){
        char cwd[1024];

        if(getcwd(cwd,sizeof(cwd))!=NULL){
            printf("%s\n",cwd);
        }

        else{
            printf("ERROR FINDING THE DIRECTORY\n");
        }

    }

    else if(strncmp(buffer,"cd ",3)==0){
        char *target_dir=buffer+3;

        if(strcmp(target_dir,"~")==0){
            char *home_dir=getenv("HOME");

            if(home_dir!=NULL){
                chdir(home_dir);
            }

            else{
                printf("cd: HOME NOT SET\n");
            }    
        }

        else if(chdir(buffer+3)!=0){
            printf("cd: %s: No such file or directory\n", buffer+3);;
        }

    }

  
    else if (strncmp(buffer, "type ", 5) == 0) {
        char *cmd = buffer + 5; 

        if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "type") == 0 || strcmp(cmd, "pwd") == 0 || strcmp(cmd,"cd")==0) {
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
        int argc_count = 0;

        char arg_buffer[1024];
        int arg_len = 0;

        int sin_quote = 0;
        int doub_quote = 0;
        char *quote_find = buffer;

        while (*quote_find == ' ') {
            quote_find++;
        }

        while(*quote_find!='\0'){

            if (*quote_find == '\\' && sin_quote == 0 && doub_quote == 0) {
                quote_find++;                        
                arg_buffer[arg_len] = *quote_find;   
                arg_len++;
            }

            else if(*quote_find=='\\' && doub_quote==1){

                if(*(quote_find+1)=='"' || *(quote_find+1)=='\\'){
                    quote_find++;
                    arg_buffer[arg_len]=*quote_find;
                    arg_len++;
                }

                else{
                    arg_buffer[arg_len]=*quote_find;
                    arg_len++;
                }
            }

            else if (*quote_find == '\'' && doub_quote == 0) {
                sin_quote = !sin_quote;
            }
        
            else if (*quote_find == '"' && sin_quote == 0) {
                doub_quote = !doub_quote;
            }

            else if(*quote_find==' ' && sin_quote==0 && doub_quote==0){
                arg_buffer[arg_len]='\0';
                args[argc_count]=strdup(arg_buffer);
                argc_count++;
                arg_len=0;
                while(*(quote_find+1)==' '){
                    quote_find++;
                }
            }

            else{
                arg_buffer[arg_len]=*quote_find;
                arg_len++;
            }
        quote_find++;
        }

        if(arg_len>0){
            arg_buffer[arg_len]='\0';
            args[argc_count]=strdup(arg_buffer);
            argc_count++ ;
        }
        args[argc_count]=NULL;

        if (args[0] != NULL) {
            pid_t pid = fork();       
            if (pid < 0) {           
                printf("Error: Failed to fork process.\n");
            } 

            else if (pid == 0) {     
                execvp(args[0], args);
                    
                printf("%s: command not found\n", args[0]); 
                exit(1);
            } 
                                
            else if (pid > 0) {       
                wait(NULL);           
            }
        }
    }

    dup2(saved_stdout,1);
    dup2(saved_stderr,2);
    close(saved_stderr);
    close(saved_stdout);
  }

  free(buffer);
  return 0;
}
