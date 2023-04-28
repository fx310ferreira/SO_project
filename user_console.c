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


void error(char* error_msg){
  printf("ERROR: %s\n", error_msg);
  exit(0);
}


void add_alert(char *id, char* key){
  int min_value, max_value;
  scanf("%s %s %d %d", id, key, &min_value, &max_value);
  if(strlen(id) < 3 || strlen(id) > 32){
    error("ID size must be between 3 and 32");
  }else if (!str_validator(id, 0)){
    error("ID characters must be alphanumeric");
  }

  if(strlen(key) < 3 || strlen(key) > 32){
    error("Key size must be between 3 and 32");
  }else if(!str_validator(key, 1)){
    error("Key characters must be alphanumeric or '_'");
  }
// missing min and max values
  return;
}

void remove_alert(){
  return;
}

void list_alerts(){
  return;
}

int main (int argc, char *argv[]){
  char id[33], key[33];
  int console_id = 0;
  // Parater validation
  if(argc != 2){
    error("Use format ./user_console {console identifier}\n"); //! What is this id??
  }

  if(sscanf(argv[1], "%d", &console_id) != 1 || console_id < 0){
    error("Invalid console identifier\n");
  }

  while (strcmp("exit", id) != 0) {
    scanf("%s", id);
    if(strcmp("stats", id) == 0){
      
    }else if(strcmp("reset", id) == 0){
      
    }else if(strcmp("sensors", id) == 0){
      
    }else if(strcmp("add_alert", id) == 0) {
      add_alert(id, key);
    }else if(strcmp("remove_alert", id) == 0){
      remove_alert();
    }else if(strcmp("list_alerts", id) == 0){
      list_alerts();
    }else{
      printf("Invalid command\n");
    }
  }

  return 0;
}
