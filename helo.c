// SYSTEM MANAGER
// ------------------------------- ATTENTION!! -------------------------------
// Compile as: gcc -Wall -Wextra -pthread sys_manager.c -o sys_manager -lrt

// The flag -lrt is necessary, because the functions shm_open() and shm_unlink(),
// part of the POSIX shared memory API, require the link of the librt library
// ---------------------------------------------------------------------------

#include <stdlib.h> 	// contains functions for general-purpose memory allocation, random number generation, conversion functions, and other utility functions
#include <stdio.h> 		// contains input and output functions for reading and writing to files or standard input/output
#include <stdbool.h> 	// defines the Boolean data type and provides functions for working with Boolean values
#include <string.h> 	// contains functions for manipulating strings, including copying, concatenating, comparing, and searching
#include <pthread.h> 	// contains functions and data types for creating and manipulating threads
#include <semaphore.h> 	// contains functions and data types for creating and manipulating semaphores, which are used for inter-process communication and synchronization
#include <unistd.h> 	// contains functions for performing various tasks related to system calls, including sleep, fork, and exec
#include <fcntl.h>		// contains functions and data types for working with file descriptors, including opening and closing files
#include <sys/mman.h> 	// contains functions and data types for working with memory mapping, which allows a process to access shared memory
#include <sys/types.h> 	// contains various data types used by the system, such as pid_t and size_t
#include <sys/stat.h> 	// contains functions and data types for working with file status, including checking file permissions and ownership
#include <sys/wait.h> 	// contains functions and data types for working with child processes, including waiting for a child to exit
#include <sys/ipc.h> 	// contains functions and data types for working with IPC (Inter-Process Communication) mechanisms, such as message queues and shared memory
#include <sys/shm.h> 	// contains functions and data types for working with shared memory, including creating, attaching, and detaching shared memory segments
#include <stdint.h>		// contains typedefs for integer types with specified widths, such as uint32_t and int64_t
#include <time.h>		// provides functions and data types for working with time and date information

#define NUM_PIPES 4
#define NUM_WORK_ITEMS 10 // this variable will be useless once we implement the internal queue!
#define SEM_NAME "LOG_FILE"

int pipes[NUM_PIPES][2]; // array to store pipe file descriptors
//bool* worker_status = malloc(6 * sizeof(bool));  // array to track worker status - the size is reallocated when the n_workers value is extracted from the file!!!
bool* worker_status; // array to track worker status
pthread_mutex_t worker_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex to protect worker_status
pthread_cond_t worker_cond = PTHREAD_COND_INITIALIZER; // condition variable to signal availability of workers

char *log_file_ptr, *shm_ptr;

void* console_reader_function(void* arg);
void* sensor_reader_function(void* arg);
void worker_function(int worker_index);
void* dispatcher_function(void* arg);
void* alert_watcher_function(void* arg);
void append_to_file(int fd, sem_t *sem, const char *message);

int fd;
sem_t *sem;
int queue_size, n_workers, max_keys, max_sensors, max_alerts;
FILE *f_log;
pthread_t console_reader_thread;
pthread_t sensor_reader_thread;
pthread_t dispatcher_thread;

typedef struct {
  char *alerts;
  char *sensors;
} shared_memory;

// Exits the program when the signal CTRL+C is received
void sigint_handler(int signum) {
	append_to_file(fd, sem, "SIGNAL SIGINT RECEIVED\n");
	char option[2];
	printf("\n^C pressed. Do you want to abort? ");
  	scanf("%1s", option);
  	if (option[0] == 'y') {
    	
    	// -------------------- UNNAMED PIPES AND THREADS DISASSEMBLE --------------------
		append_to_file(fd, sem, "HOME_IOT SIMULATOR WAITING FOR LAST TASKS TO FINISH!!\n");
		
		pthread_join(console_reader_thread, NULL);
		pthread_join(sensor_reader_thread, NULL);
		pthread_join(dispatcher_thread, NULL);
		
		
		// destroy mutex and condition variable
		pthread_mutex_destroy(&worker_mutex);
		pthread_cond_destroy(&worker_cond);
	  
		// close pipe file descriptors
		for (int i = 0; i < NUM_PIPES; i++) {
    		close(pipes[i][0]);
    		close(pipes[i][1]);
		}
		
		// -------------------- LOG_FILE DISASSEMBLE --------------------
		
		append_to_file(fd, sem, "HOME_IOT SIMULATOR CLOSING\n");
		// Close semaphore
    	if (sem_close(sem) == -1) {
        	perror("Error closing semaphore");
        	exit(EXIT_FAILURE);
    	}
    	
    	sem_unlink(SEM_NAME);
	
    	// Close log file
    	if (close(fd) == -1) {
        	perror("Error closing log file");
        	exit(EXIT_FAILURE);
    	}
		
    	printf("Ok, bye bye!\n");
		exit(0);
  	}
}

int main(int argc, char *argv[]){
  sem_unlink(SEM_NAME);
	
	// ---------------- COMMAND LINE PROTECTION ----------------
 	if(argc != 2) {
 		printf("Invalid command!\n");
 		return 1;
 	}
 	
	// Signal (CTRL+C) ---------------------------
	struct sigaction sa;
	sa.sa_handler = sigint_handler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);
	
 	// -------------------- LOG_FILE ASSEMBLE --------------------
 	
 	const char* LOG_FILE = "./log.txt";
    // fd = open(LOG_FILE, O_CREAT | O_RDWR | O_APPEND, 0666);
    // if (fd == -1) {
        // perror("Error opening log file");
        // exit(EXIT_FAILURE);
    // }
    f_log = fopen("log.txt", "w");

    // Initialize semaphore
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("Error initializing semaphore");
        exit(EXIT_FAILURE);
    }
    
    append_to_file(fd, sem, "HOME_IOT SIMULATOR STARTING\n");
    
    // -------------------- CONFIG_FILE ASSEMBLE --------------------  
		
	FILE *config_file_ptr = fopen(argv[1], "r");
	
	if (config_file_ptr == NULL) {
    	perror("Failed to open config file");
    	exit(1);
	}
	else {
    	fscanf(config_file_ptr, "%d", &queue_size);
    	fscanf(config_file_ptr, "%d", &n_workers);
    	fscanf(config_file_ptr, "%d", &max_keys);
    	fscanf(config_file_ptr, "%d", &max_sensors);
    	fscanf(config_file_ptr, "%d", &max_alerts);
	}
	fclose(config_file_ptr);
	
	// -------------------- UNNAMED PIPES AND THREADS ASSEMBLE --------------------
	
	// array to track worker status
	worker_status = malloc(n_workers * sizeof(bool));
	//worker_status = realloc(worker_status, n_workers * sizeof(bool));
	for (int i = 0; i < n_workers; i++) {
    	worker_status[i] = true;
	}
	
	// create console_reader thread
	pthread_create(&console_reader_thread, NULL, console_reader_function, NULL);
	append_to_file(fd, sem, "THREAD CONSOLE_READER CREATED\n");
	
	// create sensor_reador thread
	pthread_create(&sensor_reader_thread, NULL, sensor_reader_function, NULL);
	append_to_file(fd, sem, "THREAD SENSOR_READER CREATED\n");
	
	// create dispatcher thread
	if (pthread_create(&dispatcher_thread, NULL, dispatcher_function, NULL) != 0) {
    	perror("pthread_create");
    	exit(EXIT_FAILURE);
	}
	append_to_file(fd, sem, "THREAD DISPATCHER CREATED\n");
	
  	// initialize pipes
    for (int i = 0; i < NUM_PIPES; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
	
	// create worker processes - true
    pid_t worker_pids[n_workers];
    for (int i = 0; i < n_workers; i++) {
        worker_pids[i] = fork();
        if (worker_pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (worker_pids[i] == 0) {
            worker_function(i);
            exit(EXIT_SUCCESS); // exit worker process on completion of its function
		}
	}
	
	// wait for worker processes to complete
	for (int i = 0; i < n_workers; i++) {
    	int status;
    	waitpid(worker_pids[i], &status, 0);
    	printf("Worker %d exited with status %d\n", worker_pids[i], status);
	}
	
	// create worker processes
	int i, pid, status;
    for (i = 0; i < 5; i++) {
        pid = fork();
        if (pid == 0) {
            // child process code
            printf("Child process %d with pid %d\n", i+1, getpid());
            exit(0);
        } else if (pid == -1) {
            // fork error
            perror("fork");
            exit(1);
        }
    }
    // parent process code
    for (i = 0; i < 5; i++) {
        pid = wait(&status);
        if (pid == -1) {
            perror("wait");
            exit(1);
        }
        printf("Child process %d with pid %d exited with status %d\n", i+1, pid, status);
    }
	
  pause();

	exit(0);
}

void worker_function(int worker_index) {
    // int pipe_index = worker_index % NUM_PIPES;
    printf("Worker %d is now available\n", getpid());
    // worker_status[worker_index] = true;
        // char work_item[256];
        // read(pipes[pipe_index][0], work_item, 256);
        // pthread_mutex_lock(&worker_mutex);
        // worker_status[worker_index] = false;
        // printf("Worker %d is processing work item \"%s\"\n", getpid(), work_item);
        // do work here
        // worker_status[worker_index] = true;
        printf("Worker %d is now available\n", getpid());
        // pthread_cond_signal(&worker_cond); // signal the dispatcher that a worker is available
        // pthread_mutex_unlock(&worker_mutex);
    // }
}

void* dispatcher_function(void* arg) {
    char* work_queue[NUM_WORK_ITEMS] = {"Hello", "World", "!", "This", "is", "our", "Operating", "Systems", "project", ":)"}; // example work queue
    int next_work_item = 0; // keep track of next item in work queue
    while (next_work_item < NUM_WORK_ITEMS) {
        pthread_mutex_lock(&worker_mutex);
        while (true) {
            int available_worker = -1;
            for (int i = 0; i < n_workers; i++) {
                if (worker_status[i]) {
                    available_worker = i;
                    break;
                }
            }
            if (available_worker != -1) {
                int current_pipe = available_worker % NUM_PIPES;
                write(pipes[current_pipe][1], work_queue[next_work_item], strlen(work_queue[next_work_item])+1);
                printf("Dispatcher sent work item \"%s\" to worker %d through pipe %d\n", work_queue[next_work_item], available_worker, current_pipe);
                next_work_item++;
                pthread_mutex_unlock(&worker_mutex);
                break;
            }
            pthread_cond_wait(&worker_cond, &worker_mutex); // wait for a worker to become available
        }
    }
    pthread_mutex_unlock(&worker_mutex);
    return NULL;
}

void* alert_watcher_function(void* arg){
	// Code here
	return NULL;
}

void* console_reader_function(void* arg){
	// Thread que capta os comandos da User Console enviados pelo named pipe CONSOLE PIPE
	return NULL;
}

void* sensor_reader_function(void* arg){
	// Code here
	return NULL;
}

void append_to_file(int fd, sem_t *sem, const char *message){
    // Wait on semaphore
    if (sem_wait(sem) == -1) {
        perror("Error waiting on semaphore");
        exit(EXIT_FAILURE);
    }
	
	time_t seconds;
    struct tm *timeStruct;

    seconds = time(NULL);

    timeStruct = localtime(&seconds);
    
    char str[8 + 1 + strlen(message)]; // time length + blank space + message length

    sprintf(str, "%02d:%02d:%02d %s", timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec, message);
    
    // Write to log file
    // write(fd, str, strlen(message));
    fprintf(f_log, "%s\n", str);
    // Flush file buffer to disk
    fflush(f_log);
    // if (fdatasync(fd)) {
        // perror("Error flushing file buffer");
        // exit(EXIT_FAILURE);
    // }

    // Release semaphore
    if (sem_post(sem) == -1) {
        perror("Error releasing semaphore");
        exit(EXIT_FAILURE);
    }
}
