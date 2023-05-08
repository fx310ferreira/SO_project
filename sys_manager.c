/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/
#define DEBUG 0

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
#include "internal_queue.h"

/* Variables read from file 0: QUEUE_SZ, 1: N_WORKERS, 2: MAX_KEYS, 3: MAX_SENSORS, 4: MAX_ALERTS */
int config[5];
FILE *config_file;
FILE *log_file;
pthread_t sensor_reader_t, console_reader_t, dispatcher_t;
sem_t *log_sem, *shm_sem, *worker_sem, *sensor_sem, *console_sem;
shared_memory *shm;
int fd_sensor, fd_console;
int shmid = 0, parent_pid;
internal_queue *queue;
pthread_mutex_t queue_mutex;
pthread_cond_t queue_empty_cond, queue_full_cond;
int **workers_fd;
int fds[2];

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
// ! change the code so that sensors close when home iot closes
// ! close the unamed pipes
/* Function to handle errors*/
void error(char *error_msg){
  int i;
  printf("ERROR: %s\n", error_msg);

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

  // for(i = 0; i < config[1]; i++){
  //   close(workers_fd[i][0]);
  //   close(workers_fd[i][1]);
  // }
  // free(workers_fd);

  logger("HOME_IOT waiting for workers to finish");
  for (i = 0; i < config[1]; i++) {
    wait(NULL);
  }

  if(shmid != 0){
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
  sem_close(worker_sem);
  sem_unlink("WORKER_SEM");
  sem_close(sensor_sem);
  sem_unlink("SENSOR_SEM");
  sem_close(console_sem);
  sem_unlink("CONSOLE_SEM");
  pthread_mutex_destroy(&queue_mutex);
  clear_queue(queue);
  free(queue);
  pthread_cond_destroy(&queue_full_cond);
  pthread_cond_destroy(&queue_empty_cond);
  exit(1);
}

//! fazer com que os worker parem de trabalhar usar
//! fechar a pipe para eles pararem
void worker(int id){
  char message[256];
  int n = 1;
  worker_job job;

  sprintf(message, "WORKER %d READY", id);
  logger(message);

  if(read(workers_fd[0][0], &n, sizeof(int)) <= 0){
    printf("WORKER %d EXITING\n", id);
    return;
  }
  printf("WORKER %d RECEIVED %d\n", id, n);

  sprintf(message, "WORKER %d EXITING", id);
  logger(message);
}

void alert_watcher(){
  logger("ALERT WATCHER READY");
  logger("ALERT WATCHER EXITING");
}

void *sensor_reader(){
  char message[256], error_msg[290];
  int n;

  logger("THREAD SENSOR_READER CREATED");   

  while (1){
    if((n = read(fd_sensor, &message, sizeof(message))) <= 0){
      pthread_exit(NULL);
    }
    sem_post(sensor_sem);
    message[n] = '\0';
    pthread_mutex_lock(&queue_mutex);
    #ifdef DEBUG
    printf("MESSAGE RECEIVED: %s\n", message);
    #endif
    if(queue->size < config[0]){
      insert_node(queue, &message, 1);
      pthread_cond_signal(&queue_empty_cond);
    }
    else{
      sprintf(error_msg, "INTERNAL_QUEUE FULL: %s", message);
      logger(error_msg);
    }
    pthread_mutex_unlock(&queue_mutex);
  }
  pthread_exit(NULL);
} 

void *console_reader(){
  char message[256];
  command_t command;
  fd_set read_fd;

  logger("THREAD CONSOLE_READER CREATED");

  while (1){
    if(read(fd_console, &command, sizeof(command_t)) <= 0){
      pthread_exit(NULL);
    }
    sem_post(console_sem);
    sprintf(message, "COMMAND RECEIVED: %s ID: %s KEY: %s", command.cmd, command.alert.id, command.alert.key);
    pthread_mutex_lock(&queue_mutex);
    if(queue->size < config[0]){
      insert_node(queue, &command, 0);
      pthread_cond_signal(&queue_empty_cond);
    }else{
      while (queue->size >= config[0]){
        #ifdef DEBUG
        printf("QUEUE FULL\n");
        #endif
        pthread_cond_wait(&queue_full_cond, &queue_mutex);
      }
      #ifdef DEBUG
      printf("QUEUE NOT FULL inserting\n");
      #endif
      insert_node(queue, &command, 0);
      pthread_cond_signal(&queue_empty_cond);
    }
    pthread_mutex_unlock(&queue_mutex);
  }
  pthread_exit(NULL);
}

void *dispatcher(){
  worker_job job;
  logger("THREAD DISPATCHER CREATED");
  int n = 1;
  while (1){
    sleep(1);
    write(workers_fd[0][1], &n, sizeof(int));
    pthread_mutex_lock(&queue_mutex);
    if(queue->size > 0){
      if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL))
        error("setting cancel state");
      if(queue->alert_head != NULL){
        #ifdef DEBUG
        printf("ALERT: %s\n", queue->alert_head->command.cmd);
        #endif
        job.type = 0;
        memcpy(&job.command, &queue->alert_head->command, sizeof(command_t)); //* check if this is working
        remove_node(queue, 0);
      }else{
        #ifdef DEBUG
        printf("SENSOR: %s\n", queue->sensor_head->sensor);
        #endif
        job.type = 1;
        strcpy(job.sensor, queue->sensor_head->sensor);
        remove_node(queue, 1);
      }
      pthread_cond_signal(&queue_full_cond);
      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }else{
      #ifdef DEBUG
      printf("QUEUE EMPTY\n");
      #endif
      pthread_cond_wait(&queue_empty_cond, &queue_mutex);
    }
    pthread_mutex_unlock(&queue_mutex);
  }
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

void sync_creator(){
  int i;

  /* Creates the sem for the internal queue */
  shm_sem = sem_open("SHM_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_sem == SEM_FAILED){
    error("Not able to create semaphore");
  }

    /* Creates the sem for the workers */
  worker_sem = sem_open("WORKER_SEM", O_CREAT | O_EXCL, 0700, 0);
  if(worker_sem == SEM_FAILED){
    error("Not able to create semaphore");
  }

  sensor_sem = sem_open("SENSOR_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(sensor_sem == SEM_FAILED){
    error("Not able to create semaphore");
  }

  console_sem = sem_open("CONSOLE_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(console_sem == SEM_FAILED){
    error("Not able to create semaphore");
  }

  /* Allocates memory for workers semaphores */
  workers_fd = malloc(sizeof(int*)*config[1]);
  for(i = 0; i < config[1]; i++){
    workers_fd[i] = malloc(sizeof(int)*2);
  }

  /* Creates the semaphore for the workers */
  for(i = 0; i < config[1]; i++){
    if(pipe(workers_fd[i]) < 0){
      error("Not able to create pipe");
    }
  }

  pipe(fds);

  /* Creates the mutex for the internal queue */
  if(pthread_mutex_init(&queue_mutex, NULL) != 0){
    error("Not able to create mutex");
  }

  /* Creates the condition variable */
  if(pthread_cond_init(&queue_empty_cond, NULL) != 0){
    error("Not able to create condition variable");
  }

    /* Creates the condition variable */
  if(pthread_cond_init(&queue_full_cond, NULL) != 0){
    error("Not able to create condition variable");
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
    
    // for(i = 0; i < config[1]; i++){
    //   close(workers_fd[i][0]);
    //   close(workers_fd[i][1]);
    // }
    // free(workers_fd);

    logger("HOME_IOT waiting for workers to finish");
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
    sem_close(worker_sem);
    sem_unlink("WORKER_SEM");
    sem_close(sensor_sem);
    sem_unlink("SENSOR_SEM");
    sem_close(console_sem);
    sem_unlink("CONSOLE_SEM");
    pthread_mutex_destroy(&queue_mutex);
    print_queue(queue);
    clear_queue(queue);
    free(queue);
    pthread_cond_destroy(&queue_full_cond);
    pthread_cond_destroy(&queue_empty_cond);
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

  #ifdef DEBUG
  sem_unlink("LOG_SEM");
  sem_unlink("SHM_SEM");
  sem_unlink("WORKER_SEM");
  sem_unlink("SENSOR_SEM");
  sem_unlink("CONSOLE_SEM");
  #endif

  // Initialize the signal handler
  struct sigaction ctrlc;
  ctrlc.sa_handler = ctrlc_handler;
  sigfillset(&ctrlc.sa_mask);
  ctrlc.sa_flags = 0;
  sigaction(SIGINT, &ctrlc, NULL);

  parent_pid = getpid();

  log_initializer();
  
  if(argc != 2){
    error("Use format: ./home_io <file_name>");
  }

  sync_creator();

  logger("HOME_IOT SIMULATOR STARTING");

  read_config_file(argv[1]);

  //Creates the internal queue
  queue = create_queue();

  /* Creates the shared memory */
  shmid = shmget(IPC_PRIVATE, sizeof(shared_memory)+sizeof(alert)*config[4] + sizeof(char)*STR*config[3] + sizeof(key)*config[2]+sizeof(int)*config[1], IPC_CREAT | 0777); //! put headers
  if(shmid == -1){
    error("Not able to create shared memory");
  }
  shm = (shared_memory*)shmat(shmid, NULL, 0);

  //Pointing to shared memory
  shm->alerts = (alert*)(((void*)shm) + sizeof(shared_memory));
  shm->keys = (key*)(((void*)shm->alerts) + sizeof(alert)*config[4]); //! change this
  shm->workers = (int*)(((void*)shm->keys) + sizeof(key)*config[2]); 
  // shm->sensors = (char**)(((void*)shm->keys) + sizeof(key)*config[2]);

  pipes_initializer();

  //Create workers
  for (i = 0; i < config[1]+1; i++) {
    pid = fork();
    if(pid == -1)
      error("Not able to create workers");
    if(pid == 0){
      /* Blocks all signals */
      sigset_t mask;
      sigfillset(&mask);
      sigprocmask(SIG_SETMASK, &mask, NULL);
      if(i == config[1]){
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
