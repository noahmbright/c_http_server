A multithreaded implementation of HTTP in C. 

# Structure

## The sockets

This section is a very close summary of [Beej's
guide](beej's-guide-to-network-programming).

Sockets are what allow the server and clients to communicate. As a server, you
need to _bind_ to a socket, then _listen_ for any incoming connections.

An incoming connection will the be the result of a _client_ trying to _connect_.

### `getaddrinfo()`

`getaddrinfo` gives you a struct containing information about IP addresses. You
pass it a pointer to a linked list of `struct addrinfo`'s, which it will update
with its results. You can walk that linked list looking for valid addresses to
get sockets on. Behave yourself and call `freeaddrinfo` on it when you're done.

### `socket()`

To get the actual socket you use to communicate with everyone, call the
`socket()` function. This takes three arguments, which you can pass from your
`addrinfo`. `socket()` returns you an `int`, which is a socket descriptor, or
-1 on error.

### `bind()`

This is necessary for the server. The actual binding of the socket to a port
happens as a side effect of this function. The returned `int` is -1 on failure.

### `listen()`

After `bind()`ing to a port, you `listen()` for incoming connections. Listen
takes two arguments, the first is your socket descriptor. The second is another
`int` that limits the number of incoming connections that will get queued and
wait to be `accept()`ed.

### `accept()`

When listening and a connection arrives, you must `accept()` it. `accept()` returns
an `int`, which is another socket descriptor. This is the one that you can use
to `send()` and `recv()` on. The socket descriptor from the original call to
`socket()` continue to `listen()`.

### `send()` and `recv()`

`send()` and `recv()` actually send messages around. For a web server, you want
reliable message transfer, so you use TCP connections, which maintain a
connection between client and server. These system calls work over TCP connections.

## Multithreading

Threading is done using a threadpool. The idea is that starting a brand new thread
every single time a connection comes in is expensive, and an attacker can send in
some excessive number of requests and bog down your system with thousands of
new threads. 

The alternative is to start a fixed number of worker threads at the beginning
of the program. When the main thread gets a connection, it puts a task in a
task queue. When the task queue is nonempty, worker threads will grab the next
task and serve the request. 

There are a few chances for data races in this model. The tasks are stored in 
a shared resource: the main thread's task queue. It's possible that the OS
will schedule a context switch while a worker is grabbing a task from the queue,
and then another worker thread will also end up grabbing the same task. The
critical sections in this program are thus where the tasks are enqueued by the
main thread and dequeued by the workers. We simply need to lock and unlock a
shared mutex before and after touching the queue.

To avoid the workers constantly spinning while they wait for incoming tasks,
we use condition variables. The worker threads sleep while the task queue has
size 0. When the main thread enqueues a task, it signals that there is work to
do, and the next thread in line gets to work.

# Resources

* [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/). The
  canonical resource for C socket programming.

* [RFC 1945](https://datatracker.ietf.org/doc/html/rfc1945). The RFC I found
  first, hence the one I'm using as my source of truth.

* [Kazuho Oku's
  Picohttpparser](https://github.com/h2o/picohttpparser/tree/master) This is a
  really good resource for a fast parser, using some arcane C.

* [This reddit
  comment](https://www.reddit.com/r/C_Programming/comments/kbfa6t/comment/gfh8kid/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button)
  gives a lot of good ideas about what a server can do. This was useful after I
  could accept a message from Chrome and was like, "now what".

* [CodeVault](https://www.youtube.com/watch?app=desktop&v=_n2hE2gyPxU) on
  YouTube. I learned almost the entirety of the pthreads API from his lessons. 
