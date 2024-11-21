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
#define MAX_ATTEMPTS (10)

pthread_mutex_t queue_mutex;
pthread_cond_t queue_condition;

void send_400_response(int accepted_socket) {
  long sent_bytes;
  const char *message = "HTTP/1.0 400\r\n";
  int message_length = 14;
  if ((sent_bytes = send(accepted_socket, message, message_length, 0)) == -1) {
    perror("sent -1 bytes on file_to_send");
  }
  close(accepted_socket);
}

void sigchld_handler(int s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved_errno;
}

static char *read_file(const char *path, long *size) {
  printf("trying to open file: %s\n", path);
  FILE *file = fopen(path, "r");
  if (!file) {
    perror("failed to open file to read");
    return NULL;
  }

  fseek(file, 0L, SEEK_END);
  long file_size = ftell(file);
  rewind(file);

  char *buffer = malloc(file_size + 1);
  if (!buffer) {
    perror("failed to malloc");
    return NULL;
  }

  long bytes_read = fread(buffer, sizeof(char), file_size, file);
  if (bytes_read < file_size) {
    perror("failed to fread");
    return NULL;
  }

  buffer[file_size] = '\0';
  *size = file_size;
  fclose(file);
  return buffer;
}

void serve_request(Task *task) {
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  printf("thread %llu sleeping\n", tid);
  sleep(1);
  int accepted_socket = task->socket;

  char buffer[BUFFER_LENGTH];

  printf("thread %llu recv'ing\n", tid);
  unsigned received_bytes = recv(accepted_socket, buffer, BUFFER_LENGTH, 0);
  if (received_bytes == -1 || received_bytes > BUFFER_LENGTH) {
    perror("received -1 bytes");
    send_400_response(accepted_socket);
    return;
  }
  buffer[received_bytes] = '\0';

  HTTP_Request request = parse_http_request(buffer);
  RequestLine request_line = request.request_line;
  Header *headers = request.headers;
  const char *url = request_line.relative_path.path;

  printf("request line url is %.*s\n", request_line.relative_path.path_length,
         url);

  long sent_bytes;
  unsigned message_length = 0;

  if (!request_line.is_valid) {
    fprintf(stderr, "invalid request line, responding 400\n");
    send_400_response(accepted_socket);
    return;
  }

  char filepath[256];
  int filepath_bytes_written;
  if (request_line.relative_path.path_length == 1 &&
      strncmp(url, "/", 1) == 0) {
    filepath_bytes_written =
        snprintf(filepath, 256, "files_to_serve/index.html");
  } else {
    filepath_bytes_written =
        snprintf(filepath, 256, "files_to_serve/%.*s",
                 request_line.relative_path.path_length, url);
  }

  long file_size = 0;
  const char *file_to_send = read_file(filepath, &file_size);

  int attempts = 1;
  while (file_to_send == NULL && attempts < MAX_ATTEMPTS) {
    file_to_send = read_file(request_line.relative_path.path, &file_size);
    ++attempts;
  }

  if (file_to_send == NULL) {
    fprintf(stderr, "file_to_send is NULL, responding 400\n");
    send_400_response(accepted_socket);
    return;
  }

  const char *response_header = "HTTP/1.0 200\r\n";
  message_length += 14;

  char content_length_header[50];
  int content_length_bytes_written = snprintf(
      content_length_header, 50, "Content-Length: %ld\r\n", file_size + 1);
  if (content_length_bytes_written > 50) {
    fprintf(stderr, "wrote too large a content length, responding 400\n");
    send_400_response(accepted_socket);
    free((void *)file_to_send);
    return;
  }

  const char *content_type = NULL;
  if (request_line.relative_path.path_length == 12 &&
      strncmp(url, "/favicon.ico", 12) == 0) {
    content_type = "Content-Type: image/ico\r\n";
  } else {
    content_type = "Content-Type: text/html; charset=utf-8\r\n";
  }

  char http_response[8192];
  int message_bytes_written =
      snprintf(http_response, 8192, "%s%s%s\r\n%s", response_header,
               content_length_header, content_type, file_to_send);
  if (message_bytes_written > 8192) {
    fprintf(stderr, "wrote too large a message, responding 400\n");
    send_400_response(accepted_socket);
    free((void *)file_to_send);
    return;
  }

  printf("sending message:\n%s\n", http_response);

  if (send_all(accepted_socket, http_response, message_bytes_written + 1,
               &sent_bytes) == -1) {
    perror("sent -1 bytes");
  }

  while (headers) {
    Header *temp = headers;
    headers = headers->next;
    free(temp);
  }

  printf("Finished serving request\n");
  free((void *)file_to_send);
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
