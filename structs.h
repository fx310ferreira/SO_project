/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/

#ifndef STRUCTS_H
#define STRUCTS_H

#define STR 33

typedef struct INTERNAL_QUEUE{
  int key;
  int sensor_id;
  int value;
  struct INTERNAL_QUEUE *next;
} INTERNAL_QUEUE;

typedef struct alert{
    char id[STR];
    char key[STR];
    int min;
    int max;
}alert;

typedef struct sensor{
    char id[STR];
    char key[STR];
    int min;
    int max;
    int last;
    int sum;
    int count;
}sensor;

typedef struct shared_memory{
    int num_sensors;
    int num_alerts;
    alert *alerts;
    sensor *sensors;
}shared_memory;

typedef struct command_t{
  int console_id;
  char cmd[STR];
  alert alert;
}command_t;

#endif
