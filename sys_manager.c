#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

FILE *config_file;
/* Variables read from file 0: QUEUE_SZ, 1: N_WORKERS, 2: MAX_KEYS, 3: MAX_SENSORS, 4: MAX_ALERTS */
int config[5];
/* Function to handle errors*/
void error(char *error_msg){
  if(config_file != NULL){
    fclose(config_file);
  }
  printf("ERROR: %s\n", error_msg);
  exit(1);
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

int main (int argc, char *argv[]){
  if(argc != 2){
    error("Use format: ./home_io <file_name>");
  } 
  read_config_file(argv[1]); 

  return 0;
}


