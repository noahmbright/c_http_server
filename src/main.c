#include "http_server.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_LENGTH (4096)
pthread_mutex_t queue_mutex;
pthread_cond_t queue_condition;

void sigchld_handler(int s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved_errno;
}

void serve_request(Task *task) {
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("thread %llul sleeping\n", tid);
  sleep(5);
  int accepted_socket = task->socket;

  char buffer[BUFFER_LENGTH];

  printf("thread %llul recv'ing\n", tid);
  unsigned received_bytes = recv(accepted_socket, buffer, BUFFER_LENGTH, 0);
  if ((received_bytes) == -1) {
    perror("received -1 bytes");
    exit(1);
  }

  assert(received_bytes < BUFFER_LENGTH);
  buffer[received_bytes] = '\0';
  RequestLine request_line = parse_request_line(buffer);

  unsigned sent_bytes, message_length;
  const char *message;

  if (request_line.is_valid) {
    message = "HTTP/1.0 200";
    message_length = 12;
  } else {
    message = "HTTP/1.0 400";
    message_length = 12;
  }

  printf("thread %llul sending\n", tid);
  if ((sent_bytes = send(accepted_socket, message, message_length, 0)) == -1) {
    perror("sent -1 bytes");
  }
  close(accepted_socket);
}

void submit_task(TaskQueue *queue, Task *task) {
  // submitting the task is a critical section for the main thread
  pthread_mutex_lock(&queue_mutex);
  enqueue_task(queue, task);
  pthread_mutex_unlock(&queue_mutex);
  pthread_cond_signal(&queue_condition);
}

// args will be/contain a task queue pointer
void *start_server_thread(void *args) {
  TaskQueue *task_queue = (TaskQueue *)args;

  // critical section is checking the shared queue for a task
  // don't want two threads data racing for the same task
  // get the lock and wait until the task queue has non zero size
  // this thread will be blocking but thats okay, there's no work to do
  while (1) {
    pthread_mutex_lock(&queue_mutex);

    while (task_queue->size == 0)
      pthread_cond_wait(&queue_condition, &queue_mutex);

    // want the actual serving to occur outside the critical section in parallel
    // unlock once we got something to do
    // allow another thread to start waiting on stuff to do
    Task *task = dequeue_task(task_queue);
    pthread_mutex_unlock(&queue_mutex);

    assert(task);
    serve_request(task);

    task = NULL;
    free(task);
  }
}

int main() {

  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  int listener_socket = get_socket();
  pthread_mutex_init(&queue_mutex, NULL);
  pthread_cond_init(&queue_condition, NULL);
  TaskQueue task_queue = new_task_queue();
  ThreadPool thread_pool = new_thread_pool(&start_server_thread, &task_queue);

  uint64_t main_tid;
  pthread_threadid_np(NULL, &main_tid);
  printf("main thread is %llu\n", main_tid);

  while (1) {
    printf("waiting to accept a socket, size %d\n", task_queue.size);
    int accepted_socket = accept_connection(listener_socket);
    printf("accepted socket, size %d\n", task_queue.size);
    submit_task(&task_queue, new_task(accepted_socket));
  }

  pthread_mutex_destroy(&queue_mutex);
  pthread_cond_destroy(&queue_condition);

  return 0;
}
