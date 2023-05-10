/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/types.h>
#include "functions.h"
#include "structs.h"

int fd_console;
int console_id, msgqid;
sem_t *alert_sem;
pthread_t alert_thread;

void cleanup(){
  pthread_kill(alert_thread, SIGUSR1);
  pthread_join(alert_thread, NULL);
  exit(0);
}

void error(char* error_msg){
  printf("ERROR: %s\n", error_msg);
  cleanup();
}

void send_comand(command_t* command, char* cmd){
  msg_queue_msg msg;
  strcpy(command->cmd, cmd);
  write(fd_console, command, sizeof(command_t));
  printf("WATING FOR A RESPONSE\n");
  msgrcv(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), console_id, 1);
  printf("RESPONSE RECEIVED\n");
  printf("%s\n", msg.msg);
}

void add_alert(command_t* command, char* cmd){
  int min_value, max_value;
  char id[33], key[33];
  
  if(scanf(" %s %s %d %d", id, key, &min_value, &max_value) != 4){
    printf("Invalid input\n");
  }
  
  if(strlen(id) < 3 || strlen(id) > 32){
    printf("ID size must be between 3 and 32\n");
    return;
  }else if (!str_validator(id, 0)){
    printf("ID characters must be alphanumeric\n");
    return;
  }

  if(strlen(key) < 3 || strlen(key) > 32){
    printf("Key size must be between 3 and 32\n");
    return;
  }else if(!str_validator(key, 1)){
    printf("Key characters must be alphanumeric or '_'\n");
    return;
  }

  if(min_value > max_value){
    printf("Min value must be lower than max value\n");
    return;
  }

  if(min_value < 0 || max_value < 0){
    printf("Min and max values must be positive\n");
    return;
  }

  command->alert.min = min_value;
  command->alert.max = max_value;
  command->alert.console_id = console_id;
  strcpy(command->alert.id, id);
  strcpy(command->alert.key, key);

  send_comand(command, cmd);

  return;
}

void remove_alert(command_t* command, char* cmd){
  char id[33];

  if(scanf(" %s", id) != 1){
    printf("Invalid input\n");
  }

  if(strlen(id) < 3 || strlen(id) > 32){
    printf("ID size must be between 3 and 32\n");
    return;
  }else if (!str_validator(id, 0)){
    printf("ID characters must be alphanumeric\n");
    return;
  }
  
  strcpy(command->alert.id, id);

  send_comand(command, cmd);
  return;
}

void ctrlc_handler(){
  printf("\nSIGINT received\n");
  cleanup();
}

void sigpipe_handler(){
  printf("Error writing to pipe\n");
  cleanup();
}

void signal_setup(){
  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask, SIGINT);
  sigdelset(&mask, SIGPIPE);
  sigdelset(&mask, SIGUSR1);
  sigprocmask(SIG_SETMASK, &mask, NULL);
  
  // Changing action of SIGINT
  struct sigaction ctrlc;
  ctrlc.sa_handler = ctrlc_handler;
  sigfillset(&ctrlc.sa_mask);
  ctrlc.sa_flags = 0;
  sigaction(SIGINT, &ctrlc, NULL);

  // Changing action of SIGPIPE
  struct sigaction sigpipe;
  sigpipe.sa_handler = sigpipe_handler;
  sigfillset(&sigpipe.sa_mask);
  sigpipe.sa_flags = 0;
  sigaction(SIGPIPE, &sigpipe, NULL);
}

void close_handler(){
  pthread_exit(NULL);
}

void* alert_reader(){
  // Changing action of SIGINT
  struct sigaction close_signal;
  close_signal.sa_handler = close_handler;
  sigfillset(&close_signal.sa_mask);
  close_signal.sa_flags = 0;
  sigaction(SIGUSR1, &close_signal, NULL);
  msg_queue_msg msg;

  while(1){
    sem_wait(alert_sem);
    msgrcv(msgqid, &msg, sizeof(msg_queue_msg)-sizeof(long), console_id, 1);
    printf("\n%s\n", msg.msg);
  }
}

int main (int argc, char *argv[]){
  char cmd[33];
  command_t command;
  // Parater validation
  if(argc != 2){
    error("Use format ./user_console {console identifier}\n");
  }

  if(sscanf(argv[1], "%d", &console_id) != 1 || console_id < 0){
    error("Invalid console identifier\n");
  }

  command.console_id = console_id;

  signal_setup();

  if((msgqid = msgget(ftok("sys_manager.c", 'b'), 0)) < 0){
    error("Not able to create message queue");
  }
  
  if ((fd_console = open("CONSOLE_PIPE", O_WRONLY)) < 0) {
    error("Error opening CONSOLE_PIPE");
  }

  if ((alert_sem = sem_open("ALERT_SEM", 0)) == SEM_FAILED) {
    error("Error opening ALERT_SEM");
  }
  
  if(pthread_create(&alert_thread, NULL, alert_reader, NULL)<0){
    error("Error creating alert thread");
  }

  while (strcmp("exit", cmd) != 0) {
    scanf("%s", cmd);
    if(strcmp("stats", cmd) == 0)
      send_comand(&command, cmd);
    else if(strcmp("reset", cmd) == 0)
      send_comand(&command, cmd);
    else if(strcmp("sensors", cmd) == 0)
      send_comand(&command, cmd);
    else if(strcmp("add_alert", cmd) == 0)
      add_alert(&command, cmd);
    else if(strcmp("remove_alert", cmd) == 0)
      remove_alert(&command, cmd);
    else if(strcmp("list_alerts", cmd) == 0)
      send_comand(&command, cmd);
    else if(strcmp("exit", cmd) != 0)
      printf("Invalid command\n");
  }
  return 0;
}
