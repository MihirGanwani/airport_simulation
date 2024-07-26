#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>


#define MAX_RUNWAYS 10
#define MAX_PLANES 100
#define DEPARTURE 1

struct FlightDetails {
    long mtype;
    char airportArrival;
    char airportDeparture;
    char planeID;
    double totalWeight;
    char planeType;
    int numPassengers;
    int cargoItems;
    int takenOff;
    int landingDone;
};

struct cleanup_message {
    long mtype;
    char msg_text;
};

void* handle_departure(void* arg);
void* handle_arrival(void* arg);

int airport_num;
int loadCapacityOfRunway[11];
sem_t runwaySem[11];

int find_runway(int total_weight) {
    int best_runway = -1;
    int min_diff = INT_MAX;
    
    for (int i = 0; i < MAX_RUNWAYS; i++) {
        int diff = loadCapacityOfRunway[i] - total_weight;
        if (diff >= 0 && diff < min_diff) {
            min_diff = diff;
            best_runway = i;
        }
    }
    
    int maxLoadRunway = INT_MIN;
    for(int i=0; i<MAX_RUNWAYS; i++) {
        if(loadCapacityOfRunway[i] > maxLoadRunway) {
            maxLoadRunway = loadCapacityOfRunway[i];
        }
    }

    if (best_runway == -1 && total_weight>maxLoadRunway) {
        if(total_weight<=15000) {
            printf("Using Backup Runway.\n");
            return MAX_RUNWAYS; // Using backup runway
        }
        else {
            printf("Plane too heavy.\n");
        }
    }
    else
        return best_runway;
}

void* handle_departure(void* arg) {
    printf("departure\n");
    struct FlightDetails* plane = (struct FlightDetails*)arg;
    int total_weight = plane->totalWeight;
    int runway = find_runway(total_weight);

    sem_wait(&runwaySem[runway]);

    printf("Boarding...\n");
    sleep(3);

    plane->takenOff = 1;
    
    key_t key = ftok("airtrafficcontroller.c", 'A');
    if (key == -1) {
        perror("ftok\n");
        exit(EXIT_FAILURE);
    }

    int msqid;
    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget\n");
        exit(EXIT_FAILURE);
    }

    //takeoff
    printf("Taking off\n");
    sleep(2);

    printf("Plane %d has completed boarding/loading and taken off from Runway No. %d of Airport No. %d.\n", plane->planeID, runway, airport_num);

    sem_post(&runwaySem[runway]);

    printf("Plane %d is flying\n", plane->planeID);
    sleep(30);

    plane->mtype = 230;
    if (msgsnd(msqid, plane, sizeof(struct FlightDetails), 0) == -1) {
        perror("msgsnd\n");
        exit(EXIT_FAILURE);
    }
    

    pthread_exit(NULL);
}

void* handle_arrival(void* arg) {
    printf("arrival\n");
    struct FlightDetails* plane = (struct FlightDetails*)arg;
    int total_weight = plane->totalWeight;
    int runway = find_runway(total_weight);

    sem_wait(&runwaySem[runway]);
    
    printf("Landing...\n");
    sleep(2);
    printf("Deboarding...\n");
    sleep(3);

    plane->landingDone = 1;

    key_t key = ftok("airtrafficcontroller.c", 'A');
    if (key == -1) {
        perror("ftok\n");
        exit(EXIT_FAILURE);
    }

    int msqid;
    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget\n");
        exit(EXIT_FAILURE);
    }

    plane->mtype = 231;
    if (msgsnd(msqid, plane, sizeof(struct FlightDetails), 0) == -1) {
        perror("msgsnd\n");
        exit(EXIT_FAILURE);
    }

    sem_post(&runwaySem[runway]);


    printf("Plane %d has landed on Runway No. %d of Airport No. %d and has completed deboarding/unloading.\n", plane->planeID, runway, airport_num);

    pthread_exit(NULL);
}

int main() {
    // sem_t *aatc = sem_open("atcandairport", O_CREAT, 0644, 2);
    // if (aatc == SEM_FAILED) {
    //     perror("sem_open\n");
    //     return 1;
    // }

    for (int i = 0; i < 11; ++i) {
        sem_init(&runwaySem[i], 0, 1); // Initializing with value 1 (unlocked)
    }
    
    printf("Enter Airport Number: \n");
    scanf("%d", &airport_num);
    printf("Enter number of Runways: \n");
    int num_runways;
    scanf("%d", &num_runways);
    if(num_runways<1 || num_runways>10) {
        printf("Wrong number of runways.\n");
        exit(EXIT_FAILURE);
    }
    printf("Enter loadCapacity of Runways (give as a space separated list in a single line): \n");
    for (int i = 0; i < num_runways; i++) {
        scanf("%d", &loadCapacityOfRunway[i]);
        if(loadCapacityOfRunway[i]<1000 || loadCapacityOfRunway[i]>12000) {
            printf("Load Capacity Exceeded\n");
            exit(EXIT_FAILURE);
        }
    }
    
    pthread_t threads[MAX_PLANES];

    key_t key = ftok("airtrafficcontroller.c", 'A');
    if (key == -1) {
        perror("ftok\n");
        exit(EXIT_FAILURE);
    }

    int msqid;
    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget\n");
        exit(EXIT_FAILURE);
    }

    int num_of_planes = 0;

    
    while (1) {
        struct cleanup_message cmsg;
        cmsg.msg_text = 'N';
        if(msgrcv(msqid, &cmsg, sizeof(struct cleanup_message), 69, IPC_NOWAIT) == -1) {
            if(errno==ENOMSG) {

            }
            else {
                printf("msgrcv error\n");
                exit(EXIT_FAILURE);
            }
        }
        else {
            if (msgsnd(msqid, &cmsg, sizeof(struct cleanup_message), 0) == -1) {
                perror("Error in sending message\n");
                exit(EXIT_FAILURE);
            }
        }

        int termination_condition = 0;
        if(cmsg.msg_text=='y' || cmsg.msg_text=='Y') {
            termination_condition = 1;
        }
         

        int planeArrived = 0;

        // To receive plane details from air traffic controllerr
        struct FlightDetails planemessage[10];


        printf("Waiting for planes.\n");



        ssize_t bytesReceived = msgrcv(msqid, &planemessage[num_of_planes], sizeof(struct FlightDetails), airport_num, 0);
        if (bytesReceived == -1) {
            if (errno == ENOMSG) {
                printf("No more messages of type in the queue\n");
                // break;
            } else {
                perror("msgrcv\n");
                exit(EXIT_FAILURE);
            }
        } else if (bytesReceived == 0) {
            // The queue is empty
            printf("the msgqueue is empty\n");
            // break;
        }
        else {
            // printf("Message received of type: %li\n", planemessage[num_of_planes].mtype);
            planeArrived = 1;
            // break;
        }
        
        if(planeArrived) {
            printf("handling departure/arrival\n");
            if (planemessage[num_of_planes].airportDeparture == airport_num) {
                printf("plane will depart from this port\n");
                if(pthread_create(&threads[num_of_planes], NULL, handle_departure, (void*)&planemessage[num_of_planes])!=0) {
                    printf("Failed to create thread.\n");
                    exit(EXIT_FAILURE);
                }
            }
            else if (planemessage[num_of_planes].airportArrival == airport_num) {
                printf("plane will arrive at this port\n");
                if(pthread_create(&threads[num_of_planes], NULL, handle_arrival, (void*)&planemessage[num_of_planes])!=0) {
                    printf("Failed to create thread.\n");
                    exit(EXIT_FAILURE);
                }
            }

            num_of_planes++;

            
            planeArrived = 0;
        }

        if(msgrcv(msqid, &cmsg, sizeof(struct cleanup_message), 69, IPC_NOWAIT) == -1) {
            if(errno==ENOMSG) {
                
            }
            else {
                printf("msgrcv error\n");
                exit(EXIT_FAILURE);
            }
        }
        else {
            if (msgsnd(msqid, &cmsg, sizeof(struct cleanup_message), 0) == -1) {
                perror("Error in sending message\n");
                exit(EXIT_FAILURE);
            }
        }

        if(cmsg.msg_text=='y' || cmsg.msg_text=='Y') {
            termination_condition = 1;
        }
        
        if (termination_condition) {
            break;
        }
    }
    
    // Joining threads
    for (int i = 0; i < num_of_planes; i++) {
        pthread_join(threads[i], NULL);
    }
    

    // // Close the message queue
    // if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    //     perror("msgctl");
    //     exit(EXIT_FAILURE);
    // }

    
    for (int i = 0; i < MAX_RUNWAYS; ++i) {
        sem_destroy(&runwaySem[i]);
    }

    
    // Exit
    return 0;
}
