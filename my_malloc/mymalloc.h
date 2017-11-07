#ifndef _MYMALLOC_H
#define _MYMALLOC_H
#define MAX_SIZE 8388608
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "arch/atomic.h"
#include "sched/sched.h"
#include "sched/task.h"
#include "util/compiler.h"
#include "util/list.h"
#include "util/rb.h"


typedef enum truth {TRUE, FALSE} BOOLEAN;
//3 different types of metadata (Page, Thread memory, and Virtual Memory)
typedef enum type {PAGE_START,PAGE_END, THREAD_PTR, VM, SHALLOC} TYPE;
typedef struct _node{
    int space;
    BOOLEAN used;
    TYPE type; //Page or pointer for thread
    gthread_task_t* thread;
} Node;
void * mymalloc(size_t size, gthread_task_t* owner);
void myfree(void * data, gthread_task_t* owner);
void* shalloc(size_t size);
void myfreeShalloc(void* p);
void swapPages(Node* source, Node* target);
void initblock();

void printpagemem();
void printshallocmem();
void debug(char* str);
Node* getShallocRegion();

extern char myblock[MAX_SIZE];


#endif //_MYMALLOC_H
