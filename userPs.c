#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>

#include "sharedMemoryKeys.h"

const int DEBUG = 1;

//--------------Function prototypes-----------------------

sem_t* getSemaphore(key_t* key, size_t* size, int* shmid);
int* getShmMsg(key_t* key, size_t* size, int* shmid);
int* getShmLogicalClock();

//--------------------------------------------------------


int main(int argc, char* argv[]) {
    //Iterator
    int i;

    //Shared memory keys
    key_t shmSemKey = SEM_KEY;
    key_t shmMsgKey = MSG_KEY;
    key_t shmClockKey = CLOCK_KEY;

    //Shared memory IDs
    int shmSemID = 0;
    int shmMsgID = 0;
    int shmClockID = 0;

    //Shared memory sizes
    size_t shmSemSize = sizeof(sem_t);
    size_t shmMsgSize = 0;
    size_t shmClockSize = 2 * sizeof(int);

    //Shared memory control structs.
    struct shmid_ds shmSemCtl;
    struct shmid_ds shmMsgCtl;
    struct shmid_ds shmClockCtl;
    int rtrn;

    //Shared memory pointers
    sem_t* semPtr = NULL;
    int* shmMsgPtr = NULL;
    int* shmClockPtr = NULL;

    //--------------------------------
    
    //Set size of shmMsg array.
    shmMsgSize = (unsigned long) atoi(argv[1]);

    if(DEBUG) {
        fprintf(stderr, "Child:%d, says hello to parent:%d\n", getpid(), getppid());
        fprintf(stderr, "shmMsgSize: %ld\n", shmMsgSize);
    }

    sleep(1);

    //Point the pointer to the semaphore.
    semPtr = getSemaphore(&shmSemKey, &shmSemSize, &shmSemID);

    //Point the pointer to the shared memory.
    shmMsgPtr = getShmMsg(&shmMsgKey, &shmMsgSize, &shmMsgID);

    sem_wait(semPtr);
    //Test print.
    fprintf(stderr, "MSGArray: ");
    for(i = 0; i < shmMsgSize; ++i) {
        fprintf(stderr, "%d ", *shmMsgPtr++);
    }
    fprintf(stderr, "\n");
    sem_post(semPtr);

    return 50;
}

int* getShmMsg(key_t* key, size_t* size, int* shmid) {
    //Fetch the shmid.
    *shmid = shmget(*key, *size, 0777);
    if(*shmid < 0) {
        perror("ERROR:usrPs:shmget failed");
        exit(1);
    }

    //Attach to segment
    int* temp = (int*)shmat(*shmid, 0, 0);
    if(temp == (int*) -1) {
        perror("ERROR:usrPs:shmat failed");
        exit(1);
    }

    return temp;
}

sem_t* getSemaphore(key_t* key, size_t* size, int* shmid) {
    //Fetch the shmid.
    *shmid = shmget(*key, *size, 0777);
    if(*shmid < 0) {
        perror("ERROR:usrPs:shmget failed(semaphore)");
        exit(1);
    }

    //Attach to segment
    sem_t* temp = (sem_t*)shmat(*shmid, 0, 0);
    if(temp == (sem_t*) -1) {
        perror("ERROR:usrPs:shmat failed(semaphore)");
        exit(1);
    }

    return temp;
}