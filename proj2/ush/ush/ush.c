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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include "parse.h"
#include "ush.h"

void ushExBuiltIn (Cmd C, int builtInID) {
    DETAIL(builtInID);
    int fildesout, fildesinp, fildeserr;
    saveDescriptors(&fildesout, &fildesinp, &fildeserr);
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
//            writeToPipe(pipeFD, false);
            break;
        case TpipeErr:
//            writeToPipe(pipeFD, true);
            break;
        default:                    // BuiltIn commands are not pipes
            fprintf(stderr, "Unknown Command Out Token\n");
            _exit(EXIT_FAILURE);
    }
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
            fprintf(stderr, "inbuiltCmd Switch default Error!");
            break;
    }
    restoreDescriptors(fildesout, fildesinp, fildeserr);
}

void ushExCmd (Cmd C, int pipeFD[2]) {
    D("ushExCmd\n");
    if (C == NULL) {
        fprintf(stderr, "Null Command Pointer Passed\n");
        _exit(EXIT_FAILURE);
    }
    int builtIn = isBuiltin(C->args[0]);    // Check last command for BuiltIn
    if (builtIn > 0) {
        ushExBuiltIn(C, builtIn);
        return;
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
        perror("Error");    // This part of code never executes if execvp is success
    }
}

void pipeIn (int pipeInFD[2], Cmd C) {
    D("pipeIn\n");
    close(pipeInFD[1]);                     // Close unused write end
    int builtIn = isBuiltin(C->args[0]);    // Check last command for BuiltIn
    if (builtIn > 0) {
        ushExBuiltIn(C, builtIn);
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork Error");
    } else if (pid == 0) {      // Child Process
        D("pipeInChild\n");
        signal(SIGINT, SIG_DFL);
        dup2(pipeInFD[0], STDIN_FILENO);
        close(pipeInFD[0]);
        ushExCmd(C, pipeInFD);
        _exit(EXIT_FAILURE);
    } else {                    // Parent Process
        D("pipeInParent\n");
        close(pipeInFD[0]);
        waitOnPid(pid);         // Wait on child an exit as this is the last cmd
    }
}

void pipeInOut (int pipeInFD[2], Cmd C) {
    D("pipeInOut\n");           // C is pipe in middle of other commands
    int pipeOutFD[2];
    createPipe(pipeOutFD);      // Create an Output Pipe
    close(pipeInFD[1]);         // Close unused write end of Input Pipe
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork Error");
    } else if (pid == 0) {                  // Child Process
        D("ushInOutChild\n");
        signal(SIGINT, SIG_DFL);
        dup2( pipeInFD[0],  STDIN_FILENO);  // Read from Input Pipe
        dup2(pipeOutFD[1], STDOUT_FILENO);  // Write to Output Pipe
        close( pipeInFD[0]);
        close(pipeOutFD[1]);
        ushExCmd(C, pipeOutFD);
        _exit(EXIT_FAILURE);
    } else {                                // Parent Process
        close(pipeInFD[0]);
        D("ushInOutParent\n");
        waitOnPid(pid);
        checkNextCmd(C->next, pipeOutFD);
    }
}

void checkNextCmd (Cmd C, int pipeFD[2]) {  // C is next command to be evaluated
    if (C != NULL) {
        if (C->next != NULL) {              // Then C is a pipe command
            pipeInOut(pipeFD, C);
        } else {                            // Else C is the last one in the statement
            pipeIn(pipeFD, C);
        }
    }
}

void ushExStmt (Pipe P) {
    int pipeFD[2];
    Cmd cmd1 = P->head;                     // First command in pipe
    if (cmd1->next == NULL) {               // If command is not a pipe,
        int builtIn = isBuiltin(cmd1->args[0]);
        if (builtIn > 0) {                  // and is builtIn
            ushExBuiltIn(cmd1, builtIn);    // Execute BuiltIn commands
            return;                         // Return to parse more pipes
        }
    } else {                                // If first command is a pipe
        createPipe(pipeFD);
    }
    pid_t pid = fork();                     // If cmd is not BuiltIn, Fork
    if (pid < 0) {
        perror("Fork Error");
    } else if (pid == 0) {                  // Child Process
        D("ushExStmtChild\n");
        signal(SIGINT, SIG_DFL);
        ushExCmd(cmd1, pipeFD);
        _exit(EXIT_FAILURE);
    } else {                                // Parent process
        D("ushExStmtParent\n");
        waitOnPid(pid);
        checkNextCmd(cmd1->next, pipeFD);
    }
}

void ushExLine (Pipe P) {
    while (P != NULL) {                         // Iterate over all instructions in pipe
        ushExStmt(P);                           // To execute them all one by one.
        P = P->next;
    }
}

void readUshrcIfExists() {
    char * ushrc = (char *)malloc(500*sizeof(char));
    strcpy(ushrc, getenv("HOME"));
    strcat(ushrc, "/.ushrc");
    if(  -1 != access( ushrc, F_OK )) {         // If .ushrc file exists in HOME:
        int stdin_copy = dup(STDIN_FILENO);
        close(STDIN_FILENO);
        errorCheck(open(ushrc,(O_RDONLY),(S_IRUSR|S_IRGRP|S_IROTH)), "File Open");
        do {
            Pipe P = parse();
            if (!strcmp(P->head->args[0], "end")) {
                freePipe(P);
                break;
            }
            ushExLine(P);
            fflush(stdout);
            freePipe(P);
        } while (1);
        close(STDIN_FILENO);
        int returnVal = dup2(stdin_copy,  STDIN_FILENO);
        errorCheck(returnVal, "dup2");
    }
}

int  main(int argc, const char * argv[]) {
    Pipe P;
    char host[256];
    signal(SIGINT , SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    if(gethostname(host, sizeof(host)) < 0) {       // get hostname for prompt
        D("Error: Couldnt get hostname! Aborting...\n");
        exit(EXIT_FAILURE);
    }
    readUshrcIfExists();
    clearerr(stdin);
    do {
        if (isatty(STDIN_FILENO)) {
            printf("%s$ ",host);
            fflush(stdout);            
        }
        if (feof(stdin)) {
            perror("EOF");
        }
        if (fcntl(STDIN_FILENO, F_GETFL) < 0 && errno == EBADF) {
            perror("Bad Descriptor");
        }
        P = parse();
        if (P != NULL) {
            if (!strcmp(P->head->args[0], "end")){
                if (isatty(STDIN_FILENO)) {
                    printf("\n");
                }
                freePipe(P);
                exit(EXIT_SUCCESS);
            }
        }
        ushExLine(P);
        fflush(stdout);
        freePipe(P);
    } while (1);
    return 0;
}
