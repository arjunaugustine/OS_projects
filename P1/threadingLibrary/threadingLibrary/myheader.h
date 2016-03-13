//
//  myheader.h
//  threadingLibrary
//
//  Created by Arjun Augustine on 2/1/16.
//  Copyright © 2016 Arjun Augustine. All rights reserved.
//

#ifndef myheader_h
#define myheader_h

#include <stdio.h>
#include <stdlib.h>
#define  _XOPEN_SOURCE
#include <ucontext.h>
#include <stdbool.h>
#include "mythread.h"
#define MEM 8192
#define DEBUG_Mode

#ifdef DEBUG_Mode
    #define PRINT(x) \
        printf(#x": %d\t", x);
    #define OUTS(x) \
        printf(x);
#else
    #define PRINT(x)
    #define OUTS(x)
#endif

int threadCount = 0, eraseCount = 0, maxThreadCount = 2700000;

struct context{
    ucontext_t *context;
    int data;
    bool joinAll;
    bool joinSelf;
    bool completed;
    struct context *next;
    struct context *prev;
    struct context *parent;
    struct context *childHead;
    struct context *childTail;
    struct context *nextSibling;
    struct context *prevSibling;
};
typedef struct context *threadPtr;

struct semaphore{
    int value;
    threadPtr head;
    threadPtr tail;
};
typedef struct semaphore *semaphorePtr;

void printThread (threadPtr t) {
        PRINT(threadCount);
        PRINT(eraseCount);
        PRINT(t->data);
    if (t->parent!=NULL) {
        PRINT(t->parent->data);
    } else {
        OUTS("t->parent->data: -1\t");
    }
    if (t->nextSibling!=NULL) {
        PRINT(t->nextSibling->data);
    } else {
        OUTS("t->nextSibling->data: -1\t");
    }
    if (t->childHead!=NULL) {
        PRINT(t->childHead->data);
    } else {
        OUTS("t->childHead->data: -1     ");
    }
    if (t->next!=NULL) {
        PRINT(t->next->data);
    } else {
        OUTS("t->next->data: -1    ");
    }
//    if (t->prev!=NULL) {
//        PRINT(t->prev->data);
//    } else {
//        OUTS("t->prev->data: -1\t");
//    }
#ifdef DEBUG_Mode
    printf("\n");
#endif
}

void printTree (threadPtr root) {
    printThread(root);
    threadPtr child = (root->childHead);
    while (child) {
        threadPtr grandchild = child;
        printTree(grandchild);
        child = child->nextSibling;
    }
    return;
}

void eraseThread (threadPtr t) {
    OUTS("eraseThread\n"); printThread(t);
    eraseCount++;
    free(t->context);
    free(t);
    return;
}

int retireThread (threadPtr t) {
    // Cannot retire threads unless all its children threads are retired
    //    if (t->childHead != NULL) {
    //        printf("NOT NOT NOT Retired!!!\n");
    //        return -1;
    //    }
    OUTS("retireThread\n"); printThread(t);
    if (t->parent!=NULL) {
        if (t == t->parent->childHead) {
            // If thread is an only child, then make tail point to NULL
            if (t->nextSibling == NULL) {
                t->parent->childHead = NULL;
                t->parent->childTail = NULL;
            } else {
                // If thread is first child, then make nextSibling the first child
                t->parent->childHead = t->nextSibling;
                t->nextSibling->prevSibling = NULL;
            }
        } else if (t == t->parent->childTail) {
            // If thread is last child, then make prevSibling the last child
            t->parent->childTail = t->prevSibling;
            t->prevSibling->nextSibling = NULL;
        } else {
            // else make the previousSibling point to the nextSibling
            t->prevSibling->nextSibling = t->nextSibling;
            t->nextSibling->prevSibling = t->prevSibling;
        }
    }
    return 0;
}

void eraseTree (threadPtr rootThread) {
    OUTS("EraseTree\n"); printThread(rootThread);
    while (rootThread->childHead) {
        eraseTree(rootThread->childHead);
    }
    eraseThread(rootThread);
}

void addThreadToParent (threadPtr t, threadPtr parent) {
    // If parent has no children, add thread to childhead of parent
    if (parent->childHead == NULL) {
        parent->childHead = t;
        parent->childTail = t;
        t->prevSibling = NULL;
        t->nextSibling = NULL;
    } else { // else add thread to nextSibling of current last child
        parent->childTail->nextSibling = t;
        t->prevSibling = parent->childTail;
        t->nextSibling = NULL;
        parent->childTail = t;
    }
}

void pushToQueue (threadPtr *t, threadPtr *queueHead, threadPtr *queueTail) {
    if (*t == NULL) {
        printf("Error: Tried to add NULL pointer to Queue! Aborting...\n");
        exit (0);
    }
    if (*queueHead == NULL) {   // Initial condition where ready queue is empty
        (*t)->next = NULL;
        (*t)->prev = NULL;
        *queueHead = (*t);
        *queueTail = (*t);
    } else {                   //
        (*t)->next = NULL;
        (*t)->prev = *queueTail;
        (*queueTail)->next = (*t);   // Add thread to end of queue
        (*queueTail) = (*t);         // Increment tail pointer.
    }
    OUTS("pushToQueue\n"); printThread(*t);
}

threadPtr popFromQueue (threadPtr *t, threadPtr *queueHead, threadPtr *queueTail) {
    OUTS("popFromQueue\n"); printThread(*t);
    threadPtr returnThread = *t, nextThread = (*t)->next, prevThread = (*t)->prev;
    if (*t == NULL) {
        printf("Error: Tried to pop NULL thread pointer! Aborting...\n");
        exit (0);
    } else if ((*queueHead) == NULL || (*queueTail) == NULL) {
        printf("Error: Tried to pop from empty Queue! Aborting...\n");
        exit (0);
    } else if ((*queueHead == *queueTail) && ((*t != *queueHead) /*|| ((*t)->next != NULL) || ((*t)->prev != NULL)*/) ) {
        printf("Error: Thread not found in queue! Aborting...\n");
        exit (0);
    } else if (*queueHead == *queueTail) {    // If thread is only element in queue
        *queueHead = NULL;
        *queueTail = NULL;
    } else if (*queueHead == *t) {            // If thread is first element in queue
        *queueHead = (*t)->next;
        (*queueHead)->prev = NULL;
    } else if (*queueTail == *t) {            // If thread is last element in queue
        *queueTail = (*t)->prev;
        (*queueTail)->next = NULL;
    } else {                                // If thread is in middle of queue
        (prevThread)->next = (*t)->next;
        (nextThread)->prev = (*t)->prev;
    }
    returnThread->next = NULL;
    returnThread->prev = NULL;
    return returnThread;
}

bool isOnlyChild (threadPtr child) {
    if (child->parent == NULL) {
        printf("Error: Parent pointer is NULL! Aborting...\n");
        exit(0);
    } else {
        OUTS("isOnlyChild\n"); printThread(child->parent);
    }
    if (child->parent->childHead == child && child->parent->childTail == child) {
        if (child->nextSibling != NULL || child->prevSibling != NULL) {
            printf("Error: Only Child has a sibling exception! Aborting...\n");
            exit(0);
        } else {
            return true;
        }
    }
    return false;
}

bool findImmChild (threadPtr child, threadPtr parent) {
    threadPtr childIter = parent->childHead;
    for (; childIter != NULL; childIter = childIter->nextSibling) {
        if (childIter == child) {
            return true;
        }
    }
    return false;
}

void initThread (threadPtr t, int threadCount, threadPtr currentThread) {
    t->data = threadCount;
    t->joinAll = 0;
    t->joinSelf = 0;
    t->completed = 0;
    t->parent = currentThread;
    t->childHead = NULL;
    t->childTail = NULL;
    t->nextSibling = NULL;
    t->prevSibling = NULL;
}

threadPtr returnRoot (threadPtr t, threadPtr root) {
    OUTS("returnRoot\n"); printThread(t);
    if (t == root || t->parent == root) {
        return (t);
    }
    while (isOnlyChild(t) && t->parent->completed) {
        t = t->parent;
    }
    return (t);
}

void checkMalloc (void * ptr) {
    if (ptr == NULL) {
        printf("Error: Out of memory! Aborting...\n");
        exit(0);
    }
    return;
}

#endif /* myheader_h */
