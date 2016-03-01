//
//  ush.h
//  ush
//
//  Created by Arjun Augustine on 2/25/16.
//  Copyright © 2016 Arjun Augustine. All rights reserved.
//

#include <sys/resource.h>
#include <sys/wait.h>

#ifndef ush_h
#define ush_h

//#define DEBUG_MODE    // Include a -DDEBUG_MODE flag in Makefile to compile in debug mode.
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
char * oldpwd;

void checkNextCmd (Cmd C, int pipeFD[2]);

void errorCheck (int returnVal, char *err) {
    if (returnVal == -1) {
        perror(err);
        _exit(EXIT_FAILURE);
    }
}

void redirectInput (int descriptor) {
    int returnVal = dup2(descriptor, STDIN_FILENO);
    errorCheck(returnVal, "dup2");
    returnVal = close(descriptor);
    errorCheck(returnVal, "File Close");
}

void redirectOutput (int descriptor, bool errorOut) {
    int returnVal = dup2(descriptor, STDOUT_FILENO);
    errorCheck(returnVal, "dup2");
    if (errorOut) {
        returnVal = dup2(descriptor, STDERR_FILENO);
        errorCheck(returnVal, "dup2");
    }
    returnVal = close(descriptor);
    errorCheck(returnVal, "File Close");
}

void openFileForRead (char *infile) {
    int descriptor = 0;
    descriptor = open(infile,(O_RDONLY),(S_IRUSR|S_IRGRP|S_IROTH));
    errorCheck(descriptor, "File Open");
    redirectInput(descriptor);
}

void openFileForWrite (char *outfile, bool errorOut) {
    int descriptor = 0;
    descriptor = open(outfile,(O_CREAT|O_WRONLY|O_TRUNC),(S_IRWXU|S_IRWXG|S_IRWXO));
    errorCheck(descriptor, "File Open");
    redirectOutput(descriptor, errorOut);
}

void openFileForAppend (char *outfile, bool errorOut) {
    int descriptor = 0;
    descriptor = open(outfile,(O_APPEND|O_WRONLY),(S_IRWXU|S_IRWXG|S_IRWXO));
    errorCheck(descriptor, "File Open");
    redirectOutput(descriptor, errorOut);
}

void restoreDescriptors (int fildesout, int fildesinp, int fildeserr) {
    int returnVal = dup2(fildesinp,  STDIN_FILENO);         // Restore the descriptors
    errorCheck(returnVal, "dup2");
    returnVal = dup2(fildesout, STDOUT_FILENO);             // to previous values.
    errorCheck(returnVal, "dup2");
    returnVal = dup2(fildeserr, STDERR_FILENO);
    errorCheck(returnVal, "dup2");
}

void saveDescriptors (int *fildesout, int *fildesinp, int *fildeserr) {
    *fildesout = dup(STDOUT_FILENO);            // Duplicate the current STDOUT,
    *fildesinp = dup( STDIN_FILENO);            // restore them after indirection
    *fildeserr = dup(STDERR_FILENO);            // STDERR, STDIN descriptors to
}

void waitOnPid (pid_t pid) {
    int waitStatus;
    do {
        pid_t wpid = waitpid(pid, &waitStatus, WUNTRACED);
        if (-1 == wpid) {
//                            perror("negative wpid");
//                            _exit(EXIT_FAILURE);
        }
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
    char *env = strdup((C->nargs == 1)    ? getenv("HOME")   :
              (!strcmp (C->args[1], "-")) ? oldpwd : C->args[1]);
    if (env == NULL) {
        if (getenv("HOME") == NULL) {
            env = "cd: HOME not set\n";
        } else {
            env = "cd: OLDPWD not set\n";
        }
        fprintf(stdout, "%s", env);
        return;
    }
    oldpwd = getcwd(oldpwd, 256);
    int returnVal = chdir(env);
    if(-1 == returnVal){
        perror("cd");
    }
    free(env);
}

void ushExEcho (Cmd C) {
    if (C->args[1] == NULL) {
        fprintf(stdout, "\n");
    } else {
        int i = 1;
        while (C->args[i] != NULL) {
            if (!strncmp((C->args[i]), "$", 1)) {
                char *env = getenv((C->args[i])+1);
                if (env != NULL) {
                    fprintf(stdout, "%s ",env);
                }
            } else {
                fprintf(stdout, "%s ",C->args[i]);
            }
            i++;
        }
        fprintf(stdout, "\n");
    }
}

bool isADigit (char c) {
    if (c == '+' || c == '-' || (c >= '0' && c <= '9') ) {
        return true;
    }
    return false;
}

void ushExNice (Cmd C) {
    pid_t pid = getpid();
    if (C->nargs == 1) {
        if (-1 == setpriority(PRIO_PROCESS, pid, 4)) {
            perror("setpriority");
        }
    } else if (C->nargs == 2 && isADigit(*(C->args[1]))) {
        if (-1 == setpriority(PRIO_PROCESS, pid, atoi(C->args[1]))) {
            perror("setpriority");
        }
    } else if (C->nargs >= 3 || !isADigit(*(C->args[1])) ) {
        // Implement
//        int builtIn = isBuiltin(C->args[2]);
//        if (builtIn > 0) {                  // and is builtIn
//            ushExBuiltIn(cmd1, builtIn);    // Execute BuiltIn commands
//            return;                         // Return to parse more pipes
//        }
        pid_t pid = fork();                     // If cmd is not BuiltIn, Fork
        if (pid < 0) {
            perror("Fork Error");
        } else if (pid == 0) {                  // Child Process
            D("ushExStmtChild\n");
            int pos = C->nargs >= 3 ? 2 : 1;
            if (execvp(C->args[pos], &(C->args[pos])) == -1) {
                perror("Error");    // This part of code never executes if execvp is success
            }
            _exit(EXIT_FAILURE);
        } else {                                // Parent process
            D("ushExStmtParent\n");
            waitOnPid(pid);
        }

    } else {
        fprintf(stderr, "nice: Too many arguments.\n");
    }
}

void ushExPwd (Cmd C) {
    char *env    = (char*)malloc(500*sizeof(char));
    env = getcwd(env, 256);
    if (env == NULL) {
        perror("PWD not set");
    } else {
        fprintf(stdout, "%s\n", env);
    }
    free(env);
}

void ushExSetenv (Cmd C) {
    int returnVal = 0;
    if (C->nargs == 1) { // Print all lines in environ
        char **env = environ;
        while (*env != NULL) {
            fprintf(stdout, "%s\n",*env);
            env++;
        }
    } else if (C->nargs == 2) { // Set environ variable to null string
        returnVal = setenv(C->args[1], "", 1);
    } else if (C->nargs == 3) { // Set environ variable to specified arg
        returnVal = setenv(C->args[1], C->args[2], 1);
    } else {                    // Too many arguments
        fprintf(stderr, "setenv: Too many arguments.\n");
    }
    if (returnVal == -1) {      // In case the setenv-s fail.
        perror("setenv");
    }
}

void ushExUnsetenv (Cmd C) {
    if (C->nargs < 2) {         // Too few  arguments
        fprintf(stdout, "unsetenv: Too few  arguments.\n");
    } else if (C->nargs > 2) {  // Too many arguments
        fprintf(stdout, "unsetenv: Too many arguments.\n");
    } else {                    // Go ahead and unset
        if (-1 == unsetenv(C->args[1])) {
            perror("unsetenv");
        }
    }
}

void ushExWhere (Cmd C) {
    // All checked except occassional unwanted output.
    // Last check in CWD/PWD???
    char *env;
    oldpwd = (char*)malloc(256*sizeof(char));
    if (C->nargs == 1) {
        fprintf(stderr, "where: Too few arguments.\n");
    }
    int iter = 1;
    char *arg = C->args[iter];
    while (arg != NULL) {   // Iterate all arguments of where command
        if (isBuiltin(arg)) {
            fprintf(stdout, "%s: shell built-in command\n",arg);
        }
        char *pathCopy = strdup(getenv("PATH"));
        while ((env = strdup(strsep(&pathCopy, ":")) )) { // Choose absolute paths in $Path
            strcat(env,"/");                            // One by one and concatenate
            strcat(env,arg);                            // Them with /<arg>
            if( access( env, F_OK ) != -1 ) {           // If this file exists, print
                fprintf(stdout, "%s\n",env);
                break;
            }
            if (!pathCopy) {
                break;
            }
        }
        strcpy(env, getenv("PWD"));                     // Check in current directory also
        strcat(env,"/");
        strcat(env,arg);
        if( access( env, F_OK ) != -1 ) {
            fprintf(stdout, "%s\n",env);
        }
        arg = C->args[++iter];
        free(pathCopy);
        free(env);
    }
}

#endif /* ush_h */
