#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>

TaskQueue new_task_queue() {
  TaskQueue q;
  q.head = NULL;
  q.tail = NULL;
  q.size = 0;
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

  queue->size++;
  printf("enqueued task, queue size %d\n", queue->size);
}

Task *dequeue_task(TaskQueue *queue) {

  if (queue->head) {
    Task *ret = queue->head;
    queue->head = queue->head->next;

    if (queue->head == NULL)
      queue->tail = NULL;

    queue->size--;

    printf("dequeued task, size is %d\n", queue->size);
    return ret;
  }

  return NULL;
}

Task *new_task(int socket) {
  Task *task = malloc(sizeof(Task));
  task->next = NULL;
  task->request = NULL;
  task->socket = socket;
  return task;
}

ThreadPool new_thread_pool(void *start_thread, TaskQueue *task_queue) {
  ThreadPool pool;

  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&pool.threads[i], NULL, start_thread,
                       (void *)task_queue) != 0) {
      fprintf(stderr, "Failed to create thread\n");
    }
  }

  return pool;
}
