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


#define DEBUG_MODE
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

int status = 1;

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

bool isBuiltin (Cmd C) {
    if (strcmp(C->args[0], "cd")) {
        return 1;
    }
    if (strcmp(C->args[0], "echo")) {
        return 2;
    }
    if (strcmp(C->args[0], "logout")) {
        return 3;
    }
    if (strcmp(C->args[0], "nice")) {
        return 4;
    }
    if (strcmp(C->args[0], "pwd")) {
        return 5;
    }
    if (strcmp(C->args[0], "setenv")) {
        return 6;
    }
    if (strcmp(C->args[0], "unsetenv")) {
        return 7;
    }
    if (strcmp(C->args[0], "where")) {
        return 8;
    }
    return 0;
}

void ushExBuiltIn (Cmd C, int builtInID) {
    char *env, *envVal;
    int returnVal;
    switch (builtInID) {
        case 0:         // Not BuiltIn Command
            break;
        case 1:         // "cd"
            if (C->args[1] == NULL) {
                chdir(getenv("HOME"));
            } else if (C->args[2] == NULL) {    // Has only one argument
                chdir(C->args[1]);
            } else {
                write(STDERR_FILENO, "cd: Too many arguments.\n", sizeof("setenv: Too many arguments.\n"));
            }
            break;
        case 2:         // "echo"
            if (C->args[1] == NULL) {
                printf("\n");
            } else {
                int i = 1;
                while (C->args[i] == NULL) {    // Has only one argument
                    if (strncmp((C->args[1]), "$", 1)) {
                        env = getenv((C->args[1]+1));
                        if (env != NULL) {
                            printf("%s ",env);
                        }
                    } else {
                        printf("%s ",C->args[1]);
                    }
                    i++;
                }
                printf("\n");
            }
            break;
        case 3:         // "logout"
            break;
        case 4:         // "nice"
            break;
        case 5:         // "pwd"
            envVal = getenv("PWD");
            break;
        case 6:         // "setenv"
            if (C->args[1] == NULL) {
                env = environ[0];
                while (env != NULL) {
                    envVal = getenv(env);
                    if (envVal == NULL) {
                        perror("setEnv");
                        break;
                    }
                    printf("%s=%s\n",env, envVal);
                    env ++;
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
            break;
        case 7:         // "unsetenv"
            if (C->args[1] == NULL) {
                printf("unsetenv: Too few arguments.\n");
                break;
            } else if (C->nargs > 2) {
                printf("unsetenv: Too many arguments.\n");
                break;
            } else {
                returnVal = unsetenv(C->args[1]);
                if (returnVal == -1) {
                    perror("unsetenv ReturnVal");
                }
            }
            break;
        case 8:         // "where"
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
//    int builtIn = isBuiltin(C);
//    if (builtIn) {
//        ushExBuiltIn(C, builtIn);
//    } else {
        if (execvp(C->args[0], C->args) == -1) {
            perror("Error");
        }
//    }
}

void pipeIn (int pipeInFD[2], Cmd C) {
    D("pipeIn\n");
    pid_t pid;
    close(pipeInFD[1]); // Close unused write end
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
    if (cmd1->out == Tpipe) {
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
