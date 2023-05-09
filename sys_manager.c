/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/
// #define DEBUG 0 

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
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
sem_t *log_sem, *worker_sem, *shm_worker_sem, *shm_key_sem, *shm_sensor_sem, *shm_new_alert_sem, *shm_alert_sem, *msg_queue_sem, *alert_watcher_sem, *alert_sem;
shared_memory *shm;
int fd_sensor, fd_console;
int shmid = 0, parent_pid;
internal_queue *queue;
pthread_mutex_t queue_mutex;
pthread_cond_t queue_empty_cond, queue_full_cond;
int (*workers_fd)[2];
int msgqid;
int leave = 0;
int threads = 0;

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

void print_queue(internal_queue *queue){
    char msg[2057]; 
    char msg_aux[STR*4+10];
    alert_node *aux = queue->alert_head;
    sensor_node *aux2 = queue->sensor_head;
    sprintf(msg, "Queue:\n");
    while(aux != NULL){
        sprintf(msg_aux, "\t\t%s %s %d %d\n",aux->command.cmd, aux->command.alert.key, aux->command.alert.min, aux->command.alert.max);
        strcat(msg, msg_aux);
        aux = aux->next;
    }
    sprintf(msg_aux, "\tSensors:\n");
    strcat(msg, msg_aux);
    while(aux2 != NULL){
        sprintf(msg_aux, "\t\t%s\n", aux2->sensor);
        strcat(msg, msg_aux);
        aux2 = aux2->next;
    }
    logger(msg);
}

/* Function to handle errors*/
void error(char *error_msg){
  int i;
  printf("ERROR: %s\n", error_msg);

  logger("HOME_IOT SIMULATOR FREEING RESOURCES");

  if(config_file != NULL){
    fclose(config_file);
  }
  if(threads == 3){
    pthread_cancel(sensor_reader_t);
    pthread_cancel(console_reader_t);
    pthread_cancel(dispatcher_t);
    pthread_join(sensor_reader_t, NULL);
    pthread_join(console_reader_t, NULL);
    pthread_join(dispatcher_t, NULL);
  }

  for(i = 0; i < config[1]; i++){
    close(workers_fd[i][0]);
    close(workers_fd[i][1]);
  }

  logger("HOME_IOT waiting for workers to finish");
  for (i = 0; i < config[1]+1; i++) {
    wait(NULL);
  }

  free(workers_fd);

  shmdt(shm);
  shmctl(shmid, IPC_RMID, NULL);
  sem_close(shm_worker_sem);
  sem_unlink("SHM_WORKER_SEM");
  sem_close(shm_alert_sem);
  sem_unlink("SHM_ALERT_SEM");
  sem_close(shm_sensor_sem);
  sem_unlink("SHM_SENSOR_SEM");
  sem_close(shm_key_sem);
  sem_unlink("SHM_KEY_SEM");
  sem_close(msg_queue_sem);
  sem_unlink("MSG_QUEUE_SEM");
  sem_close(alert_sem);
  sem_unlink("ALERT_SEM");
  sem_close(alert_watcher_sem);
  sem_unlink("ALERT_WATCHER_SEM");
  sem_close(shm_new_alert_sem);
  sem_unlink("SHM_NEW_ALERT_SEM");
  close(fd_sensor);
  unlink("SENSOR_PIPE");
  close(fd_console);
  unlink("CONSOLE_PIPE");
  sem_close(msg_queue_sem);
  sem_unlink("MSG_QUEUE_SEM");
  msgctl(msgqid, IPC_RMID, NULL);
  logger("HOME_IOT SIMULATOR CLOSING");
  fclose(log_file);
  sem_close(log_sem);
  sem_unlink("LOG_SEM");
  sem_close(worker_sem);
  sem_unlink("WORKER_SEM");
  pthread_mutex_destroy(&queue_mutex);
  if(queue != NULL)
    clear_queue(queue);
  free(queue);
  pthread_cond_destroy(&queue_full_cond);
  pthread_cond_destroy(&queue_empty_cond);
  exit(1);
}

void worker(int id){
  char message[256];
  msg_queue_msg msg;
  worker_job job;
  char sens_id[STR], key[STR];
  int num;
  int new_key, new_sensor, key_i;
  int i;
  close(workers_fd[id][1]);

  sprintf(message, "WORKER %d READY", id);
  logger(message);
  while(!leave){
    sem_wait(shm_worker_sem);
    shm->workers[id] = 1;
    sem_post(shm_worker_sem);
    sem_post(worker_sem);
    sprintf(message, "WORKER %d WAITING FOR JOB", id);
    logger(message);
    if(read(workers_fd[id][0], &job, sizeof(worker_job)) <= 0){
      #ifdef DEBUG
      printf("\nWORKER %d EXITING\n", id);
      #endif
      return;
    }
    sprintf(message, "WORKER %d RECEIVED JOB", id);
    logger(message);
    if(job.type){ // 1 = sensor
      sscanf(job.sensor, "%[^#]#%[^#]#%d", sens_id, key, &num);
      new_key = new_sensor = 1;
      sem_wait(shm_sensor_sem);
      for(i = 0; i < shm->num_sensors; i++) if(!strcmp(shm->sensors[i].id, sens_id)) new_sensor = 0;
      if(new_sensor && shm->num_sensors < config[3]){
        strcpy(shm->sensors[shm->num_sensors].id, sens_id);
        shm->num_sensors++;
      }else if(new_sensor){
        sprintf(message, "WORKER %d: MAX NUMBER OF SENSORS REACHED", id);
        logger(message);
        sem_post(shm_sensor_sem);
        continue;
      }
      sem_post(shm_sensor_sem);

      sem_wait(shm_key_sem);
      for(i = 0; i < shm->num_keys; i++) if(!strcmp(shm->keys[i].key, key)) {
        new_key = 0;
        key_i = i;
        }
      if(new_key && shm->num_keys < config[2]){
        strcpy(shm->keys[shm->num_keys].key, key);
        shm->keys[shm->num_keys].last = num;
        shm->keys[shm->num_keys].sum = num;
        shm->keys[shm->num_keys].min = num;
        shm->keys[shm->num_keys].max = num;
        shm->keys[shm->num_keys].count = 1; 
        shm->num_keys++;
        sem_wait(shm_new_alert_sem);
        strcpy(shm->new_alert.key, key);
        shm->new_alert.value = num;
        sem_post(shm_new_alert_sem);
        sem_post(alert_watcher_sem);

      }else if(new_key){
        sprintf(message, "WORKER %d: MAX NUMBER OF KEYS REACHED", id);
        logger(message);
        sem_post(shm_key_sem);
        continue;
      }else{
        shm->keys[key_i].last = num;
        shm->keys[key_i].sum += num;
        shm->keys[key_i].count++;
        if(num < shm->keys[key_i].min) shm->keys[key_i].min = num;
        if(num > shm->keys[key_i].max) shm->keys[key_i].max = num;
        sem_wait(shm_new_alert_sem);
        strcpy(shm->new_alert.key, key);
        shm->new_alert.value = num;
        sem_post(shm_new_alert_sem);
        sem_post(alert_watcher_sem);
      }
      sem_post(shm_key_sem);
    }else{ // 0 = console
      msg.msgtype = job.command.console_id;
      if(strcmp(job.command.cmd, "stats")==0){
        strcpy(msg.msg, "Key\tLast\tMin\tMax\tAvg\tCount\n");
        sem_wait(shm_key_sem);
        for(i = 0; i < shm->num_keys; i++){
          sprintf(message, "%s\t%d\t%d\t%d\t%.2f\t%d\n", shm->keys[i].key, shm->keys[i].last, shm->keys[i].min, shm->keys[i].max, shm->keys[i].sum/(float)shm->keys[i].count, shm->keys[i].count);
          strcat(msg.msg, message);
        }
        sem_post(shm_key_sem);
        msgsnd(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), 0);
        sem_post(msg_queue_sem);
      }else if(strcmp(job.command.cmd, "sensors")==0){
        strcpy(msg.msg, "ID\n");
        sem_wait(shm_sensor_sem);
        for(i = 0; i < shm->num_sensors; i++){
          sprintf(message, "%s\n", shm->sensors[i].id);
          strcat(msg.msg, message);
        }
        sem_post(shm_sensor_sem);
        msgsnd(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), 0);
        sem_post(msg_queue_sem);
      }else if(strcmp(job.command.cmd, "list_alerts")==0){
        strcpy(msg.msg, "ID\tKey\tMIN\tMAX\n");
        sem_wait(shm_alert_sem);
        for(i = 0; i < shm->num_alerts; i++){
          sprintf(message, "%s\t%s\t%d\t%d\n", shm->alerts[i].id, shm->alerts[i].key, shm->alerts[i].min, shm->alerts[i].max);
          strcat(msg.msg, message);
        }
        sem_post(shm_alert_sem);
        msgsnd(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), 0);
        sem_post(msg_queue_sem);
      }else if(strcmp(job.command.cmd, "reset")==0){
        sem_wait(shm_key_sem);
        shm->num_keys = 0;
        sem_post(shm_key_sem);
        sem_wait(shm_sensor_sem);
        shm->num_sensors = 0;
        sem_post(shm_sensor_sem);
        strcpy(msg.msg, "RESET");
        msgsnd(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), 0);
        sem_post(msg_queue_sem);
      }else if(strcmp(job.command.cmd, "remove_alert")==0){
        int removed = 0;
        sem_wait(shm_alert_sem);
        for (int i = 0; i < shm->num_alerts; i++){
          if(strcmp(shm->alerts[i].id, job.command.alert.id)==0){
            shm->alerts[i] = shm->alerts[shm->num_alerts-1];
            shm->num_alerts--;
            removed = 1;
            break;
          }
        }
        if(removed){
          strcpy(msg.msg, "ALERT REMOVED");
        }else{
          strcpy(msg.msg, "ALERT NOT FOUND");
        }
        sem_post(shm_alert_sem);
        msgsnd(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), 0);
        sem_post(msg_queue_sem);
      }else if(strcmp(job.command.cmd, "add_alert")==0){
        int exists = 0;
        sem_wait(shm_alert_sem);
        for (int i = 0; i < shm->num_alerts; i++){
          if(strcmp(shm->alerts[i].id, job.command.alert.id)==0){
            exists = 1;
            break;
          }
        }
        if(!exists && shm->num_alerts < config[4]){
          shm->alerts[shm->num_alerts] = job.command.alert;
          shm->num_alerts++;
          strcpy(msg.msg, "ALERT ADDED");
        }else if(exists){
          strcpy(msg.msg, "ALERT ALREADY EXISTS");
        }else{
          strcpy(msg.msg, "MAX NUMBER OF ALERTS REACHED");
        }
        sem_post(shm_alert_sem);
        msgsnd(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), 0);
        sem_post(msg_queue_sem);
      }else{
        sprintf(message, "WORKER %d: INVALID COMMAND", id);
        logger(message);
      }
    }
  }

  sprintf(message, "WORKER %d EXITING", id);
  logger(message);
}

void alert_watcher(){
  logger("ALERT WATCHER READY");
  msg_queue_msg msg;
  while (!leave){
    if(sem_wait(alert_watcher_sem) < 0){
      #ifdef DEBUG
      logger("ALERT WATCHER EXITING");
      #endif
      return;
    };
    sem_wait(shm_alert_sem);
    sem_wait(shm_new_alert_sem);
    for (int j = 0; j < shm->num_alerts; j++){
      if(strcmp(shm->new_alert.key, shm->alerts[j].key)==0 && (shm->new_alert.value < shm->alerts[j].max || shm->new_alert.value > shm->alerts[j].min)){
        sprintf(msg.msg, "ALERT: %s %s %d %d", shm->alerts[j].id, shm->alerts[j].key, shm->alerts[j].min, shm->alerts[j].max);
        msg.msgtype = shm->alerts->console_id;
        msgsnd(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), 0);
        sem_post(alert_sem);
        break;
      }
    }
    sem_post(shm_new_alert_sem);
    sem_post(shm_alert_sem);
  }
}

void *sensor_reader(){
  char message[256], error_msg[290];
  int n;
  threads ++;
  logger("THREAD SENSOR_READER CREATED");   

  while (1){
    if((n = read(fd_sensor, &message, sizeof(message))) <= 0){
      pthread_exit(NULL);
    }
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
threads ++;
  logger("THREAD CONSOLE_READER CREATED");

  while (1){
    if(read(fd_console, &command, sizeof(command_t)) <= 0){
      pthread_exit(NULL);
    }
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
  threads ++;
  logger("THREAD DISPATCHER CREATED");
  for (int i = 0; i < config[1]; i++){
    close(workers_fd[i][0]);
  }
  
  while (1){
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
      pthread_mutex_unlock(&queue_mutex);
      continue;
    }
    pthread_mutex_unlock(&queue_mutex);
    #ifdef DEBUG
    printf("DISPATCHER WATING FOR WORKER\n");
    #endif
    sem_wait(worker_sem);
    sem_wait(shm_worker_sem);
    for (int i = 0; i < config[1]; i++){
      if(shm->workers[i] == 1){
        write(workers_fd[i][1], &job, sizeof(worker_job));
        shm->workers[i] = 0;
        break;
      }
    }
    sem_post(shm_worker_sem);
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
  int i;
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

    /* Allocates memory for workers semaphores */
  workers_fd = malloc(sizeof(int)*2*config[1]);

  /* Creates the semaphore for the workers */
  for(i = 0; i < config[1]; i++){
    if(pipe(workers_fd[i]) < 0){
      error("Not able to create pipe");
    }
  }

}

void sync_creator(){

  /* Creates the sem for the shm workers */
  shm_worker_sem = sem_open("SHM_WORKER_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_worker_sem == SEM_FAILED){
    error("Not able to create shm_worker semaphore");
  }

  /* Creates the sem for the shm alerts */
  shm_alert_sem = sem_open("SHM_ALERT_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_alert_sem == SEM_FAILED){
    error("Not able to create shm_alert semaphore");
  }

  /* Creates the sem for the shm sensors */
  shm_sensor_sem = sem_open("SHM_SENSOR_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_sensor_sem == SEM_FAILED){
    error("Not able to create shm_sensor semaphore");
  }

  /* Creates the sem for the shm keys */
  shm_key_sem = sem_open("SHM_KEY_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_key_sem == SEM_FAILED){
    error("Not able to create shm_key semaphore");
  }

    /* Creates the sem for the workers */
  worker_sem = sem_open("WORKER_SEM", O_CREAT | O_EXCL, 0700, 0);
  if(worker_sem == SEM_FAILED){
    error("Not able to create worker semaphore");
  }

  /* Creates the mutex for the msg queue */
  msg_queue_sem = sem_open("MSG_QUEUE_SEM", O_CREAT | O_EXCL, 0700, 0);
  if(msg_queue_sem == SEM_FAILED){
    error("Not able to create msg_queue semaphore");
  }

  alert_sem = sem_open("ALERT_SEM", O_CREAT | O_EXCL, 0700, 0);
  if(alert_sem == SEM_FAILED){
    error("Not able to create alert semaphore");
  }

  alert_watcher_sem = sem_open("ALERT_WATCHER_SEM", O_CREAT | O_EXCL, 0700, 0);
  if(alert_watcher_sem == SEM_FAILED){
    error("Not able to create alert_watcher semaphore");
  }

  shm_new_alert_sem = sem_open("SHM_NEW_ALERT_SEM", O_CREAT | O_EXCL, 0700, 1);
  if(shm_new_alert_sem == SEM_FAILED){
    error("Not able to create shm_new_alert semaphore");
  }

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

  /* Creates the message queue*/
  if((msgqid = msgget(ftok("sys_manager.c", 'b'), IPC_CREAT | 0700)) < 0){
    error("Not able to create message queue");
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

    if(threads == 3){
      pthread_cancel(sensor_reader_t);
      pthread_cancel(console_reader_t);
      pthread_cancel(dispatcher_t);
      pthread_join(sensor_reader_t, NULL);
      pthread_join(console_reader_t, NULL);
      pthread_join(dispatcher_t, NULL);
    }
    
    for(i = 0; i < config[1]; i++){
      close(workers_fd[i][0]);
      close(workers_fd[i][1]);
    }

    logger("HOME_IOT waiting for workers to finish");
    #ifdef DEBUG
    printf("===> %d\n", config[1]);
    #endif
    for (i = 0; i < config[1]+1; i++) {
      wait(NULL);
    }

    free(workers_fd);

    #ifdef DEBUG
    for(i = 0; i < config[3]; i++){
      printf("ID %d: %s\n", i, shm->sensors[i].id);
    }
    #endif

    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    sem_close(shm_worker_sem);
    sem_unlink("SHM_WORKER_SEM");
    sem_close(shm_alert_sem);
    sem_unlink("SHM_ALERT_SEM");
    sem_close(shm_sensor_sem);
    sem_unlink("SHM_SENSOR_SEM");
    sem_close(shm_key_sem);
    sem_unlink("SHM_KEY_SEM");
    sem_close(msg_queue_sem);
    sem_unlink("MSG_QUEUE_SEM");
    sem_close(alert_sem);
    sem_unlink("ALERT_SEM");
    sem_close(alert_watcher_sem);
    sem_unlink("ALERT_WATCHER_SEM");
    sem_close(shm_new_alert_sem);
    sem_unlink("SHM_NEW_ALERT_SEM");
    close(fd_sensor);
    unlink("SENSOR_PIPE");
    close(fd_console);
    unlink("CONSOLE_PIPE");
    msgctl(msgqid, IPC_RMID, NULL);
    print_queue(queue);
    logger("HOME_IOT SIMULATOR CLOSING");
    fclose(log_file);
    sem_close(log_sem);
    sem_unlink("LOG_SEM");
    sem_close(worker_sem);
    sem_unlink("WORKER_SEM");
    pthread_mutex_destroy(&queue_mutex);
    clear_queue(queue);
    free(queue);
    pthread_cond_destroy(&queue_full_cond);
    pthread_cond_destroy(&queue_empty_cond);
    exit(0);
  }else{
    leave = 1;
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
    printf("ERROR: Not possible to create log semaphore\n");
    exit(1);
  }
}

int main (int argc, char *argv[]){
  int i, pid;

  #ifdef DEBUG
  sem_unlink("LOG_SEM");
  sem_unlink("SHM_WORKER_SEM");
  sem_unlink("SHM_ALERT_SEM");
  sem_unlink("SHM_SENSOR_SEM");
  sem_unlink("SHM_KEY_SEM");
  sem_unlink("WORKER_SEM");
  sem_unlink("MSG_QUEUE_SEM");
  sem_unlink("ALERT_SEM");
  sem_unlink("ALERT_WATCHER_SEM");
  sem_unlink("SHM_NEW_ALERT_SEM");
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
  shm->sensors = (sensor*)(((void*)shm->keys) + sizeof(int)*config[1]);

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
      sigdelset(&mask, SIGINT);
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
