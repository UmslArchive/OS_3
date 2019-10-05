#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <errno.h>
#include <semaphore.h>

#include "sharedMemoryKeys.h"

//============= Globals ================================
enum FLAGS {
    HELP_FLAG,
    SPAWN_FLAG,
    LOG_FLAG,
    TIME_FLAG,
    TOTAL_FLAGS
};

const int DEBUG = 1;

//======================================================

//Function Prototypes
void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime);
void printIntArray(int* arr, int size);
void processChild(size_t size);

//--------------------------------------------------------

int main(int argc, char* argv[]) {
    //Iterator
    int i;
    
    //Command line argument values
    int maxChildren = 5;
    char* logFileName = "log.txt";
    int terminateTime = 5;

    //Process(noun) variables
    int status;
    pid_t pid = 0;

    //Shared memory variables
    key_t key = MSG_KEY;
    int shmflg;
    int shmid;
    size_t size;
    char* shmPtr = NULL;
    int* shmIntPtr = NULL;
    struct shmid_ds shmid_ds;
    int rtrn;

    //-----

    //Getopts stuff.
    handleArgs(argc, argv, &maxChildren, &logFileName, &terminateTime);

    if(DEBUG) {
        fprintf(stderr, "maxChildren = %d\n", maxChildren);
        fprintf(stderr, "logFileName = %s\n", logFileName);
        fprintf(stderr, "terminateTime = %d\n\n", terminateTime);
        fprintf(stderr, "Parent before fork: %d\n", getpid());
    }

    //Set number of bytes of shared memory to be allocated
    size = sizeof(int) + sizeof(int) + sizeof(int) * maxChildren;
    char sizeString[30];
    sprintf(sizeString, "%d", (int)size); //store size as a string to be passed.

    //---------

    //Spawn a fan of maxChildren # of processes
    for(i = 1; i < maxChildren; i++) {
        pid = fork();

        //Fork error
        if(pid < 0) {
            perror("ERROR:oss:failed to fork");
            exit(1);
        }

        //Child execs from the loop, so no further execution occurs from this file
        //for the child.
        if(pid == 0) {
            execl("./usrPs", "usrPs", sizeString, (char*) NULL);
            exit(55);
        }
            
        
        //Process parent only
        if(pid > 0) {
            if(DEBUG) fprintf(stderr, "onPass:%d, Parent created child process: %d\n", i, pid);
        }
    }

    if(pid > 0) {
        /* 
        Setup shared memory as such: 
            - first sizeof(int) # of bytes stores seconds
            - next sizeof(int) # of bytes stores nanoseconds
            - remaining maxChildren # of bytes stores one-byte for each child
            process' shmMsg which is nothing more than a flag.
        */

        //Create segment
        int ossShmFlags = IPC_CREAT | IPC_EXCL | 0777;
        shmid = shmget(key, size, ossShmFlags);
        if(shmid < 0) {
            perror("ERROR:oss:shmget failed");
            fprintf(stderr, "key:%d, size:%ld, flags:%d\n", key, size, ossShmFlags);
            exit(1);
        }

        //Attach segment
        shmPtr = (char*)shmat(shmid, NULL, 0);
        shmIntPtr = (int*)shmPtr;

        //Test write to shared memory.
        for(i = 0; i < size / sizeof(int); ++i) {
            *shmIntPtr++ = i - 2;
        }
    }
    
    //Wait for each child to exit
    for(i = 1; i < maxChildren; i++) {
        wait(&status);
        if(DEBUG) fprintf(stderr, "%d: exit status: %d\n", i, WEXITSTATUS(status));
    }

    //Remove shared memory segment upon total detachment.
    if(pid > 0) {
        int cmd = IPC_RMID;
        rtrn = shmctl(shmid, cmd, &shmid_ds);
        if(rtrn == -1) {
            perror("ERROR:oss:shmctl failed");
            exit(1);
        }
    }

    return 0;
}

//----------------------------------------------------------------------

//Function definitions.

void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime) {
    int i;
    int flagArray[TOTAL_FLAGS];

    //Initialize the flag array to 0
    for(i = 0; i < TOTAL_FLAGS; i++)
        flagArray[i] = 0;
        
    if(DEBUG) {
        printf("flagArray initialized to:\n");
        printIntArray(flagArray, TOTAL_FLAGS);
    }

    //Parse each argument from the command line
    int arg = -1;
    while((arg = getopt(argc, argv, "hs:l:t:")) != -1) {
        switch(arg) {
            case 'h':
                flagArray[HELP_FLAG] = 1;
                break;
            case 's':
                flagArray[SPAWN_FLAG] = 1;
                *maxChild = atoi(optarg);
                break;
            case 'l':
                flagArray[LOG_FLAG] = 1;
                *logFile = optarg;
                break;
            case 't':
                flagArray[TIME_FLAG] = 1;
                *termTime = atoi(optarg);
                break;
            case '?':
                printf("ERROR:oss:Invalid CLI arguments\nProgram Exited");
                exit(1);
                break;
        }
    }
}

void printIntArray(int* arr, int size) {
    int i;
    for(i = 0; i < size; ++i) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}