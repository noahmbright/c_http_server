#pragma once
#include <pthread.h>

#define NUM_THREADS 2

typedef struct {
  int is_valid;

  const char *scheme;
  unsigned scheme_length;

} URI;

typedef struct {
  int is_valid;

  const char *path;
  unsigned path_length;

  const char *query;
  unsigned query_length;

  const char *params;
  unsigned params_length;
} RelativePath;

typedef struct {
  int is_valid;
  const char *method;
  unsigned method_length;

  RelativePath relative_path;

  unsigned http_major;
  unsigned http_minor;

  int is_simple;
} RequestLine;

typedef struct Header {
  const char *header_string;
  unsigned header_length;
  const char *body_string;
  unsigned body_length;

  struct Header *next;
} Header;

typedef struct Task {
  struct Task *next;
  int socket;
  const char *request;
} Task;

typedef struct {
  Task *head;
  Task *tail;
  int size;
} TaskQueue;

typedef struct {
  pthread_t threads[NUM_THREADS];
} ThreadPool;

// sockets
int get_socket();
int accept_connection(int socket_descriptor);

// parsing
RequestLine parse_request_line(const char *);

// task queue
TaskQueue new_task_queue();
Task *new_task(int socket);
void enqueue_task(TaskQueue *, Task *);
Task *dequeue_task(TaskQueue *queue);
ThreadPool new_thread_pool(void *start_thread, TaskQueue *);
