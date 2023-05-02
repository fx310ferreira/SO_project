/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/

#ifndef STRUCTS_H
#define STRUCTS_H

#define STR 33

typedef struct alert{
    char id[STR];
    char key[STR];
    int console_id;
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

typedef struct alert_node{ // Node for the internal queue
    command_t command;
    struct alert_node *next;
}alert_node;

typedef struct sensor_node{ // Node for the internal queue
    char sensor[4*STR]; // 4*STR because we need to store the sensor id, key, min and max
    struct sensor_node *next;
}sensor_node;

typedef struct internal_queue{
  alert_node *alert_head;
  alert_node *alert_tail;
  sensor_node *sensor_head;
  sensor_node *sensor_tail;
  int size;
} internal_queue;

#endif
