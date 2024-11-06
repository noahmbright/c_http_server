typedef struct {
  const char *beginning_of_uri;
  unsigned length;
} URI;

typedef struct {
  const char *method;
  unsigned method_length;

  URI uri;
  unsigned http_major;
  unsigned http_minor;
  int is_simple;
} RequestLine;

typedef struct Header {
  const char *header_string;
  unsigned header_length;
  const char *body_string;
  unsigned body_length;

  Header *next;
} Header;
