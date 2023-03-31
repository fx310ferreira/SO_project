#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

long int count;

void error(char* error_msg){
  printf("ERROR: %s\n", error_msg);
  exit(0);
}

void ctrlz_handler(){
  printf("\n%ld messages printed since the start\n", count);
}

int main (int argc, char *argv[])
{
  int min_value, max_value, time_intreval, reading;
  count = 0;
  // Changing action of SIGTSTP
  struct sigaction ctrlz;
  ctrlz.sa_handler = ctrlz_handler;
  sigfillset(&ctrlz.sa_mask);
  ctrlz.sa_flags = 0;
  sigaction(SIGTSTP, &ctrlz, NULL);
   
  /* Parater validation*/
  if(argc != 6){
    error("Use format ./sensor {ID} {time interval} {key} {min_val} {max_val}\n");
  }

  if(strlen(argv[1]) < 3 || strlen(argv[1]) > 32){
    error("ID size must be between 3 and 32");
  }
  

  if(sscanf(argv[2], "%d", &time_intreval) != 1 || time_intreval < 0){
    error("Time intreval must be >= 0");
  }

  if(strlen(argv[3]) < 3 || strlen(argv[3]) > 32){
    error("Key size must be between 3 and 32");
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
  srand(time(NULL));
  while (1) {
    reading = rand()%(max_value-min_value+1)+min_value;
    //Comment and replace for final euvaluation
    printf("%s#%s#%d\n", argv[1], argv[3], reading);
    count ++;
    sleep(time_intreval); 
  } 

  return 0;
}
