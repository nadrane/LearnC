#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

char** tokenize(const char* input);

struct MsgInfo {
    long priority;       /* message type, must be > 0 */
    int temperature;
    int pid;
    int stable;
};

struct Payload {
    long mtype;
    char mtext[50];
};

struct Payload ConvertToPayload(struct MsgInfo msg) {
    struct Payload payload;
    
    payload.mtype = 2;
    sprintf(payload.mtext, "%d,%d,%d", msg.temperature, msg.pid, msg.stable);
    return payload;
}

struct MsgInfo ConvertToMsgInfo(struct Payload payload) {
    struct MsgInfo msg;
    char** tokens = tokenize(payload.mtext);
    
    msg.priority = payload.mtype;
    msg.temperature = atoi(tokens[0]);
    msg.pid = atoi(tokens[1]);
    msg.stable = atoi(tokens[2]);
    return msg;
}

struct MsgInfo CreateMsgInfo(int temperature, int pid, int stable) {
    struct MsgInfo msg;
    
    msg.priority = 2;
    msg.temperature = temperature;
    msg.pid = pid;
    msg.stable = stable;
    return msg;
}

int ReceieveMessage(int msgQueueId, struct MsgInfo* msg) {
    int bytesReceived = 0;
    int success = TRUE;
    struct Payload payload;
    
    fprintf(stderr, "%d -- Receiving temperature from message queue %d\n", getpid(), msgQueueId);
    // Returns number of bytes received on success, -1 on failure
    bytesReceived = msgrcv(msgQueueId, &payload, sizeof(payload.mtext), 2, 0);
    if (bytesReceived == -1) {
        fprintf(stderr, "%d -- msgrcv failed: %s\n", getpid(), strerror(errno));
        success = FALSE;
    } else {
        *msg = ConvertToMsgInfo(payload);
    }
    return success;
}

int SendMessage(const int msgQueueId, const struct MsgInfo msg) {
    int success = TRUE;
    struct Payload payload;
    
    payload = ConvertToPayload(msg);
    fprintf(stderr, "%d -- Sending temperature to message queue %d. Message: %s\n", getpid(), msgQueueId, payload.mtext);
    // msgsnd returns -1 on failure
    if (msgsnd(msgQueueId, &payload, sizeof(payload.mtext), MSG_NOERROR) == -1) {
        fprintf(stderr, "%d -- msgsnd failed: %s\n", getpid(), strerror(errno));
        success = FALSE;
    }
    return success;
}

int CreateMessageQueue(int queueName) {
    int msgQueueId = msgget(queueName, 0600 | IPC_CREAT);
    if (msgQueueId < 0) {
        fprintf(stderr, "%d -- Message queue with name %d failed to create\n", getpid(), queueName);
    } else {
        fprintf(stderr, "%d -- Creating message queue with name %d and id %d\n", getpid(), queueName, msgQueueId);
    }
    return msgQueueId;
}

char** tokenize(const char* input) {
    char* str = strdup(input);
    int count = 0;
    int capacity = 10;
    char* tok;
    char** result;
    
    fprintf(stderr, "%d -- Starting tokenizer\n", getpid());
    
    result = malloc(capacity * sizeof(*result));
    tok = strtok(str, ",");
    
    fprintf(stderr, "%d -- Token %d = %s\n", getpid(), count, tok);
    
    while(1)
    {
        if (count >= capacity) {
            result = realloc(result, (capacity*=2)*sizeof(*result));
        }
        
        result[count++] = tok ? strdup(tok) : tok;
        
        if (!tok) break;
        
        tok = strtok(NULL, ",");
        fprintf(stderr, "%d -- token %d ==%s\n", getpid(), count, tok);
    }
    
    free(str);
    return result;
}
