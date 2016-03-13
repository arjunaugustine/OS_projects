//
//  mythread.c
//  threadingLibrary
//
//  Created by Arjun Augustine on 2/1/16.
//  Copyright © 2016 Arjun Augustine. All rights reserved.
//

#include "myheader.h"

ucontext_t *rootContext;
threadPtr rootThread = NULL, currentThread = NULL;
threadPtr joinQueueHead = NULL, joinQueueTail = NULL;
threadPtr readyQueueHead = NULL, readyQueueTail = NULL;

void MyThreadExit (void) {
    OUTS("MyThreadExit\n"); printThread(currentThread);
    threadPtr t = currentThread;
    // Check join queue for parent waiting on self
    if (t->parent == NULL && t != rootThread) {
        printf("Error: Exitting thread has no parent! Aborting...\n");
        exit(0);
    }
    if (t->joinSelf || (t != rootThread && t->parent->joinAll && isOnlyChild(t)) ) {
        popFromQueue(&(t->parent), &joinQueueHead,  &joinQueueTail);
        pushToQueue(&(t->parent), &readyQueueHead, &readyQueueTail);
    }
    if (t->childHead) { // If it has a child, mark thread as complete and end process.
        t->completed = true;
        retireThread(t);    //
    } else {            // else Check if its only child and its parent is marked complete.
        threadPtr temp = t;
        t = returnRoot(t, rootThread);
        retireThread(temp);    // Must come after returnRoot else parent looses info on this thread.
        eraseTree(t);   // and erase the whole tree that was waiting on this child.
    }
    if (readyQueueHead != NULL) {
        currentThread = popFromQueue(&readyQueueHead, &readyQueueHead, &readyQueueTail);
        setcontext(currentThread->context);
    } else {
        if (joinQueueHead != NULL) {
            printf("Error: Ready queue empty before join queue! Aborting...\n");
            exit(0);
        } else {
            setcontext(rootContext);
        }
    }
}

void MyThreadJoinAll (void) {
    OUTS("MyThreadJoinAll\n"); printThread(currentThread);
    if (readyQueueHead == NULL) {
        printf("Info: Ready Queue is empty. Abort Join process...\n");
        return;
    }
    if (currentThread->childHead == NULL) {
        printf("Info: Thread has no child threads. Abort Join process...\n");
        return;
    }
    currentThread->joinAll = true;
    ucontext_t *tempContext = currentThread->context;
    pushToQueue(&currentThread, &joinQueueHead, &joinQueueTail);
    currentThread = popFromQueue(&readyQueueHead, &readyQueueHead, &readyQueueTail);
    swapcontext(tempContext, currentThread->context);
    return;
}

int MyThreadJoin (MyThread thread) {
    OUTS("MyThreadJoin\n"); printThread(currentThread);
    threadPtr t = thread;
    if (thread == NULL) {
        printf("Info: Join called on a NULL thread. Aborting Join process...\n");
        return (-1);
    }
    else if (readyQueueHead == NULL) {
        printf("Info: Ready Queue is empty. Aborting Join process...\n");
        return (-1);
    }
    if (!findImmChild(t, currentThread)) {
        printf("Info: Thread either finished execution or its not an immediate child...\n");
        return (-1);
    }
    // thread is an immediate child of currentThread, add currentThread to joinQueue.
    // and yield to next thread in readyQueue;
    t->joinSelf = true;
    ucontext_t *tempContext = currentThread->context;
    pushToQueue(&currentThread, &joinQueueHead, &joinQueueTail);
    currentThread = popFromQueue(&readyQueueHead, &readyQueueHead, &readyQueueTail);
    swapcontext(tempContext, currentThread->context);
    return 0;
}

void MyThreadYield (void) {
    ucontext_t *tempContext = currentThread->context;
    pushToQueue(&currentThread, &readyQueueHead, &readyQueueTail);
    currentThread = popFromQueue(&readyQueueHead, &readyQueueHead, &readyQueueTail);
    swapcontext(tempContext, currentThread->context);
}

MyThread MyThreadCreate (void(*start_funct)(void *), void *args) {
    if (threadCount == maxThreadCount) {
        printf("Error: Maximum memory allocation exceeded! Aborting...\n");
        exit(0);
    }
    threadPtr t = (threadPtr) malloc(sizeof(struct context));
    checkMalloc((void *)t);
    ucontext_t *contextPtr = (ucontext_t*) malloc(sizeof(ucontext_t));
    checkMalloc((void *)contextPtr);
    getcontext(contextPtr);
    contextPtr->uc_link = 0;
    contextPtr->uc_stack.ss_size=MEM;
    contextPtr->uc_stack.ss_flags=0;
    contextPtr->uc_stack.ss_sp=malloc(MEM);
    checkMalloc((void *)contextPtr->uc_stack.ss_sp);
    makecontext(contextPtr, (void (*) (void))start_funct, 1, args);
    threadCount++;
    initThread (t, threadCount, currentThread);
    t->context = contextPtr;
    if (currentThread != NULL) { // Add all threads except root thread to their parents.
        addThreadToParent (t, currentThread);
    }
    OUTS("MyThreadCreate\n");    printThread(t);
    pushToQueue(&t, &readyQueueHead, &readyQueueTail);
    return (MyThread) t;
}

void MyThreadInit (void(*start_funct)(void *), void *args) {
    rootContext= (ucontext_t *) malloc (sizeof(ucontext_t));
    checkMalloc((void *)rootContext);
//    if (rootContext == NULL) {
//        printf("Error: Out of memory! Aborting...\n");
//        exit(0);
//    }
    rootThread = MyThreadCreate(start_funct, args);
    currentThread = popFromQueue(&readyQueueHead, &readyQueueHead, &readyQueueTail);
    if (swapcontext(rootContext, rootThread->context) == -1) {
        printf("Error: Swap from MyThreadInit Failed! Aborting...\n");
        exit(0);
    }
}

int MySemaphoreDestroy(MySemaphore sem) {
    OUTS("MySemaphoreDestroy\n");    printThread(currentThread);
    semaphorePtr s = (semaphorePtr) sem;
    if (s->head != NULL) {
        return -1;
    } else {
        free(s);
        return 0;
    }
}

void MySemaphoreWait(MySemaphore sem) {
    semaphorePtr s = (semaphorePtr) sem;
    OUTS("MySemaphoreWait\n");    printThread(currentThread);
    if (s == NULL) {
        printf("Info: Wait on a NULL semaphore called! Returning...\n");
        return;
    }
    s->value --;
    PRINT(s->value); PRINT(s->head);
    if (s->value < 0) {
        ucontext_t *tempContext = currentThread->context;
        pushToQueue(&currentThread, &s->head, &s->tail);
        currentThread = popFromQueue(&readyQueueHead, &readyQueueHead, &readyQueueTail);
        swapcontext(tempContext, currentThread->context);
    }
}

void MySemaphoreSignal(MySemaphore sem) {
    semaphorePtr s = (semaphorePtr) sem;
    OUTS("MySemaphoreSignal\n");    printThread(currentThread);
    threadPtr t;
    s->value ++;
    PRINT(s->value); PRINT(s->head);
    if (/*s->value < 0 && */s->head != NULL) {
        t = popFromQueue(&s->head, &s->head, &s->tail);
        pushToQueue(&t, &readyQueueHead, &readyQueueTail);
    }
}

MySemaphore MySemaphoreInit(int initialValue) {
    OUTS("MySemaphoreInit\n");
    //printThread(currentThread);
    if (initialValue < 0) {
        printf("Error: Init value of semaphore is negative! Aborting...\n");
        return  NULL;
    }
    semaphorePtr s = (semaphorePtr) malloc(sizeof(struct semaphore));
    checkMalloc((void *)s);
//    if (s == NULL) {
//        printf("Error: Out of memory! Aborting...\n");
//        exit(0);
//    }
    s->value = initialValue;
    s->head  = NULL;
    s->tail  = NULL;
    return (MySemaphore) s;
}
