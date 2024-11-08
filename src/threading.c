#include "http_server.h"
#include <pthread.h>
#include <stdlib.h>

TaskQueue new_task_queue() {
  TaskQueue q;
  q.head = NULL;
  q.tail = NULL;
  return q;
}

void enqueue_task(TaskQueue *queue, Task *task) {
  if (queue->tail) {
    queue->tail->next = task;
    queue->tail = task;
  } else
    queue->tail = task;

  if (queue->head == NULL)
    queue->head = task;
}

Task *dequeue_task(TaskQueue *queue) {

  if (queue->head) {
    Task *ret = NULL;
    ret = queue->head;
    queue->head = queue->head->next;

    if (queue->head == NULL)
      queue->tail = NULL;

    return ret;
  }

  return NULL;
}

Task *new_task() {
  Task *task = malloc(sizeof(Task));
  task->next = NULL;
  return task;
}
