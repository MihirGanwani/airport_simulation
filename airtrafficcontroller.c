#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>


#define MAX_AIRPORTS 10
#define MAX_MESSAGE_SIZE 256
#define AIR_TRAFFIC_CONTROLLER_FILE "AirTrafficController.txt"

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

void log_journey(int departure_airport, int arrival_airport, int plane_id) {
    FILE *file = fopen(AIR_TRAFFIC_CONTROLLER_FILE, "a");
    if (file != NULL) {
        fprintf(file, "Plane %d has departed from Airport %d and will land at Airport %d.\n",
                plane_id, departure_airport, arrival_airport);
        fclose(file);
    } else {
        perror("Error opening file for logging\n");
    }
}

int main() {
    int num_airports;
    int msqid;
    struct FlightDetails planemessage;
    pid_t pid;

    // sem_t *patc = sem_open("waitingforplanetosend", O_CREAT, 0644, 2);
    // if (patc == SEM_FAILED) {
    //     perror("sem_open\n");
    //     return 1;
    // }

    // sem_t *aatc = sem_open("atcandairport", O_CREAT, 0644, 2);
    // if (aatc == SEM_FAILED) {
    //     perror("sem_open\n");
    //     return 1;
    // }


    printf("Enter the number of airports to be handled/managed: \n");
    scanf("%d", &num_airports);

    if (num_airports < 2 || num_airports > MAX_AIRPORTS) {
        fprintf(stderr, "Invalid number of airports. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    key_t key = ftok("airtrafficcontroller.c", 'A');
    if (key == -1) {
        perror("ftok\n");
        exit(EXIT_FAILURE);
    }

    if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget\n");
        exit(EXIT_FAILURE);
    }

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

        printf("waiting for plane\n");
        // sem_wait(patc);

        //receive request to fly from plane 
        if (msgrcv(msqid, &planemessage, sizeof(struct FlightDetails), 13, 0) == -1) {
            printf("msgrcv: %d\n", errno);
            exit(EXIT_FAILURE);
        }


        printf("message received from plane\n");
        // sem_post(patc);

        //sending message to departure airport
        planemessage.mtype = planemessage.airportDeparture;
        if (msgsnd(msqid, &planemessage, sizeof(struct FlightDetails), 0) == -1) {
            perror("msgsnd\n");
            exit(EXIT_FAILURE);
        }


        //receiving message from departure airport that the plane has departed
        struct FlightDetails departedPlane;
        if (msgrcv(msqid, &departedPlane, sizeof(struct FlightDetails), 230, 0) == -1) {
            printf("msgrcv: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        //sending message to arrival airport 
        departedPlane.mtype = departedPlane.airportArrival;
        if (msgsnd(msqid, &departedPlane, sizeof(struct FlightDetails), 0) == -1) {
            perror("msgsnd\n");
            exit(EXIT_FAILURE);
        }


        log_journey(planemessage.airportDeparture, planemessage.airportArrival, planemessage.planeID);


        //receiving confirmation from airport that the plane has arrived
        struct FlightDetails arrivedPlane;
        if (msgrcv(msqid, &arrivedPlane, sizeof(struct FlightDetails), 231, 0) == -1) {
            printf("msgrcv: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        // printf("Plane has arrived its destination\n");

        //sending message to the arrived plane so that it can terminate
        arrivedPlane.mtype = 40+arrivedPlane.planeID;
        if (msgsnd(msqid, &arrivedPlane, sizeof(struct FlightDetails), 0) == -1) {
            perror("msgsnd\n");
            exit(EXIT_FAILURE);
        }




        // sem_post(aatc);

        if(msgrcv(msqid, &cmsg, sizeof(struct cleanup_message), 69, IPC_NOWAIT) == -1) {
            if(errno==ENOMSG) {
                printf("message not received from cleanup.\n");
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


        if(termination_condition) {
            break;
        }


    }

    // // Close the message queue
    // if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    //     perror("msgctl");
    //     exit(EXIT_FAILURE);
    // }

    return 0;
}
