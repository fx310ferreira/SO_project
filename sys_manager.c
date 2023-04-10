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
#include "structs.h"
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Variables read from file 0: QUEUE_SZ, 1: N_WORKERS, 2: MAX_KEYS, 3: MAX_SENSORS, 4: MAX_ALERTS */
int config[5];
FILE *config_file;
FILE *log_file;
pthread_t sensor_reader_t, console_reader_t, dispatcher_t;
sem_t *log_sem, *shm_sem;
shared_memory *shm;
int shmid = 0;

/* Fucntion that is responsible for writing logs */
void logger(char *message){
  time_t now = time(NULL);
  struct tm *date_time = localtime(&now);
  sem_wait(log_sem);
  printf("%02d:%02d:%02d %s\n", date_time->tm_hour, date_time->tm_min, date_time->tm_sec, message);
  fprintf(log_file, "%4d/%02d/%02d %02d:%02d:%02d %s\n", date_time->tm_year+1900, date_time->tm_mon, date_time->tm_mday, date_time->tm_hour, date_time->tm_min, date_time->tm_sec, message);
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
  logger("THREAD SENSOR_READER CREATED");
  logger("THREAD SENSOR_READER EXITING");
  pthread_exit(NULL);
} 

void *console_reader(){
  logger("THREAD CONSOLE_READER CREATED");
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

void ctrlc_handler(){
  int i;

  printf("\n");
  logger("SIGNAL SIGINT RECEIVED");
  logger("HOME_IOT SIMULATOR FREEING RESOURCES");
  
  if(config_file != NULL){
    fclose(config_file);
  }

  pthread_join(sensor_reader_t, NULL);
  pthread_join(console_reader_t, NULL);
  pthread_join(dispatcher_t, NULL);
  for (i = 0; i < config[1]; i++) {
    wait(NULL);
  }
  if(shmid != 0){
    sem_wait(shm_sem);
    if(shm != NULL){
      free(shm->alerts);
      free(shm->sensors);
    }
    sem_post(shm_sem);
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    sem_close(shm_sem);
    sem_unlink("SHM_SEM");
  }
  logger("HOME_IOT SIMULATOR CLOSING");
  fclose(log_file);
  sem_close(log_sem);
  sem_unlink("LOG_SEM");
  exit(0);
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
  sem_unlink("SHM_SEM");
  sem_unlink("LOG_SEM");

  log_initializer();
  
  if(argc != 2){
    error("Use format: ./home_io <file_name>");
  }

  struct sigaction ctrlc;
  ctrlc.sa_handler = ctrlc_handler;
  sigfillset(&ctrlc.sa_mask);
  ctrlc.sa_flags = 0;
  sigaction(SIGINT, &ctrlc, NULL);

  logger("HOME_IOT SIMULATOR STARTING");

  read_config_file(argv[1]);

  /* Creates the shared memory */
  shmid = shmget(IPC_PRIVATE, sizeof(shared_memory), IPC_CREAT | 0777);
  if(shmid == -1){
    error("Not able to create shared memory");
  }
  shm = (shared_memory*)shmat(shmid, NULL, 0);
  shm_sem = sem_open("SHM_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_sem == SEM_FAILED){
    error("Not able to create semaphore");
  }

  sem_wait(shm_sem);
  shm->alerts = malloc(sizeof(alert)*config[4]);
  shm->sensors = malloc(sizeof(sensor)*config[3]);
  if(shm->alerts == NULL || shm->sensors == NULL){
    error("Not able to allocate memory");
  }
  sem_post(shm_sem);

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
