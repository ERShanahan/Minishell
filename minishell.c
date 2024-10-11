#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"
#define SIZE 1024
#define EXIT "exit\n"
#define HOME "HOME"

volatile sig_atomic_t interrupt = 0;

void interrupt_handler(int signal){
    interrupt = 1;
    printf("\n");
}

void prompt(){
    char* cwd;
    if((cwd = getcwd(NULL, 0))){
        printf("%s[%s]", BLUE, cwd);
        printf("%s> ", DEFAULT);
    }else{
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
    }
    free(cwd);
}

int break_command(char** buf, char* command){

    char* arg;
    arg = strtok(command, " \t\n");

    int count = 0;
    while( arg != NULL){
        buf[count] = arg;
        arg = strtok(NULL, " \t\n");
        count++;
    }

    return count;
}

void pre_built_cd(char** args, int argc){
    if(argc > 2){
        fprintf(stderr, "Error: Too many arguments to cd.\n");
    }else if(argc < 2){
        char* curr_usr = getenv(HOME);
        chdir(curr_usr);
    }else if(args[1][0] == '~'){
        char* curr_usr = getenv(HOME);
        char path[SIZE];
        sprintf(path, "%s%s", curr_usr, &args[1][1]);
        chdir(path);
    }else if(chdir(args[1])){
        fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", args[1], strerror(errno));
    }
}

void pre_built_pwd(char** args, int argc){
    char* cwd;
    if((cwd = getcwd(NULL, 0))){
        printf("%s\n", cwd);
    }else{
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
    }
    free(cwd);
}

void pre_built_lf(char** args, int argc){

    char* cwd = getcwd(NULL, 0);

    DIR* dir;
    dir = opendir(".");
    if(dir == NULL){
        fprintf(stderr, "Cannot open %s\n", cwd);
        exit(EXIT_FAILURE);
    }

    struct dirent* dir_entry;
    struct stat fileinfo;

    while( (dir_entry = readdir(dir)) != NULL){
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0){
            continue;
        }
        printf("%s\n", dir_entry->d_name);
    }

    closedir(dir);
    free(cwd);
}

void pre_built_lp(char** args, int argc){
    char* cwd = getcwd(NULL, SIZE);

    chdir("/proc");

    DIR* dir;
    dir = opendir("/proc");
    if(dir == NULL){
        fprintf(stderr, "Cannot open /proc\n");
        exit(EXIT_FAILURE);
    }

    struct dirent* dir_entry;
    struct stat fileinfo;

    while( (dir_entry = readdir(dir)) != NULL){
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0){
            continue;
        }

        if(isdigit(dir_entry->d_name[0])){
            char* proc_num = dir_entry->d_name;
            chdir(dir_entry->d_name);

            FILE* fstream;
            char* line = NULL;
            size_t len = 0;
            struct passwd* user;
            struct stat stats;

            if( (fstream = fopen("cmdline", "r")) == NULL){
                fprintf(stderr, "Error in opening file.\n");
                break;
            }

            getline(&line, &len, fstream);

            if(stat("cmdline", &stats) == -1){
                fprintf(stderr, "Error: could not get stat of cmdline.\n");
                continue;
            }

            user = getpwuid(stats.st_uid);
            if(user == NULL){
                fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
                continue;
            }

            printf("%5s %s %s\n", proc_num, user->pw_name, line);

            fclose(fstream);
            chdir("..");
            free(line);
        }
    }

    closedir(dir);
    chdir(cwd);
    free(cwd);
}

int ex_pre_cmd(char** arguments, int argc){

    if(arguments[0] == NULL){
        return 0;
    }

    if(strcmp(arguments[0], "exit") == 0){
        
        exit(EXIT_SUCCESS);

    }else if(strcmp(arguments[0], "cd") == 0){

        pre_built_cd(arguments, argc);
        return 0;

    }else if(strcmp(arguments[0], "pwd") == 0){

        pre_built_pwd(arguments, argc);
        return 0;

    }else if(strcmp(arguments[0], "lf") == 0){
        
        pre_built_lf(arguments, argc);
        return 0;

    }else if(strcmp(arguments[0], "lp") == 0){

        pre_built_lp(arguments, argc);
        return 0;
    }

    return 1;
}

int execute_command(char** arguments){

    int child;
    int stat;
    pid_t this_pid;

    if ( (child = fork()) < 0){
        perror("fork");
        return 1;
    }

    else if ( child == 0 ){

        execvp(arguments[0], arguments);
        fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
        exit(EXIT_SUCCESS);

    }else{
        while(1){
            this_pid = waitpid(child, &stat, 0);

            if(this_pid == -1){
                if(errno == EINTR){
                    continue;
                }else{
                    perror("waitpid");
                    exit(EXIT_FAILURE);
                }
            }else{
                break;
            }
        }
	}
}

int main(int argc, char* argv[]){
    
    struct sigaction signal = {0};
    signal.sa_handler = interrupt_handler;

    sigemptyset(&signal.sa_mask);

    if (sigaction(SIGINT, &signal, NULL) == -1) {
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
        return 0;
    }

    char command[SIZE];
    char* arguments[SIZE];

    while(1){

        prompt();

        if(fgets(command, SIZE, stdin) == NULL){
            if(interrupt){
                interrupt = 0;
                continue;
            }else{
                fprintf(stderr, "Error: Failed to read from stdin. %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

        int argc = break_command(arguments, command);

        if(ex_pre_cmd(arguments, argc)){
            execute_command(arguments);
        }

        for(int i = 0; i < SIZE; i++){
            arguments[i] = NULL;
        }
    }

    return 0;
}
