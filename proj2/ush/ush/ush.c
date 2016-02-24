//
//  main.c
//  ush
//
//  Created by Arjun Augustine on 2/18/16.
//  Copyright © 2016 Arjun Augustine. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "parse.h"


//#define DEBUG_MODE
#ifdef DEBUG_MODE
    #define DETAIL(x) \
        printf(#x": %d\n", x);
    #define D(x) \
        printf(x);
#else
    #define DETAIL(x)
    #define D(x)
#endif

extern char **environ;

int  status = 1;

void checkNextCmd (Cmd C, int pipeFD[2]);

void checkDescriptor (int descriptor) {
    if (descriptor == -1) {
        perror("Unable to Open File");
        _exit(EXIT_FAILURE);
    }
}

void redirectInput (int descriptor) {
    dup2(descriptor, STDIN_FILENO);
    close(descriptor);
}

void redirectOutput (int descriptor, bool errorOut) {
    dup2(descriptor, STDOUT_FILENO);
    if (errorOut) {
        dup2(descriptor, STDERR_FILENO);
    }
    close(descriptor);
}

void openFileForRead (char *infile) {
    int descriptor = 0;
    descriptor = open(infile,(O_RDONLY),(S_IRUSR|S_IRGRP|S_IROTH));
    checkDescriptor(descriptor);
    redirectInput(descriptor);
}

void openFileForWrite (char *outfile, bool errorOut) {
    int descriptor = 0;
    descriptor = open(outfile,(O_CREAT|O_WRONLY|O_TRUNC),(S_IRWXU|S_IRWXG|S_IRWXO));
    checkDescriptor(descriptor);
    redirectOutput(descriptor, errorOut);
}

void openFileForAppend (char *outfile, bool errorOut) {
    int descriptor = 0;
    descriptor = open(outfile,(O_APPEND|O_WRONLY),(S_IRWXU|S_IRWXG|S_IRWXO));
    checkDescriptor(descriptor);
    redirectOutput(descriptor, errorOut);
}

void waitOnPid (pid_t pid) {
    int waitStatus;
    pid_t wpid;
    do {
        wpid = waitpid(pid, &waitStatus, WUNTRACED);
    } while (!WIFEXITED(waitStatus) && !WIFSIGNALED(waitStatus));
}

void createPipe(int pipeFD[2]) {
    pipe(pipeFD);
    if (pipeFD < 0) {
        perror("Could not create pipe");
        _exit(EXIT_FAILURE);
    }
}

void writeToPipe (int pipeFD[2], bool errorOut) {
    close(pipeFD[0]); // Close unused read end
    dup2(pipeFD[1], STDOUT_FILENO);
    if (errorOut) {
        dup2(pipeFD[1], STDERR_FILENO);
    }
    close(pipeFD[1]);
}

int  isBuiltin (char *C) {
    if (!strcmp(C, "cd")) {
        return 1;
    }
    if (!strcmp(C, "echo")) {
        return 2;
    }
    if (!strcmp(C, "logout")) {
        return 3;
    }
    if (!strcmp(C, "nice")) {
        return 4;
    }
    if (!strcmp(C, "pwd")) {
        return 5;
    }
    if (!strcmp(C, "setenv")) {
        return 6;
    }
    if (!strcmp(C, "unsetenv")) {
        return 7;
    }
    if (!strcmp(C, "where")) {
        return 8;
    }
    return 0;
}

void ushExCd(Cmd C){
    int returnVal = 1;
    char *env    = (char*)malloc(100*sizeof(char));
    env = ((C->nargs == 1) ? getenv("HOME") : C->args[1]);
    returnVal = chdir(env);
    if(-1 == returnVal){
        perror("cd");
    }
}

void ushExEcho (Cmd C) {
    char *env    = (char*)malloc(100*sizeof(char));
    if (C->args[1] == NULL) {
        printf("\n");
    } else {
        int i = 1;
        while (C->args[i] != NULL) {
            if (!strncmp((C->args[i]), "$", 1)) {
                env = getenv((C->args[i])+1);
                if (env != NULL) {
                    printf("%s ",env);
                }
            } else {
                printf("%s ",C->args[i]);
            }
            i++;
        }
        printf("\n");
    }
}

void ushExNice (Cmd C) {
    
}

void ushExPwd (Cmd C) {
    char *env    = (char*)malloc(100*sizeof(char));
    env = getcwd(env, 256);
    if (env == NULL) {
        perror("PWD not set");
    } else {
        printf("%s\n", env);
    }
}

void ushExSetenv (Cmd C) {
    char *env    = (char*)malloc(100*sizeof(char));
    char *envVal = (char*)malloc(100*sizeof(char));
    int returnVal;
    if (C->args[1] == NULL) {
        int iter = 0;
        env = environ[0];
        while (env != NULL) {
            envVal = getenv(env);
            if (envVal == NULL) {
                perror("setEnv");
                break;
            }
            printf("%s=%s\n",env, envVal);
            env = environ[++iter];
        }
    } else if (C->args[2] == NULL) {
        returnVal = setenv(C->args[1], "", 1);
        if (returnVal == -1) {
            perror("setenv ReturnVal");
        }
    } else if (C->args[3] == NULL) {
        returnVal = setenv(C->args[1], C->args[2], 1);
        if (returnVal == -1) {
            perror("setenv ReturnVal");
        }
    } else {
        write(STDERR_FILENO, "setenv: Too many arguments.\n", sizeof("setenv: Too many arguments.\n"));
    }
}

void ushExUnsetenv (Cmd C) {
    int returnVal;
    if (C->args[1] == NULL) {
        printf("unsetenv: Too few arguments.\n");
    } else if (C->nargs > 2) {
        printf("unsetenv: Too many arguments.\n");
    } else {
        returnVal = unsetenv(C->args[1]);
        if (returnVal == -1) {
            perror("unsetenv ReturnVal");
        }
    }
}

// All checked except occassional unwanted output.
void ushExWhere (Cmd C) {
    char *env      = (char*)malloc(500*sizeof(char));
    char *pathCopy = (char*)malloc(900*sizeof(char));
    if (C->nargs == 1) {
        write(STDERR_FILENO, "where: Too few arguments.\n", sizeof("where: Too few arguments.\n"));
    }
    int iter = 1;
    char *arg = C->args[iter];
    while (arg != NULL) {
        if (isBuiltin(arg)) {
//            write(STDOUT_FILENO, , <#size_t#>)
            printf("%s: shell built-in command\n",arg);
        }
        strcpy(pathCopy, getenv("PATH"));
        while ((strcpy(env, strsep(&pathCopy, ":")))) {
            strcat(env,"/");
            strcat(env,arg);
            if( access( env, F_OK ) != -1 ) {
                printf("%s\n",env);
                break;
            }
            if (!pathCopy) {
                break;
            }
        }
        strcpy(env, getenv("PWD"));
        strcat(env,"/");
        strcat(env,arg);
        if( access( env, F_OK ) != -1 ) {
            printf("%s\n",env);
        }
        iter++;
        arg = C->args[iter];
    }
}

void ushExBuiltIn (Cmd C, int builtInID) {
    DETAIL(builtInID);
    switch (builtInID) {
        case 0:         // Not BuiltIn Command
            break;
        case 1:         // "cd"
            ushExCd(C);
            break;
        case 2:         // "echo"
            ushExEcho(C);
            break;
        case 3:         // "logout"
            exit(EXIT_SUCCESS);
            break;
        case 4:         // "nice"
            ushExNice(C);
            break;
        case 5:         // "pwd"
            ushExPwd(C);
            break;
        case 6:         // "setenv"
            ushExSetenv(C);
            break;
        case 7:         // "unsetenv"
            ushExUnsetenv(C);
            break;
        case 8:         // "where"
            ushExWhere(C);
            break;
        default:
            printf("inbuiltCmd Switch default Error!");
            break;
    }
}

void ushExCmd (Cmd C, int pipeFD[2]) {
    D("ushExCmd\n");
    if (C == NULL) {
        fprintf(stderr, "Null Command Pointer Passed\n");
        _exit(EXIT_FAILURE);
    }
    if (C->in == Tin) {
        openFileForRead(C->infile);
    }
    switch ( C->out ) {
        case Tnil:
            break;
        case Tout:
            openFileForWrite(C->outfile, false);
            break;
        case Tapp:
            openFileForAppend(C->outfile, false);
            break;
        case ToutErr:
            openFileForWrite(C->outfile, true);
            break;
        case TappErr:
            openFileForAppend(C->outfile, true);
            break;
        case Tpipe:
            writeToPipe(pipeFD, false);
            break;
        case TpipeErr:
            writeToPipe(pipeFD, true);
            break;
        default:
            fprintf(stderr, "Unknown Command Out Token\n");
            _exit(EXIT_FAILURE);
    }
    if (execvp(C->args[0], C->args) == -1) {
        perror("Error");
    }
}

void pipeIn (int pipeInFD[2], Cmd C) {
    D("pipeIn\n");
    pid_t pid;
    close(pipeInFD[1]); // Close unused write end
    int builtIn = isBuiltin(C->args[0]);
    if (builtIn > 0) {
        ushExBuiltIn(C, builtIn);
        return;
    }
    pid = fork();
    if (pid < 0) {
        perror("Fork Error");
    } else if (pid == 0) { // Child Process
        D("pipeInChild\n");
        dup2(pipeInFD[0], STDIN_FILENO);
        close(pipeInFD[0]);
        ushExCmd(C, pipeInFD);
        _exit(EXIT_FAILURE);
    } else {
        D("pipeInParent\n");
        close(pipeInFD[0]);
        waitOnPid(pid);
    }
}

void pipeInOut (int pipeInFD[2], Cmd C) {
    D("pipeInOut\n");
    pid_t pid;
    int pipeOutFD[2];
    createPipe(pipeOutFD);    // Create an Output Pipe
    close(pipeInFD[1]); // Close unused write end of Input Pipe
    pid = fork();
    if (pid < 0) {
        perror("Fork Error");
    } else if (pid == 0) { // Child Process
        D("ushInOutChild\n");
        dup2( pipeInFD[0],  STDIN_FILENO);
        dup2(pipeOutFD[1], STDOUT_FILENO);
        close( pipeInFD[0]);
        close(pipeOutFD[1]);
        ushExCmd(C, pipeInFD);
        _exit(EXIT_FAILURE);
    } else {
        close(pipeInFD[0]);
        D("ushInOutParent\n");
        waitOnPid(pid);
        checkNextCmd(C->next, pipeOutFD);
    }
}

void checkNextCmd (Cmd C, int pipeFD[2]) {
    if (C != NULL) {
        if (C->next != NULL) {
            pipeInOut(pipeFD, C);
        } else {
            pipeIn(pipeFD, C);
        }
    }
}

void ushExStmt (Pipe P) {
    pid_t pid;
    int pipeFD[2];
    Cmd cmd1 = P->head;
    if (cmd1->out != Tpipe) {
        int builtIn = isBuiltin(cmd1->args[0]);
        DETAIL(builtIn);
        if (builtIn > 0) {
            ushExBuiltIn(cmd1, builtIn);
            return;
        }
    } else {
        createPipe(pipeFD);
    }
    pid = fork();
    if (pid < 0) {
        perror("Fork Error");
    } else if (pid == 0) { // Child Process
        D("ushExStmtChild\n");
        ushExCmd(cmd1, pipeFD);
        _exit(EXIT_FAILURE);
    } else { // Parent process
        D("ushExStmtParent\n");
        int waitStatus;
        pid_t wpid;
        do {
            wpid = waitpid(pid, &waitStatus, WUNTRACED);
//            if (-1 == wpid) {
//                perror("negative wpid");
//                _exit(EXIT_FAILURE);
//            }
        } while (!WIFEXITED(waitStatus) && !WIFSIGNALED(waitStatus));
        D("ushExStmtParentWaitOver.\n");
        checkNextCmd(cmd1->next, pipeFD);
    }
}

void ushExLine (Pipe P) {
    while (P != NULL) {
        ushExStmt(P);
        P = P->next;
    }
}

int main(int argc, const char * argv[]) {
    Pipe P;
    char host[256];
    do {
        if(gethostname(host, sizeof(host)) < 0) {
            D("Error: Couldnt get hostname! Aborting...\n");
            exit (-1);
        }
        printf("%s$ ",host);
        P = parse();
        ushExLine(P);
        freePipe(P);
    } while (status);
    return 0;
}
