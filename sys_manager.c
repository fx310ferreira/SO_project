/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include "structs.h"

/* Variables read from file 0: QUEUE_SZ, 1: N_WORKERS, 2: MAX_KEYS, 3: MAX_SENSORS, 4: MAX_ALERTS */
int config[5];
FILE *config_file;
FILE *log_file;
pthread_t sensor_reader_t, console_reader_t, dispatcher_t;
sem_t *log_sem, *shm_sem;
shared_memory *shm;
int fd_sensor, fd_console;
int shmid = 0, parent_pid;


/* Function that is responsible for writing logs */
void logger(char *message){
  time_t now = time(NULL);
  struct tm *date_time = localtime(&now);
  sem_wait(log_sem);
  printf("%02d:%02d:%02d %s\n", date_time->tm_hour, date_time->tm_min, date_time->tm_sec, message);
  fprintf(log_file, "%4d/%02d/%02d %02d:%02d:%02d %s\n", date_time->tm_year+1900, date_time->tm_mon+1, date_time->tm_mday, date_time->tm_hour, date_time->tm_min, date_time->tm_sec, message);
  fflush(log_file);
  sem_post(log_sem);
}

/* Function to handle errors*/
void error(char *error_msg){
  // add logger to print error message
  printf("ERROR: %s\n", error_msg);

  logger("HOME_IOT SIMULATOR FREEING RESOURCES");

  if(config_file != NULL){
    fclose(config_file);
  }
  //! SHOULD I USE pthread_kill(****, SIGKILL)
  pthread_cancel(sensor_reader_t);
  pthread_cancel(console_reader_t);
  pthread_cancel(dispatcher_t);
  pthread_join(sensor_reader_t, NULL);
  pthread_join(console_reader_t, NULL);
  pthread_join(dispatcher_t, NULL);
  if(shmid != 0){
    if(shm != NULL){
      free(shm->alerts);
      free(shm->sensors);
    }
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    sem_close(shm_sem);
    sem_unlink("SHM_SEM");
  }
  logger("HOME_IOT SIMULATOR CLOSING");
  fclose(log_file);
  sem_close(log_sem);
  sem_unlink("LOG_SEM");
  exit(1);
}


//! fazer com que os worker parem de trabalhar usar
//! fechar a pipe par eles pararem
void worker(int num){
  char message[256];
  sprintf(message, "WORKER %d READY", num);
  logger(message);
  sprintf(message, "WORKER %d EXITING", num);
  logger(message);
}

void alert_watcher(){
  logger("ALERT WATCHER READY");
  logger("ALERT WATCHER EXITING");
}

void *sensor_reader(){
  char message[256];
  fd_set read_fd;
  int n;

  logger("THREAD SENSOR_READER CREATED");   

  while (1){
    FD_ZERO(&read_fd);
    FD_SET(fd_sensor, &read_fd);

    if(select(fd_sensor+1, &read_fd, NULL, NULL, NULL)<0){
      error("selecting SENSOR_PIPE");
    }
    n = read(fd_sensor, &message, sizeof(message));
    message[n] = '\0';
    logger(message); //! change to add to quque and write when queue is full

  }
  logger("THREAD SENSOR_READER EXITING");
  pthread_exit(NULL);
} 

void *console_reader(){
  char message[256];
  command_t command;
  fd_set read_fd;

  logger("THREAD CONSOLE_READER CREATED");

  while (1){
    FD_ZERO(&read_fd);
    FD_SET(fd_console, &read_fd);
    if(select(fd_console+1, &read_fd, NULL, NULL, NULL)<0){
      error("selecting CONSOLE_PIPE");
    }
    read(fd_console, &command, sizeof(command_t));
    sprintf(message, "COMMAND RECEIVED: %s ID: %s KEY: %s", command.cmd, command.alert.id, command.alert.key);
    logger(message); //! change to add to quque and write when queue is full
  }
  logger("THREAD CONSOLE_READER EXITING");
  pthread_exit(NULL);
}

void *dispatcher(){
  logger("THREAD DISPATCHER CREATED");
  logger("THREAD DISPATCHER EXITING");
  pthread_exit(NULL);
}

void read_config_file(char *file_name){

  config_file = fopen(file_name, "r");
  /* Exits if no file is found*/
  if(config_file == NULL){
    error("No configuration file found");
  }
   
  char line[256];
  int i;
  for (i = 0; i < 5; i++) {
    fgets(line, 256, config_file);
    if((sscanf(line, "%d", &config[i]) != 1) || (config[i] <= 0 && i < 4) || config[i] < 0){
      error("Wrong file format");
    }
  }
  fclose(config_file);
  config_file = NULL;
}

void pipes_initializer(){
  if ((mkfifo("SENSOR_PIPE", O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)){
    error("creating SENSOR_PIPE");
  }

  if ((fd_sensor = open("SENSOR_PIPE", O_RDWR))<0){
    error("opening SENSOR_PIPE");
  }

  if ((mkfifo("CONSOLE_PIPE", O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)){
    error("creating CONSOLE_PIPE");
  }

  if((fd_console = open("CONSOLE_PIPE", O_RDWR))<0){
    error("opening CONSOLE_PIPE");
  }

}

void ctrlc_handler(){
  int i;
  if(getpid() == parent_pid){
    printf("\n");
    logger("SIGNAL SIGINT RECEIVED");
    logger("HOME_IOT SIMULATOR FREEING RESOURCES");
    
    if(config_file != NULL){
      fclose(config_file);
    }
    pthread_cancel(sensor_reader_t);
    pthread_cancel(console_reader_t);
    pthread_cancel(dispatcher_t);
    pthread_join(sensor_reader_t, NULL);
    pthread_join(console_reader_t, NULL);
    pthread_join(dispatcher_t, NULL);
    for (i = 0; i < config[1]; i++) {
      wait(NULL);
    }

    if(shmid != 0){
      sem_post(shm_sem);
      shmdt(shm);
      shmctl(shmid, IPC_RMID, NULL);
      sem_close(shm_sem);
      sem_unlink("SHM_SEM");
    }
    close(fd_sensor);
    close(fd_console);
    unlink("SENSOR_PIPE");
    unlink("CONSOLE_PIPE");
    logger("HOME_IOT SIMULATOR CLOSING");
    fclose(log_file);
    sem_close(log_sem);
    sem_unlink("LOG_SEM");
    exit(0);
  }
}

/* Open the log.txt file, gives as error if it is not possible*/
void log_initializer(){

  log_file = fopen("log.txt", "w");
  
  if(log_file == NULL){
    printf("ERROR: Not possible to open file log.txt\n");
    exit(1);
  }

  /* Creates and opens the semaphore */
  log_sem = sem_open("LOG_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(log_sem == SEM_FAILED){
    printf("ERROR: Not possible to create semaphore\n");
    exit(1);
  }
}

int main (int argc, char *argv[]){
  int i, pid;

  parent_pid = getpid();

  log_initializer();
  
  if(argc != 2){
    error("Use format: ./home_io <file_name>");
  }

  // Initialize the signal handler
  struct sigaction ctrlc;
  ctrlc.sa_handler = ctrlc_handler;
  sigfillset(&ctrlc.sa_mask);
  ctrlc.sa_flags = 0;
  sigaction(SIGINT, &ctrlc, NULL);

  logger("HOME_IOT SIMULATOR STARTING");

  read_config_file(argv[1]);

  /* Creates the shared memory */
  shmid = shmget(IPC_PRIVATE, sizeof(alert)*config[4] +sizeof(sensor)*config[3], IPC_CREAT | 0777); //! put headers
  if(shmid == -1){
    error("Not able to create shared memory");
  }
  shm = (shared_memory*)shmat(shmid, NULL, 0);
  shm_sem = sem_open("SHM_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_sem == SEM_FAILED){
    error("Not able to create semaphore");
  }

  //Pointing to sahred memory
  shm->alerts = (alert*)(((void*)shm) + sizeof(shared_memory));
  shm->sensors = (sensor*)(((void*)shm->alerts) + sizeof(alert)*config[4]); //! change this
  
  pipes_initializer();

  //Create workers
  for (i = 0; i < config[1]+1; i++) {
    pid = fork();
    if(pid == -1)
      error("Not able to create workers");
    if(pid == 0){
      if(i == 0){
        alert_watcher();
      }else{
        worker(i);
      }
      exit(0);
    }
  }

  /* Creates the threads */  
  if(pthread_create(&sensor_reader_t, NULL, sensor_reader, NULL) != 0){
    error("Not able to create thread sensor_reader");
  }
  if(pthread_create(&console_reader_t, NULL, console_reader, NULL) != 0){
    error("Not able to create thread console_reader");
  }
  if (pthread_create(&dispatcher_t, NULL, dispatcher, NULL) != 0){
    error("Not able to create thread dispatcher");
  }

  logger("HOME_IOT SIMULATOR WATING");

  pause();

  return 0;
}
