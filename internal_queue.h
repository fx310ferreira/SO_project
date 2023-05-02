/*
  Frederico Ferreira 2021217116
  Nuno Carvalho do Nascimento 2020219249
*/
#ifndef INTERNAL_QUEUE_H
#define INTERNAL_QUEUE_H

#include "structs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

internal_queue* create_queue(){
  internal_queue *queue = (internal_queue*) malloc(sizeof(internal_queue));
  queue->alert_head = NULL;
  queue->alert_tail = NULL;
  queue->sensor_head = NULL;
  queue->sensor_tail = NULL;
  queue->size = 0;
  return queue;
}

void insert_node(internal_queue *queue, void* prop, int type){
    if(type == 0){ // Alert
        alert_node *new_node = (alert_node*) malloc(sizeof(alert_node));
        new_node->command = *(command_t*) prop;
        new_node->next = NULL;
        if(queue->alert_head == NULL){
        queue->alert_head = new_node;
        queue->alert_tail = new_node;
        }else{
        queue->alert_tail->next = new_node;
        queue->alert_tail = new_node;
        }
    }else{ // Sensor
        sensor_node *new_node = (sensor_node*) malloc(sizeof(sensor_node));
        strcpy(new_node->sensor, (char*) prop);
        new_node->next = NULL;
        if(queue->sensor_head == NULL){
        queue->sensor_head = new_node;
        queue->sensor_tail = new_node;
        }else{
        queue->sensor_tail->next = new_node;
        queue->sensor_tail = new_node;
        }
    }
    queue->size++;
}

void remove_node(internal_queue *queue, int type){
    if(type == 0){ // Alert
        alert_node *aux = queue->alert_head;
        queue->alert_head = queue->alert_head->next;
        free(aux);
    }else{ // Sensor
        sensor_node *aux = queue->sensor_head;
        queue->sensor_head = queue->sensor_head->next;
        free(aux);
    }
    queue->size--;
}

void clear_queue(internal_queue *queue){
    while(queue->alert_head != NULL){
        remove_node(queue, 0);
    }
    while (queue->sensor_head != NULL){
        remove_node(queue, 1);
    }
}

void print_queue(internal_queue *queue){
    alert_node *aux = queue->alert_head;
    sensor_node *aux2 = queue->sensor_head;
    printf("Alerts:\n");
    while(aux != NULL){
        printf("%s %s %d %d\n",aux->command.cmd, aux->command.alert.key, aux->command.alert.min, aux->command.alert.max);
        aux = aux->next;
    }
    printf("Sensors:\n");
    while(aux2 != NULL){
        printf("%s\n", aux2->sensor);
        aux2 = aux2->next;
    }
}

#endif