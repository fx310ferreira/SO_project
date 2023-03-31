#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

void error(char* error_msg){
  printf("ERROR: %s\n", error_msg);
  exit(0);
}

void stats(){
  return;
}
 void reset(){
  return;
}

void sensors(){
  return;
}

void add_alert(char *id, char* key){
  int min_value, max_value;
  scanf("%s %s %d %d", id, key, &min_value, &max_value);
  if(strlen(id) < 3 || strlen(id) > 32){
    //Exit???
    printf("ID must be between 3 and 32\n");
  }
  return;
}

void remove_alert(){
  return;
}

void list_alerts(){
  return;
}

int main (int argc, char *argv[])
{
  char id[33], key[33];
  /* Parater validation*/
  if(argc != 2){
    error("Use format ./user_console {console identifier}\n"); //What is this id??
  }

  while (strcmp("exit", id) != 0) {
    scanf(" %s", id);
    if(strcmp("stats", id) == 0){
      stats();
    }else if(strcmp("reset", id) == 0){
      reset();
    }else if(strcmp("sensors", id) == 0){
      sensors();
    }else if(strcmp("add_alert", id) == 0) {
      add_alert(id, key);
    }else if(strcmp("remove_alert", id) == 0){
      remove_alert();
    }else if(strcmp("list_alerts", id) == 0){
      list_alerts();
    }
  }

  return 0;
}
