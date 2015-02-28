#include <sys/types.h>
#include <unistd.h>

#include "message_queue.h"
#include "external.h"

// Global because it is used in a signal handler (which cannot accept parameters)
int externalMessageQueueId;             // The ID of the UNIX message queue we will receive messages from

int main(int argc, char* argv[]) {
    int externalTemperature;            // Temperature from this weather station
    int centralMessageQueueId;          // The ID of the UNIX message queue we will send messages to
    int status;                         // The return value of msgctl when we kill off the external message queue
    struct MsgInfo msg;                 // Used to send and receive temperatures from the UNIX message queue
    struct msqid_ds dummyParam;         // Necessary to close the external UNIX message queue in function msgctl
    
    signal(SIGTERM, CleanUp);
    
    if (argc != 4) {
        fprintf(stderr, "%d -- This program requires 3 arguments: First starting temperature. Then central message queue id. Then external message queue id\n",getpid());
        return 0;
    }
    
    fprintf(stderr, "%d -- Memory replaced in child process\n", getpid());
    
    // Initialize with the value passed in
    externalTemperature = atoi(argv[1]);
    centralMessageQueueId = atoi(argv[2]);
    externalMessageQueueId = atoi(argv[3]);
    
    while (TRUE) {
        if (!ReceieveMessage(externalMessageQueueId, &msg)) {
            fprintf(stderr, "%d -- ReceiveMessage failed. Quitting program\n", getpid());
            return 1;
        }
        if (msg.stable) {
            // Now that the process is stable, close the associated message queue
            status= msgctl(externalMessageQueueId, IPC_RMID, &dummyParam);
            break;
        }
        msg.temperature = NewExternalTemp(externalTemperature, msg.temperature);
        msg.pid = getpid();
        if (!SendMessage(centralMessageQueueId, msg)) {
            fprintf(stderr, "%d -- SendMessage failed. Quitting program\n", getpid());
            return 1;
        }
    }
    printf("%d -- Temperature stabilized at %d\n", getpid(), externalTemperature);
    CleanUp(externalMessageQueueId);
}

int NewExternalTemp(int currentTemp, int centralTemp) {
    return (currentTemp * 3 + 2 * centralTemp) / 5;
}

void CleanUp() {
    struct msqid_ds dummyParam;   // Necessary to close the central UNIX message queue in function msgctl
    int status;                   // The return value of msgctl when we kill off the central message queue
    
    fprintf(stderr, "%d -- External process cleaning up queue %d now\n", externalMessageQueueId, getpid());
    
    // Now that the central process is stable, close the central message queue
    status= msgctl(externalMessageQueueId, IPC_RMID, &dummyParam);
    exit(0);
}
