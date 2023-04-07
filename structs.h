/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/

#ifndef STRUCTS_H
#define STRUCTS_H

typedef struct INTERNAL_QUEUE{
  int key;
  int sensor_id;
  int value;
  struct INTERNAL_QUEUE *next;
} INTERNAL_QUEUE;

typedef struct alert{
    char id[33];
    char key[33];
    int min;
    int max;
}alert;

typedef struct sensor{
    char id[33];
    char key[33];
    int min;
    int max;
    int avg;
    int count;
}sensor;

typedef struct shared_memory{
    alert *alerts;
    sensor *sensors;
}shared_memory;

#endif
