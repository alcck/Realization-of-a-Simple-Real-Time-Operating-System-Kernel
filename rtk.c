#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

// Define constants
#define MAX_PROCESSES 100
#define MAX_NAME_LENGTH 20
#define MAX_FILE_SIZE 10000
#define TIME_SLICE 100 // Time slice for time-sliced processes (in milliseconds)

// Define process states
enum ProcessState {
    READY,
    RUNNING,
    BLOCKED,
    DELAYED
};

// Define PCB (Process Control Block) structure
typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    int type; // Real-Time Process (RTP) or Time-Sliced Process (TCP)
    enum ProcessState state;
    // Add other necessary fields (registers, semaphore id, etc.)
} PCB;

// Define PCB Queue structure
typedef struct {
    PCB* queue[MAX_PROCESSES];
    int front, rear;
    // Add other necessary fields
} PCBQueue;

// Define Semaphore structure
typedef struct {
    enum ProcessState state;
    int value;
    PCBQueue* blockedQueue;
    // Add other necessary fields
    sem_t mutex;
} Semaphore;

// Define function prototypes
void enqueue(PCBQueue* q, PCB* process);
PCB* dequeue(PCBQueue* q);
void* scheduler(void* arg);
void make_proc(PCB* process, char* name, int type);
void make_ready(PCB* process);
void block(PCB* process);
void make_sem(Semaphore* sem, int value);
void wait_sem(Semaphore* sem);
void signal_sem(Semaphore* sem);
void* producer(void* arg);
void* consumer(void* arg);

// Global variables
Semaphore s1, s2;
char shared;
FILE* file_rtk;

int main() {
    // Initialize resources
    PCB producer_pcb, consumer_pcb;
    pthread_t scheduler_thread, producer_thread, consumer_thread;

    // Initialize semaphores
    make_sem(&s1, 1);
    make_sem(&s2, 0);

    // Open file
    file_rtk = fopen("rtk.c", "r");

    // Create scheduler thread
    pthread_create(&scheduler_thread, NULL, scheduler, NULL);

    // Create producer thread
    make_proc(&producer_pcb, "Producer", 0); // 0 for Real-Time Process
    pthread_create(&producer_thread, NULL, producer, &producer_pcb);

    // Create consumer thread
    make_proc(&consumer_pcb, "Consumer", 0); // 0 for Real-Time Process
    pthread_create(&consumer_thread, NULL, consumer, &consumer_pcb);

    // Wait for threads to finish
    pthread_join(scheduler_thread, NULL);
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    // Clean up resources
    fclose(file_rtk);
    sem_destroy(&s1.mutex);
    sem_destroy(&s2.mutex);

    return 0;
}

void enqueue(PCBQueue* q, PCB* process) {
    q->rear = (q->rear + 1) % MAX_PROCESSES;
    q->queue[q->rear] = process;
}

PCB* dequeue(PCBQueue* q) {
    PCB* process = q->queue[q->front];
    q->front = (q->front + 1) % MAX_PROCESSES;
    return process;
}

void* scheduler(void* arg) {
    // Initialize file pointers
    FILE* file_rtk2 = fopen("rtk2.c", "w");
    if (file_rtk2 == NULL) {
        printf("Error: Unable to create file rtk2.c\n");
        exit(EXIT_FAILURE);
    }

    // Create PCBs for producer and consumer processes
    PCB producer_pcb, consumer_pcb;
    make_proc(&producer_pcb, "Producer", 0); // 0 for Real-Time Process
    make_proc(&consumer_pcb, "Consumer", 0); // 0 for Real-Time Process

    while (1) {
        // Run producer process
        wait_sem(&s1);
        char c = fgetc(file_rtk);
        if (c == EOF) {
            shared = 0;
            signal_sem(&s2);
            break;
        }
        shared = c;
        signal_sem(&s2);
        usleep(TIME_SLICE * 1000); // Sleep for TIME_SLICE milliseconds

        // Run consumer process
        wait_sem(&s2);
        if (shared == 0)
            break;
        fputc(shared, file_rtk2);
        signal_sem(&s1);
        usleep(TIME_SLICE * 1000); // Sleep for TIME_SLICE milliseconds
    }

    // Close file pointers
    fclose(file_rtk2);

    // Exit scheduler thread
    pthread_exit(NULL);
}


void make_proc(PCB* process, char* name, int type) {
    static int id_counter = 0;
    process->id = id_counter++;
    strncpy(process->name, name, MAX_NAME_LENGTH);
    process->type = type;
    process->state = READY;
    // Initialize other fields as needed
}

void make_ready(PCB* process) {
    process->state = READY;
}

void block(PCB* process) {
    process->state = BLOCKED;
}

void make_sem(Semaphore* sem, int value) {
    sem_init(&sem->mutex, 0, value);
    sem->value = value;
    sem->state = READY;
    // Initialize blocked queue
    sem->blockedQueue = (PCBQueue*)malloc(sizeof(PCBQueue));
    if (sem->blockedQueue == NULL){
	printf("Error: Failed to allocate memory for blocked queue\n");
	exit(EXIT_FAILURE);
 }
sem->blockedQueue->front = sem->blockedQueue->rear = -1;
}

void wait_sem(Semaphore* sem) {
    sem_wait(&sem->mutex);
    sem->value--;
    if (sem->value < 0) {
        PCB* process;
	block(process);
        enqueue(sem->blockedQueue,process);
    }
}

void signal_sem(Semaphore* sem) {
    sem_post(&sem->mutex);
    sem->value++;
    if (sem->value <= 0) {
        // Unblock a process from sem->blockedQueue and make it ready
        PCB* process = dequeue(sem->blockedQueue);
        make_ready(process);
    }
}

void* producer(void* arg) {
    PCB* process = (PCB*)arg;
    char c;
    while (1) {
        wait_sem(&s1);
        c = fgetc(file_rtk);
        if (c == EOF){
	 shared = 0;
	 signal_sem(&s2);
         break;
	}
    shared = c;
    signal_sem(&s2);
    }
fclose(file_rtk);
pthread_exit(NULL);
}

void* consumer(void* arg) {
    PCB* process = (PCB*)arg;
    FILE* file_rtk2 = fopen("rtk2.c", "w");
    if (file_rtk2 == NULL) {
        printf("Error: Unable to create file rtk2.c\n");
        exit(EXIT_FAILURE);
    }
    while (1) {
        wait_sem(&s2);
        if (shared == 0)
            break;
        fputc(shared, file_rtk2);
        signal_sem(&s1);
    }
    fclose(file_rtk2);
    pthread_exit(NULL);
}
