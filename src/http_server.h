typedef enum { METHOD_GET, METHOD_HEAD, METHOD_POST, METHOD_EXTENSION } Method;

typedef struct {
  const char *beginning_of_uri;
  unsigned length;
} URI;

typedef struct {
  Method method;
  URI uri;
  unsigned http_major;
  unsigned http_minor;
} RequestLine;
