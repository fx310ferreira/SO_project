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
#include "functions.h"

long unsigned int count;
int t = 0, fd_sensor;

void error(char* error_msg){
  printf("ERROR: %s\n", error_msg);
  close(fd_sensor);
  exit(0);
}

void ctrlz_handler(){
  printf("\n%lu messages printed since the start\n", count);
}

void cleanup(){
  close(fd_sensor);
  exit(0);
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
  // Changing action of SIGTSTP
  struct sigaction ctrlz;
  ctrlz.sa_handler = ctrlz_handler;
  sigfillset(&ctrlz.sa_mask);
  ctrlz.sa_flags = 0;
  sigaction(SIGTSTP, &ctrlz, NULL);

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

int main (int argc, char *argv[])
{
  int min_value, max_value, time_intreval, reading;
  char msg[128];
  count = 0;

  // Parater validation
  if(argc != 6){
    error("Use format ./sensor {ID} {time interval} {key} {min_val} {max_val}\n");
  }

  if(strlen(argv[1]) < 3 || strlen(argv[1]) > 32){
    error("ID size must be between 3 and 32");
  }else if (!str_validator(argv[1], 0)){
    error("ID characters must be alphanumeric");
  }
  

  if(sscanf(argv[2], "%d", &time_intreval) != 1 || time_intreval < 0){
    error("Time intreval must be >= 0");
  }

  if(strlen(argv[3]) < 3 || strlen(argv[3]) > 32){
    error("Key size must be between 3 and 32");
  }else if(!str_validator(argv[3], 1)){
    error("Key characters must be alphanumeric or '_'");
  }   

  if(sscanf(argv[4], "%d", &min_value) != 1){
    error("Min value must be a integer");
  }

  if(sscanf(argv[5], "%d", &max_value) != 1){
    error("Max value must be a integer");
  }
  
  if(max_value < min_value){
    error("Min value must be smaller than max value");
  }

  signal_setup();

  srand(time(NULL));

  if ((fd_sensor = open("SENSOR_PIPE", O_WRONLY)) < 0) {
    error("Error opening SENSOR_PIPE");
  }

  while (1) {
    sleep(t);
    reading = rand()%(max_value-min_value+1)+min_value;
    sprintf(msg, "%s#%s#%d", argv[1], argv[3], reading);
    write(fd_sensor, msg, strlen(msg)+1);
    count ++;
    t = sleep(time_intreval); //! ctrlz stops the sleep 
  } 

  return 0;
}
