#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>
#include<fcntl.h> // for open
#include<unistd.h> // for close
#include<pthread.h>
#include<signal.h>
#include"common.h"

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 

struct Client { 
    char name[50];
    int group_id;
    int sck_id; // 0 indicates sock id is deleted.
    struct Client* next; 
};

// push at the end of the linked list
void append(struct Client* head, struct Client* new_node) {
    if (head == NULL) {
        head = new_node;
        return;
    }
    while(head->next != NULL) {
        head = head->next;
    }
    head->next = new_node;
    return;
}

struct Message {
    char message[500];
    int group_id;
    char client_name[50];
};



// ------------------------------Queue implementation
struct QNode { 
    struct Message key;
    struct t_format t;
    struct QNode* next; 
};

// The queue, front stores the front node of LL and rear stores the 
// last node of LL 
struct Queue { 
    struct QNode *front, *rear;
    pthread_mutex_t q_lock;
}; 
  
// A global queue element which stores all the messages
struct Queue* q;

// A utility function to create an empty queue 
struct Queue* createQueue() 
{ 
    q = (struct Queue*)malloc(sizeof(struct Queue)); 
    q->front = q->rear = NULL; 
    return q; 
} 

void enQueue(struct QNode* new) 
{ 
    // If queue is empty, then new node is front and rear both 
    if (q->rear == NULL) { 
        q->front = q->rear = new; 
        return; 
    } 
  
    // Add the new node at the end of queue and change rear 
    q->rear->next = new; 
    q->rear = new; 
} 
  
// Function to remove a key from given queue q 
void deQueue() 
{ 
    // If queue is empty, return NULL. 
    if (q->front == NULL) 
        return; 
  
    // Store previous front and move front one node ahead 
    struct QNode* temp = q->front; 
  
    q->front = q->front->next; 
  
    // If front becomes NULL, then change rear also as NULL 
    if (q->front == NULL) 
        q->rear = NULL; 
  
    free(temp); 
} 
// -----------------------------Queue implementation ends.


// delete the client from the group
void delete(struct Client* head, int id) { 
    while(head->next != NULL && (head->sck_id != id)) {
        head = head->next;
    }
    // delete the client
    head->sck_id = 0;
}




// Each group contains linked list of clients
// max 4 groups allowed
struct Client* groups[4];

void traverse(struct Client* head) {
    char buff[500];
    while(head != NULL) {
        if ((head->sck_id != 0) && (strcmp(head->name, q->front->key.client_name) != 0)) {
            memset(buff, '\0', sizeof(buff));
            char name[30];
            strcpy(name, q->front->key.client_name);
            strncat(name, ": ", 2);
            strcpy(buff, name);
            strncat(buff, q->front->key.message, sizeof(q->front->key.message));
            struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
            t_m->t = q->front->t;
            strcpy(t_m->buff, buff);
            send(head->sck_id, t_m, sizeof(struct time_message), 0);
        } 
        head = head->next;
    }
}

void* queue_thread_func(void* fd) {
    while(1) {
        if (q->front != NULL) {
            int g_id = q->front->key.group_id;
            pthread_mutex_lock(&q->q_lock);
            traverse(groups[g_id]);
            deQueue();
            pthread_mutex_unlock(&q->q_lock);
        }
    }
}

// Handler for client threads
void* client_thread(void* connfd) {
    int sckfd = *((int *)connfd);

    struct Client* client = (struct Client*)malloc(sizeof(struct Client));
    char buff[500];

    strcpy(buff, "Welcome to chatapp group\n");
    send(sckfd, buff, sizeof(buff), 0);
    memset(buff, '\0', sizeof(buff));

    strcpy(buff, "Enter your name: ");
    send(sckfd, buff, sizeof(buff), 0);
    memset(buff, '\0', sizeof(buff));

    // read the name of the client
    recv(sckfd, buff, sizeof(buff), 0); 
    strcpy(client->name, buff);
    memset(buff, '\0', sizeof(buff));

    strcpy(buff, "Enter your group id: ");
    send(sckfd, buff, sizeof(buff), 0);
    memset(buff, '\0', sizeof(buff));

    // Read group id
    recv(sckfd, buff, sizeof(buff), 0);
    client->group_id = atoi(buff);
    client->sck_id = sckfd;

    
    pthread_mutex_lock(&lock);
    // Add the client into the group
    if (groups[client->group_id] == NULL) {
        groups[client->group_id] = client;
    } else {
        append(groups[client->group_id], client);
    }
    pthread_mutex_unlock(&lock);

    // Now read the message from the client for infinite time
    // until he quits
    while(1) {
        struct time_message* t_m = (struct time_message*)malloc(sizeof(struct time_message));
        recv(sckfd, t_m, sizeof(struct time_message), 0);
        if (strcmp(t_m->buff, "exit") == 0) {
            delete(groups[client->group_id], client->sck_id);
            break;
        }
        // Now make the QNode and enque it.
        struct Message msg;
        strcpy(msg.client_name, client->name);
        msg.group_id = client->group_id;
        strcpy(msg.message, t_m->buff);
        struct QNode* qnode = (struct QNode*)malloc(sizeof(struct QNode));
        qnode->key = msg;
        qnode->t = t_m->t;
        qnode->next = NULL;
        pthread_mutex_lock(&q->q_lock);
        enQueue(qnode);
        pthread_mutex_unlock(&q->q_lock);
    }
}

struct argument {
    char* sddr;
    int portno;
} arg;
    
// Main thread will listen to the clients and form groups and
// will be responsible for creating new client threads and
// assigning them into their respective groups and also will be
// responsible for deleting the clients.
void* main_thread_func(void* argv) {
    int sockfd, connfd, len;
    struct sockaddr_in serveraddr,cli;
    struct argument* args = (struct argument*) argv;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1){
        printf("socket creation failed\n");
    }   
    else printf("socket successfully created\n");

    // memset(&serveraddr,0,sizeof(serveraddr));
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(args->sddr);
    serveraddr.sin_port = htons(args->portno);

    if ((bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) != 0) {
        printf("socket bindings failed\n");
        exit(0);
    }
    else printf("socket binding succesfull\n");

    if ((listen(sockfd, 100)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n");

    pthread_t client_thread_ids[100];
    int i = 0;
    while(1) {
        socklen_t addrsize = sizeof(cli);
        if ((connfd = accept(sockfd, (struct sockaddr*)&cli, &(addrsize))) >= 0) {
            printf("server accepted the client\n");
        } else printf("server accept failed\n");
        // Now create the thread for each client request accepted
        if(pthread_create(&client_thread_ids[i++], NULL, client_thread, &connfd) == 0) {
            printf("Thread created successfull\n");
        } else printf("Thread failed to create\n");
    }

    // join all the threads before terminating the main thread.
    for(int j = 0; j < i; j++) {
        pthread_join(client_thread_ids[j], NULL);
    }

    // close the socket
    close(sockfd);
}

int main(int argc, char* argv[]) {
    createQueue();
    pthread_t main_thread_id;
    pthread_t queue_proc_thread_id;

    arg.sddr = argv[1];
    arg.portno = atoi(argv[2]);

    pthread_create(&main_thread_id, NULL, main_thread_func, (void *) &arg);
    pthread_create(&queue_proc_thread_id, NULL, queue_thread_func, &queue_proc_thread_id);
    pthread_join(queue_proc_thread_id, NULL);
    pthread_join(main_thread_id, NULL);
}
