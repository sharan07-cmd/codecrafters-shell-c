#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <dirent.h>

typedef struct {
    int id;
    pid_t pid;
    char command[1024]; 
} Job;

Job bg_jobs[100];
int bg_job_count = 0;

static char history[1000][1024];
static int history_count=0;

typedef struct {
    char command[256];
    char script_path[1024];
} CompletionRule;

CompletionRule registry[100];
int registry_size=0;

struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode); 
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); 
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void execute_pipeline_builtin(char **cmd_args) {
    if (strcmp(cmd_args[0], "echo") == 0) {
        for (int i = 1; cmd_args[i] != NULL; i++) {
            printf("%s", cmd_args[i]);
            if (cmd_args[i+1] != NULL) printf(" ");
        }
        printf("\n");
        exit(0);
    } 

    else if (strcmp(cmd_args[0], "type") == 0) {
        if (cmd_args[1] != NULL) {
            char *cmd = cmd_args[1];
            if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "type") == 0 || strcmp(cmd, "pwd") == 0 || strcmp(cmd,"cd")==0 || strcmp(cmd,"complete")==0 || strcmp(cmd,"jobs")==0) {
                printf("%s is a shell builtin\n", cmd);
            }
            else {
                char *path_env = getenv("PATH");
                int found = 0;
                if (path_env != NULL) {
                    char *path_copy = strdup(path_env);
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
                if (found == 0) printf("%s: not found\n", cmd);
            }
        }
        exit(0);

    } 
    else if (strcmp(cmd_args[0], "pwd") == 0) {
        char cwd[1024];
        if(getcwd(cwd,sizeof(cwd))!=NULL) printf("%s\n",cwd);
        exit(0);
    } 
    else if (strcmp(cmd_args[0], "exit") == 0) {
        exit(0);
    }
}

char *script_generator(const char *text2,int state){

    char base_command[1024];
    char *target_script=NULL;

    static char script_matches[100][1024];
    static int script_match_count = 0;

    if(state==0){
        char temp_buffer[1024];
        strcpy(temp_buffer,rl_line_buffer);
        sscanf(temp_buffer,"%s",base_command);

        for(int j=0;j<registry_size;j++){

            if(strcmp(base_command,registry[j].command)==0){
                target_script=registry[j].script_path;
                break;
            }
        }

        if(target_script!=NULL){

            char words[50][1024];
            int word_count=0;

            char line_copy[1024];

            strcpy(line_copy,rl_line_buffer);
            char *token=strtok(line_copy," ");

            while(token!=NULL && word_count<50){
                strcpy(words[word_count],token);
                word_count++ ;
                token=strtok(NULL," ");
            }

            char previous_word[1024]="";

            if(strlen(text2)==0){

                if(word_count>=1){
                    strcpy(previous_word,words[word_count-1]);
                }
            }

            else{

                if(word_count>=2){
                    strcpy(previous_word,words[word_count-2]);
                }   
            }

            char execution_cmd[2048];
            snprintf(execution_cmd, sizeof(execution_cmd), "%s '%s' '%s' '%s'", target_script, base_command, text2, previous_word);

            setenv("COMP_LINE", rl_line_buffer, 1);
            char point_str[32];
            sprintf(point_str, "%d", rl_point);
            setenv("COMP_POINT", point_str, 1);

            FILE *fp=popen(execution_cmd,"r");

            if(fp!=NULL){

                char output[1024];
                script_match_count=0;
                
                while((fgets(output, sizeof(output), fp))!=NULL){
                    output[strcspn(output,"\n")]='\0';

                    if(strlen(output)>0){
                        strcpy(script_matches[script_match_count],output);
                        script_match_count++ ;
                    }
                }
                pclose(fp);
            }
        }   
    }

    if (state < script_match_count) {
        return strdup(script_matches[state]);
    }

    return NULL;
}

char *generator(const char *text1, int state){
    static char *exp_com[]={"echo","exit",NULL};
    static int text_len;
    static int check_builtins=1;
    static int builtin_index=0;
    static char *path_copy=NULL;
    static char *cur_dir=NULL;
    static DIR *dir_stream=NULL;
    static char *saveptr=NULL;

    char *current_word;
    
    if(state==0){

        char *path1=getenv("PATH");
        builtin_index=0;
        check_builtins=1;
        text_len=strlen(text1); 

        if(path_copy!=NULL){
            free(path_copy);
        }

        if(dir_stream!=NULL){
            closedir(dir_stream);
            dir_stream=NULL;
        }

        if(path1!=NULL){
        path_copy=strdup(path1);
        cur_dir=strtok_r(path_copy,":",&saveptr);
        }
        
    }    
        
        if(check_builtins==1){

            while(exp_com[builtin_index]!=NULL){

                current_word=exp_com[builtin_index];
                builtin_index++ ;

                if(strncmp(current_word,text1,text_len)==0){
                    return strdup(current_word);
                }
            }              

            check_builtins=0;
        }

        while(cur_dir!=NULL){

            if(dir_stream==NULL){

                dir_stream=opendir(cur_dir);

                if(dir_stream==NULL){
                    cur_dir=strtok_r(NULL,":",&saveptr);
                    continue;
                }
            }

            struct dirent *entry;

            while((entry=readdir(dir_stream))!=NULL){
                
                if(strncmp(entry->d_name,text1,text_len)==0){

                    char full_path[1024];
                    snprintf(full_path,sizeof(full_path),"%s/%s",cur_dir,entry->d_name);

                    if(access(full_path,X_OK)==0){
                        return strdup(entry->d_name);
                    }
                }
            }

            closedir(dir_stream);
            dir_stream=NULL;
            cur_dir=strtok_r(NULL,":",&saveptr);
        }

    return NULL;
}

char **command_completion(const char *text, int start, int end) {

    if(start==0){
        rl_attempted_completion_over = 1; 
        return rl_completion_matches(text, generator);
    }

    else{
        char base_command[1024];
        char temp_buffer[1024];

        strcpy(temp_buffer,rl_line_buffer);
        sscanf(temp_buffer,"%s",base_command);

        for(int i=0;i<registry_size;i++){

            if(strcmp(base_command,registry[i].command)==0){
                rl_attempted_completion_over=1;
                return rl_completion_matches(text,script_generator);
            }
        }

        rl_attempted_completion_over=0;
        return NULL;    

    }
}

int main(int argc, char *argv[]) {

    enable_raw_mode();
    setbuf(stdout,NULL);
 
  int job_id;  
  setbuf(stdout, NULL);
  char *buffer=NULL;
  size_t size=0;

  rl_attempted_completion_function = command_completion;
  while (1)
  
  {
    for (int i = 0; i < bg_job_count; i++) {
            char marker = ' '; 
            
            if (i == bg_job_count - 1) {
                marker = '+'; 
            } 
            
            else if (i == bg_job_count - 2) {
                marker = '-'; 
            }

            int status;
            int result=waitpid(bg_jobs[i].pid,&status,WNOHANG);

            if(result>0){
                int length=0;
                length=strlen(bg_jobs[i].command);

                if(bg_jobs[i].command[length-1]=='&'){
                    bg_jobs[i].command[length-1]='\0';

                    if(bg_jobs[i].command[length-2]==' '){
                        bg_jobs[i].command[length-2]='\0';
                    }
                }

                printf("[%d]%c %-24s%s\n", bg_jobs[i].id, marker, "Done", bg_jobs[i].command);

                for(int j=i; j<bg_job_count-1;j++){
                    bg_jobs[j]=bg_jobs[j+1];
                }

                bg_job_count--;
                i--;
            }
        }

   printf("$ ");
   fflush(stdout);

   char buffer[1024];
   
   int history_idx = history_count; 
   int buf_len = 0;
   buffer[0] = '\0';
   while (1) {
    char c;
    read(STDIN_FILENO, &c, 1); 

    if (c == '\n') {
        write(STDOUT_FILENO, "\n", 1);
        buffer[buf_len] = '\0';
        break; 
    } 
    
    else if (c == 127) { 
        
        if (buf_len > 0) {
            buf_len--;
            write(STDOUT_FILENO, "\b \b", 3);
        }
    } 
    
    else if (c == '\033') { 
    
        char seq[2];
        read(STDIN_FILENO, &seq[0], 1);
        read(STDIN_FILENO, &seq[1], 1);
                
        if (seq[0] == '[' && seq[1] == 'A') {
            
            if (history_idx > 0) {
                history_idx--;
                     
                write(STDOUT_FILENO, "\033[2K\r$ ", 7);       
                strcpy(buffer, history[history_idx]);
                buf_len = strlen(buffer);
                        
                write(STDOUT_FILENO, buffer, buf_len);
            }
        }
    } 

    else { 
        buffer[buf_len] = c;
        buf_len++;
        write(STDOUT_FILENO, &c, 1);
    }

 }
        
    strcpy(history[history_count],buffer);
    history_count++;

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

    
    if(strncmp(buffer, "echo ", 5) == 0 && strchr(buffer,'|')==NULL){
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

    else if(strncmp(buffer,"complete ",9)==0){

        char flags[10],args1[1024],args2[1024];
        int variables_filled;
        
        variables_filled=sscanf(buffer,"complete %s %s %s", flags,args1,args2);

        if(strcmp(flags,"-p")==0){
            int found=0;

            for(int i=0; i<registry_size;i++){

                if(strcmp(args1,registry[i].command)==0){
                    printf("complete -C '%s' %s\n",registry[i].script_path,registry[i].command);
                    found=1;
                    break;
                }
            }

            if(found==0){
                printf("complete: %s: no completion specification\n",args1);
            }
        }

        else if(strcmp(flags,"-C")==0){

            if(variables_filled==3){

                strcpy(registry[registry_size].script_path,args1);
                strcpy(registry[registry_size].command,args2);
                registry_size++ ;
            }
        }
        
        else if(strcmp(flags,"-r")==0){
            int found_index = -1;

        
            for (int i = 0; i < registry_size; i++) {

                if (strcmp(registry[i].command, args1) == 0) {
                    found_index = i;
                    break;
                }
            }

        
            if (found_index != -1) {

                for (int i = found_index; i < registry_size - 1; i++) {
                    registry[i] = registry[i + 1]; 
                }
            
            registry_size--;
            }
        }
    }

  
    else if (strncmp(buffer, "type ", 5) == 0 && strchr(buffer,'|')==NULL) {
        char *cmd = buffer + 5; 

        if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "type") == 0 || strcmp(cmd, "pwd") == 0 || strcmp(cmd,"cd")==0 || strcmp(cmd,"complete")==0 || strcmp(cmd,"jobs")==0 || strcmp(cmd,"history")==0) {
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

    else if(strncmp(buffer,"history",7)==0){
        int n=history_count;

        if(buffer[7]==' '){
            n=atoi(&buffer[8]);
        }

        int start_index= history_count-n;
        if(start_index<0){
            start_index=0;
        }

        for(int i=start_index;i<history_count;i++){
            printf("%5d  %s\n", i + 1, history[i]);
        }
        continue;
    }

    else if(strncmp(buffer,"jobs",4)==0){
        for (int i = 0; i < bg_job_count; i++) {
            char marker = ' '; 
            
            if (i == bg_job_count - 1) {
                marker = '+'; 
            } 
            
            else if (i == bg_job_count - 2) {
                marker = '-'; 
            }

            int status;
            int result=waitpid(bg_jobs[i].pid,&status,WNOHANG);
            
            if(result==0){
                printf("[%d]%c %-24s%s\n", bg_jobs[i].id, marker, "Running", bg_jobs[i].command);
            }

            else if(result>0){
                int length=0;
                length=strlen(bg_jobs[i].command);

                if(bg_jobs[i].command[length-1]=='&'){
                    bg_jobs[i].command[length-1]='\0';

                    if(bg_jobs[i].command[length-2]==' '){
                        bg_jobs[i].command[length-2]='\0';
                    }
                }

                printf("[%d]%c %-24s%s\n", bg_jobs[i].id, marker, "Done", bg_jobs[i].command);

                for(int j=i; j<bg_job_count-1;j++){
                    bg_jobs[j]=bg_jobs[j+1];
                }

                bg_job_count--;
                i--;
            }
        }
        continue;
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
        int is_pipeline=0;
        char **cmds[100];
        int cmd_count=0;

        cmds[0]=&args[0];
        cmd_count=1;

        for(int u=0;u<argc_count;u++){
            if(strcmp(args[u],"|")==0){
                args[u]=NULL;

                cmds[cmd_count] = &args[u+1];
                cmd_count++;
                is_pipeline=1;
            }
        }

        int is_background = 0;

        if (argc_count > 0 && strcmp(args[argc_count - 1], "&") == 0) {
            is_background = 1;
            args[argc_count - 1] = NULL; 
        }

        

        if (args[0] != NULL) {

            if(is_pipeline){
                int prev_fd=0;
                for(int i=0;i<cmd_count;i++){
                    int fd[2];
                    if(i<cmd_count-1){
                        if(pipe(fd)==-1){

                        }
                    }

                    pid_t pid=fork();
                    if(pid==0){
                        if(prev_fd!=0){
                            dup2(prev_fd,0);
                            close(prev_fd);
                        }

                        if(i<cmd_count-1){
                            dup2(fd[1],1);
                            close(fd[0]);
                            close(fd[1]);
                        }

                        execute_pipeline_builtin(cmds[i]);
                        execvp(cmds[i][0], cmds[i]);
                        printf("ERROR: COMMAND NOT FOUND\n");
                        exit(1);
                    }

                    else if(pid>0){
                        if(prev_fd!=0){
                            close(prev_fd);
                        }

                        if(i<cmd_count-1){
                            prev_fd=fd[0];
                            close(fd[1]);
                        }
                    }
                }

                for(int i=0;i<cmd_count;i++){
                    wait(NULL);
                }
            }

            else{

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

                    if(is_background){

                        if(bg_job_count==0){
                            job_id=1;
                        }
                    
                        else{
                            int max_id=0;

                            for(int k=0; k<bg_job_count;k++){

                                if (bg_jobs[k].id> max_id){
                                    max_id=bg_jobs[k].id;
                                }
                            }

                            job_id=max_id+1;
                        }

                        printf("[%d] %d\n", job_id, pid);
                        bg_jobs[bg_job_count].id = job_id;
                        bg_jobs[bg_job_count].pid = pid;
                        strcpy(bg_jobs[bg_job_count].command, buffer); 
            
                        bg_job_count++; 
                    
                    }

                    else{
                        waitpid(pid,NULL,0);
                    }
                }
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
