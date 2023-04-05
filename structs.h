#ifndef STRUCTS_H
#define STRUCTS_H

typedef struct INTERNAL_QUEUE{
  int key;
  int sensor_id;
  int value;
  struct INTERNAL_QUEUE *next;
} INTERNAL_QUEUE;

typedef struct shared_memory{
  int in;
}shared_memory;

#endif
