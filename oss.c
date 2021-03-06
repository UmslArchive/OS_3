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
#include <signal.h>

#include "sharedMemoryKeys.h"

//============= Globals ================================
enum FLAGS {
    HELP_FLAG,
    SPAWN_FLAG,
    LOG_FLAG,
    TIME_FLAG,
    TOTAL_FLAGS
};

const int DEBUG = 0;

//Shared memory IDs
int shmSemID = 0;
int shmMsgID = 0;
int shmClockID = 0;

//Shared memory control structs.
struct shmid_ds shmSemCtl;
struct shmid_ds shmMsgCtl;
struct shmid_ds shmClockCtl;

int terminateTime = 5;

//dynamic array to hold all child process IDs
int* pidArray;
int pidArraySize = 0;

//==========SIGNAL HANDLERS==============================

void interruptSignalHandler(int sig);
void alarmSignalHandler(int sig);

//======================================================

//Function Prototypes
void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime);
void printIntArray(int* arr, int size);
sem_t* createShmSemaphore(key_t* key, size_t* size, int* shmid);
int* createShmMsg(key_t* key, size_t* size, int* shmid);
int* createShmLogicalClock(key_t* key, size_t* size, int* shmid);
void cleanupSharedMemory(int* shmid, struct shmid_ds* ctl);

//--------------------------------------------------------

int main(int argc, char* argv[]) {
    //Iterator
    int i;
    int totalProcesses = 0;
    int totalProcessesWaitedOn = 0;
    
    //Command line argument values
    int maxChildren = 5;
    char* logFileName = "log.txt";

    //Process variables
    int status;
    pid_t pid = 0;

    //Shared memory keys
    key_t shmSemKey = SEM_KEY;
    key_t shmMsgKey = MSG_KEY;
    key_t shmClockKey = CLOCK_KEY;

    //Shared memory sizes
    size_t shmSemSize = sizeof(sem_t);
    size_t shmMsgSize = 2 * sizeof(int);
    size_t shmClockSize = 2 * sizeof(int);

    //Shared memory pointers
    sem_t* semPtr = NULL;
    int* shmMsgPtr = NULL;
    int* shmClockPtr = NULL;

    //File handler
    FILE* logFileHandler = NULL;

    //-----------------------------------------------

    //Getopts stuff.
    handleArgs(argc, argv, &maxChildren, &logFileName, &terminateTime);
    totalProcesses = maxChildren - 1;

    //Truncate log file
    logFileHandler = fopen(logFileName, "w+");
    fclose(logFileHandler);

    if(DEBUG) {
        fprintf(stderr, "maxChildren = %d\n", maxChildren);
        fprintf(stderr, "logFileName = %s\n", logFileName);
        fprintf(stderr, "terminateTime = %d\n\n", terminateTime);
        fprintf(stderr, "Parent before fork: %d\n", getpid());
        fprintf(stderr, "sem_t size: %ld\n", sizeof(sem_t));
    }

    //---------

    //Setup shared memory w/ semaphore
    semPtr = createShmSemaphore(&shmSemKey, &shmSemSize, &shmSemID);
    shmMsgPtr = createShmMsg(&shmMsgKey, &shmMsgSize, &shmMsgID);
    shmClockPtr = createShmLogicalClock(&shmClockKey, &shmClockSize, &shmClockID);

    //Register signal handlers
    signal(SIGINT, interruptSignalHandler);
    signal(SIGALRM, alarmSignalHandler);

    //Spawn a fan of maxChildren # of processes
    for(i = 1; i < maxChildren; i++) {
        pid = fork();
        
        //Store the child pids in the array
        if(pid > 0) {
            pidArraySize++;
            pidArray = realloc(pidArray, pidArraySize);
            pidArray[pidArraySize - 1] = pid;
        }

        //Fork error
        if(pid < 0) {
            perror("ERROR:oss:failed to fork");
            exit(1);
        }

        //Child execs from the loop, so no further execution occurs from this
        //file for the child.
        if(pid == 0) {
            execl("./usrPs", "usrPs", (char*) NULL);
            exit(55);
        } 
        
        if(DEBUG) fprintf(stderr, "onPass:%d, Parent created child process: %d\n", i, pid);
    }
    
    //Set the alarm
    alarm(terminateTime);
   
    //Wait for each child to exit
    int exitedPID;
    while(1) {
        //sleep(1);

        sem_wait(semPtr); //lock

        //fprintf(stderr, "Start OSS\n");

        //Increment the clock
        int* tempClockPtr = shmClockPtr;
        *tempClockPtr += 1;                         //<--tick rate
        if(*tempClockPtr >= 1000000000) {
            *(tempClockPtr + 1) += 1;
            *tempClockPtr = 0;
        }

        //Check MSG status.
        int isMessage = 0;
        int* tempMsgPtr = shmMsgPtr;
        //fprintf(stderr, "OSS: msgNano = %d, msgSec = %d\n",*tempMsgPtr,*(tempMsgPtr + 1) );
        if(*tempMsgPtr != 0 || *(tempMsgPtr + 1) != 0) {
            fprintf(stderr, "OSS found a message\n");
            isMessage = 1;
        }

        //fprintf(stderr, "OSSisMessage: %d\n", isMessage);
        if(isMessage) {
            exitedPID = wait(&status);
            
            //Find the process id in the index and save the index.
            int deadProcessIndex = -1;
            for(i = 0; i < pidArraySize; ++i) {
                if(exitedPID == pidArray[i]) {
                    deadProcessIndex = i;
                    pidArray[i] = 0;
                    break;
                }
            }

            //Write to log.
            logFileHandler = fopen(logFileName, "a");
            fprintf(logFileHandler,
                "Master: Child %d is terminating at my time %ds %dns because it reached %ds %dns in child process\n",
                exitedPID, *(shmClockPtr + 1), *shmClockPtr, *(shmMsgPtr + 1), *shmMsgPtr);
            fclose(logFileHandler);

            totalProcessesWaitedOn++;

            //Reset msg
            *tempMsgPtr = 0;
            *(tempMsgPtr + 1) = 0;

            if(DEBUG) fprintf(stderr, "Child %d -- exit status: %d\n\n", exitedPID, WEXITSTATUS(status));
            
            //Spawn replacement processes
            int newProcessID;
            if(totalProcesses < 100) {
                totalProcesses++;
                newProcessID = fork();
                if(newProcessID == 0) {
                    execl("./usrPs", "usrPs", (char*) NULL);       
                }

                //Replace the dead processid with the new pid in the array
                pidArray[deadProcessIndex] = newProcessID;
            }
        }

        //Break the loop when all processes have been waited on.
        if(!isMessage && totalProcesses >= 100 && totalProcessesWaitedOn == totalProcesses) {
            sem_post(semPtr);
            break;
        }

        sem_post(semPtr); //unlock
    }

    //Remove shared memory segments upon total detachment.
    if(pid > 0) {
        cleanupSharedMemory(&shmMsgID, &shmMsgCtl);
        cleanupSharedMemory(&shmClockID, &shmClockCtl);
        cleanupSharedMemory(&shmSemID, &shmSemCtl);
    }

    free(pidArray);

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

    if(flagArray[HELP_FLAG]) {
        printf("-- OSS HELP --\n\t-s x [Set max child processes that can run simultaneously]\n\t-l x [Set log file name]\n\t-t x [Sex max runtime(seconds)]\n\n");
        exit(15);
    }
}

void printIntArray(int* arr, int size) {
    int i;
    for(i = 0; i < size; ++i) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

sem_t* createShmSemaphore(key_t* key, size_t* size, int* shmid) {
    int ossShmFlags = IPC_CREAT  | IPC_EXCL  | 0777;

    //Allocate shared memory and get id
    *shmid = shmget(*key, *size, ossShmFlags);
    if(*shmid < 0) {
        perror("ERROR:oss:shmget failed(semaphore)");
        fprintf(stderr, "key:%d, size:%ld, flags:%d\n", *key, *size, ossShmFlags);
        exit(1);
    }

    //Set the pointer
    sem_t* temp = (sem_t*)shmat(*shmid, NULL, 0);
    if(temp == (sem_t*) -1) {
        perror("ERROR:oss:shmat failed(semaphore)");
        exit(1);
    }

    //Initialize semaphore
    if(sem_init(temp, 1, 1) == -1) {
        perror("ERROR:oss:sem_init failed");
        exit(1);
    }

    return temp;
}

int* createShmMsg(key_t* key, size_t* size, int* shmid) {
    int ossShmFlags = IPC_CREAT  | IPC_EXCL  | 0777;

    //Allocate shared memory and get id.
    *shmid = shmget(*key, *size, ossShmFlags);
    if(*shmid < 0) {
        perror("ERROR:oss:shmget failed(msg)");
        fprintf(stderr, "key:%d, size:%ld, flags:%d\n", *key, *size, ossShmFlags);
        exit(1);
    }

    //Set the pointer
    int* temp = (int*)shmat(*shmid, NULL, 0);
    if(temp == (int*) -1) {
        perror("ERROR:oss:shmat failed(msg)");
        exit(1);
    }

    return temp;
}

int* createShmLogicalClock(key_t* key, size_t* size, int* shmid) {
    int ossShmFlags = IPC_CREAT  | IPC_EXCL  | 0777;

    //Allocate shared memory and get id.
    *shmid = shmget(*key, *size, ossShmFlags);
    if(*shmid < 0) {
        perror("ERROR:oss:shmget failed(clock)");
        fprintf(stderr, "key:%d, size:%ld, flags:%d\n", *key, *size, ossShmFlags);
        exit(1);
    }

    //Set the pointer
    int* temp = (int*)shmat(*shmid, NULL, 0);
    if(temp == (int*) -1) {
        perror("ERROR:oss:shmat failed(clock)");
        exit(1);
    }

    return temp;
}

void cleanupSharedMemory(int* shmid, struct shmid_ds* ctl) {
    int cmd = IPC_RMID;
    int rtrn = shmctl(*shmid, cmd, ctl);
        if(rtrn == -1) {
            perror("ERROR:oss:shmctl failed");
            exit(1);
        }
}

void interruptSignalHandler(int sig) {
    //Cleanup the memory
    shmctl(shmMsgID, IPC_RMID, &shmMsgCtl);
    shmctl(shmClockID, IPC_RMID, &shmClockCtl);
    shmctl(shmSemID, IPC_RMID, &shmSemCtl);
    
    printf("Caught ctrl-c signal\n");

    free(pidArray);
    
    exit(0);
}

void alarmSignalHandler(int sig) {
    int i;

    //Cleanup the memory
    shmctl(shmMsgID, IPC_RMID, &shmMsgCtl);
    shmctl(shmClockID, IPC_RMID, &shmClockCtl);
    shmctl(shmSemID, IPC_RMID, &shmSemCtl);
    
    printf("OSS timed out after %d seconds,\n", terminateTime);
    
    //Send kill signal to every process thats alive
    for(i = 0; i < pidArraySize; ++i) {
        if(pidArray[i] != 0) {
            kill(pidArray[i], SIGQUIT);
        }
    }

    free(pidArray);

    exit(0);
}