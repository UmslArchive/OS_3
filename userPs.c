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

const int DEBUG = 0;

//--------------Function prototypes-----------------------

sem_t* getSemaphore(key_t* key, size_t* size, int* shmid);
int* getShmMsg(key_t* key, size_t* size, int* shmid);
int* getShmLogicalClock(key_t* key, size_t* size, int* shmid);
void setDeathTime(int* nanosec, int* sec, int* clockPtr);
void criticalSection(int* nanosec, int* sec, int* clockPtr, int* msgPtr, sem_t* sem);

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
    size_t shmMsgSize = 2 * sizeof(int);
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

    //Life vars
    int deathNanosec = 0;
    int deathSec = 0;
    int isInitialized = 0;

    //--------------------------------
    
    if(DEBUG) {
        fprintf(stderr, "Child:%d, says hello to parent:%d\n", getpid(), getppid());
        fprintf(stderr, "shmMsgSize: %ld\n", shmMsgSize);
    }

    //Setup shared memory
    semPtr = getSemaphore(&shmSemKey, &shmSemSize, &shmSemID);
    shmMsgPtr = getShmMsg(&shmMsgKey, &shmMsgSize, &shmMsgID);
    shmClockPtr = getShmLogicalClock(&shmClockKey, &shmClockSize, &shmClockID);

    //Set lifetime of process.
    sem_wait(semPtr);
        setDeathTime(&deathNanosec, &deathSec, shmClockPtr);
    sem_post(semPtr);

    fprintf(stderr, "DeathTime: %dns %ds\n", deathNanosec, deathSec);

    //Critical section.
    while(1) {
        sleep(1);

        sem_wait(semPtr); //lock

        //Read the clock.
        int* tempClockPtr = shmClockPtr;
        int clockNano = *tempClockPtr;
        int clockSec = *(tempClockPtr + 1);

        

        //Check MSG status.
        int isMessage = 0;
        int* tempMsgPtr = shmMsgPtr;
        if(*tempMsgPtr != 0 || *(tempMsgPtr + 1) != 0) {
            isMessage = 1;
        }

        fprintf(stderr, "USRisMessage: %d\n", isMessage);

        //Exit, post, and release if time has come
        if(clockSec > deathSec || (clockSec >= deathSec && clockNano >= deathNanosec)) {
            if(isMessage == 0) {
                // /fprintf(stderr, "exit post n release\n");
                //Post the message
                *shmMsgPtr = deathNanosec;
                *(shmMsgPtr + 1) = deathSec;

                fprintf(stderr, "nano=%d, sec=%d\n", clockNano, clockSec);

                sem_post(semPtr);
                exit(50);
            }
        }

        sem_post(semPtr); //unlock
    }

    return 100;
}

int* getShmMsg(key_t* key, size_t* size, int* shmid) {
    //Fetch the shmid.
    *shmid = shmget(*key, *size, 0777);
    if(*shmid < 0) {
        perror("ERROR:usrPs:shmget failed(msg)");
        exit(1);
    }

    //Attach to segment
    int* temp = (int*)shmat(*shmid, 0, 0);
    if(temp == (int*) -1) {
        perror("ERROR:usrPs:shmat failed(msg)");
        exit(1);
    }

    return temp;
}

int* getShmLogicalClock(key_t* key, size_t* size, int* shmid){
    //Fetch the shmid.
    *shmid = shmget(*key, *size, 0777);
    if(*shmid < 0) {
        perror("ERROR:usrPs:shmget failed(clock)");
        exit(1);
    }

    //Attach to segment
    int* temp = (int*)shmat(*shmid, 0, 0);
    if(temp == (int*) -1) {
        perror("ERROR:usrPs:shmat failed(clock)");
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

void setDeathTime(int* nanosec, int* sec, int* clockPtr) {
    int* temp = clockPtr;

    //Read shared memory clock.
    *nanosec = *temp;
    *sec = *(temp + 1);

    //Randomly generate duration.
    *nanosec += 1000;

    //Rollover
    if(*nanosec >= 1000000000) {
        *nanosec = 0;
        *sec += 1;
    }
}
