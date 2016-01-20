//
//  context.c
//  xcode
//
//  Created by Arjun Augustine on 1/17/16.
//  Copyright © 2016 Arjun Augustine. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
//#define  _XOPEN_SOURCE
//#include <ucontext.h>

struct context{
    int data;
    bool joinAll;
    bool joinSelf;
    struct context *parent;
    struct context *childHead;
    struct context *nextSibling;
    struct context *nextReadyThread;
};
typedef struct context thread;

ucontext_t newContext;
thread *rootThread = NULL, *currentThread = NULL;
thread *joinQueueHead = NULL, *joinQueueTail = NULL;
thread *readyQueueHead = NULL, *readyQueueTail = NULL;
int threadCount = 0;

void printThread (thread *t) {
    printf("%d\tParent: %d\t Next Sibling: %d\tChild Head: %d\n",t->data, (t->parent==NULL)?-1:t->parent->data, (t->nextSibling==NULL)?-1:t->nextSibling->data, (t->childHead==NULL)?-1:t->childHead->data);
}

void printTree (thread *root) {
    printThread(root);
    thread* child = (root->childHead);
    while (child) {
        thread *grandchild = child;
        printTree(grandchild);
        child = child->nextSibling;
    }
    return;
}

void eraseThread (thread *t) {
    printf("Erase:\n");
    printThread(t);
    free(t);
    printf("Erased!!!\n");
    return;
}

int retireThread (thread* t) {
    // Cannot retire threads unless all its children threads are retired
    if (t->childHead != NULL) {
        return -1;
    }
    if (t->parent!=NULL) {
    // If thread is first child, then make nextSibling the first child
        if (t == t->parent->childHead) {
            t->parent->childHead = t->nextSibling;
        } else {
            // else make the previousSibling point to the nextSibling
            thread *prevSibling = t->parent->childHead;
            while (prevSibling) {
                if (prevSibling->nextSibling == t) {
                    prevSibling->nextSibling = t->nextSibling;
                    break;
                }
                prevSibling = prevSibling->nextSibling;
            }
        }
    }
    eraseThread(t);
    return 0;
}

void retireTree (thread* rootThread) {
    while (rootThread->childHead) {
        retireTree(rootThread->childHead);
    }
    retireThread(rootThread);
}

void addThreadToParent (thread *t, thread *parent) {
    // If parent has no children, add thread to childhead of parent
    if (parent->childHead == NULL) {
        parent->childHead = t;
    } else { // else add thread to nextSibling of current last child
        thread *childTail = parent->childHead;
        while (childTail->nextSibling != NULL) {
            childTail = childTail->nextSibling;
        }
        childTail->nextSibling = t;
    }
}

void initThread (thread *t) {
    t->joinAll = 0;
    t->joinSelf = 0;
    t->parent = currentThread;
    t->childHead = NULL;
    t->nextSibling = NULL;
    t->data = threadCount;
    t->nextReadyThread = NULL;
    if (readyQueueHead == NULL) {   // Initial condition where ready queue is empty
        readyQueueHead = t;
        readyQueueTail = t;
    } else {                        //
        readyQueueTail->nextReadyThread = t;    // Add thread to end of queue
        readyQueueTail = t;                     // Increment tail pointer.
    }
}

void MyThreadInit (void/*(*start_funct)(void *), void *args */) {
    printf("Start\n");
    rootThread = MyThreadCreate();
}

thread* MyThreadCreate () {
    printf("Create\n");
    thread *t = (thread*) malloc(sizeof(thread));
    if (t == NULL) {
        printf("Error: Out of memory! Aborting... \n");
        exit(0);
    }
    threadCount++;
    initThread (t);
    addThreadToParent (t, currentThread);
    printThread(t);
    return t;
}

int main (void) {
    MyThreadInit();
    currentThread = MyThreadCreate(); // current = t2
    printf("T1 create\n");
    thread* someThread = MyThreadCreate();
    someThread = MyThreadCreate(); // someThread = t4
    MyThreadCreate();
    currentThread = someThread; // t4
    currentThread = MyThreadCreate(); // current = t6.
    printTree(rootThread);
    printf("PrintComplete\n");
    retireTree(someThread); // t6
    printTree(rootThread);
    printf("PrintComplete\n");
    retireTree(rootThread);
    printf("Completed\n");
    return 0;
}
