#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/msg.h>

#include "message_queue.h"
#include "central.h"


int main(int argc, char* argv[]) {
    int messageQueueIds[4];     // An array containing all the message queue ids opened by child processes.
    int centralMessageQueueId;    // The message queue ID for the central process.
    int childProcessIds[4];     // An array containing all of the child process ids.
    int externalTemps[100][4];    // Store a history of all external temps for each child over time
    int childCount;               // How many child processes are there?
    int maxChildCount;            // How many children will we kick off?
    int centralTemperature;       // The temperature of central
    int isStable = FALSE;         // Are we still calculating the central temperature?
    struct MsgInfo msg;           // A message to be sent between two processes
    int maxIterations;            // How many times will we try to stabilize the central temperature
    int iterations = 0;           // How many iterations have we tried so far?
    
    if (argc != 4) {
        fprintf(stderr, "%d -- This program requires 3 arguments. First children, then starting temperature, then iterations\n", getpid());
        return 0;
    }
    
    srand(time(NULL));
    
    // Initialize with the values passed in
    maxChildCount = atoi(argv[1]);
    centralTemperature = atoi(argv[2]);
    maxIterations = atoi(argv[3]);
    
    // Create central process's message queue
    centralMessageQueueId = CreateMessageQueue(100);
    
    // Fork off all of the children
    for (childCount = 0; childCount < maxChildCount; childCount++) {
        if (!ForkChild(centralMessageQueueId, messageQueueIds, childProcessIds, childCount)) {
            return 1;
        }
    }
    
    // Calculate the central temperature
    while (!isStable) {
        // First tell all of the children what the central temperature is
        msg = CreateMsgInfo(centralTemperature, getpid(), isStable);
        for (childCount = 0; childCount < maxChildCount; childCount++) {
            if (!SendMessage(messageQueueIds[childCount], msg))
            {
                fprintf(stderr, "%d -- SendMessage failed. Quitting program.\n", getpid());
                CleanUp(TRUE, maxChildCount, centralMessageQueueId, childProcessIds);
                return 0;
            }
        }
        
        // Now get responses of the current external temperature for all children
        for (childCount = 0; childCount < maxChildCount; childCount++) {
            if (!ReceieveMessage(centralMessageQueueId, &msg)) {
                fprintf(stderr, "%d -- ReceieveMessage failed. Quitting program.\n", getpid());
                CleanUp(TRUE, maxChildCount, centralMessageQueueId, childProcessIds);
                return 0;
            }
            externalTemps[iterations][childCount] = msg.temperature;
        }
        
        // Check to see if all of the external processes calculated the same temperature for two iterations in a row
        for (childCount = 0; childCount <= maxChildCount; childCount++) {
            if (externalTemps[iterations][childCount] != externalTemps[iterations -1 ][childCount]) {
                break;
            }
            isStable = TRUE;
        }
        
        // Calculate the new temperature if we are not stable yet
        if (!isStable) {
            centralTemperature = NewCentralTemp(externalTemps[iterations], centralTemperature, maxChildCount);
        }
        
        iterations++;
        
        // Force quit the children if we hit the max number of iterations
        if (iterations > maxIterations) {
            CleanUp(TRUE, maxChildCount, centralMessageQueueId, childProcessIds);
            return 0;
        }
    }
    
    // Handle the case where maxIterations was reached. isStable will not be true in this case, and we won't want to send messages to the children.
    if (isStable) {
        // Send out one last message so that the children know the temperature is stable. This lets them quit gracefully.
        msg = CreateMsgInfo(centralTemperature, getpid(), isStable);
        for (childCount = 0; childCount < maxChildCount; childCount++) {
            if (!SendMessage(messageQueueIds[childCount], msg))
            {
                fprintf(stderr, "%d -- SendMessage failed. Quitting program.\n", getpid());
                return 0;
            }
        }
    }
    CleanUp(FALSE, maxChildCount, centralMessageQueueId, childProcessIds);
    
    printf("%d -- Temperature stabilized at %d\n", getpid(), centralTemperature);
    return 0;
}

int ForkChild(int centralMessageQueueId, int messageQueueIds[], int childProcessIds[], int childCount) {
    int msgQueueId;
    int childPID = fork();
    int startingTemp;
    char tempArg[15];
    char externalQueueArg[15];
    char centralQueueArg[15];
    
    // We will open mailboxes starting with the name 100 and increment by 1 for each child
    // We want both the parent and child to open a connection to this mailbox, so keep this before exec**
    // Both parent and child get read/write privileges.
    msgQueueId = CreateMessageQueue(101 + childCount);
    messageQueueIds[childCount] = msgQueueId;
    
    if (childPID < 0) {
        fprintf(stderr, "%d -- Child process %d failed to start\n", getpid(), childCount + 1);
        return FALSE;
    // Child process
    } else if (childPID == 0){
        startingTemp = rand() % 100;
        sprintf(tempArg, "%d", startingTemp);
        sprintf(externalQueueArg, "%d", messageQueueIds[childCount]);
        sprintf(centralQueueArg, "%d", centralMessageQueueId);
        fprintf(stderr, "%d -- Replacing memory in child process %d. temp = %s. queue = %s\n", getpid(), childCount + 1, tempArg, externalQueueArg);
        if (execlp("./external", "external", tempArg, centralQueueArg, externalQueueArg, NULL) == -1) {
            perror("execlp failed");
        }
    // Parent process
    } else {
        fprintf(stderr, "%d -- Child process with PID %d started\n", getpid(), childPID);
        childProcessIds[childCount] = childPID;
    }
    return TRUE;
}

int NewCentralTemp(int externalTemps[], int centralTemp, int maxChildCount) {
    int externalTempSum = 0;
    for (int i = 0; i < 4; i++) {
        externalTempSum += externalTemps[i];
    }
    return (2 * centralTemp + externalTempSum) / (2 + maxChildCount);
}

void CleanUp(int isForced, int maxChildCount, int centralMessageQueueId, int childProcessIds[]) {
    struct msqid_ds dummyParam;   // Necessary to close the central UNIX message queue in function msgctl
    int status;                   // The return value of msgctl when we kill off the central message queue
    
    fprintf(stderr, "%d -- Central process cleaning up now\n", getpid());
    
    // If there was an error in the central process, force kill the children.
    // Otherwise they will end gracefully themselves.
    if (isForced) {
        // Force quit all of the child processes
        for (int childCount = 0; childCount < maxChildCount; childCount++) {
            kill(childProcessIds[childCount], SIGTERM);
        }
    }
    
    // Now that the central process is stable, close the central message queue
    status = msgctl(centralMessageQueueId, IPC_RMID, &dummyParam);
}

