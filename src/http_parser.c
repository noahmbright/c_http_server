#include "http_server.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HEADERS 50

const char *current_location;
const char *beginning_of_current;

int expect_and_skip_char(char c) {
  if (c != *current_location++)
    return 0;
  return 1;
}

static void set_beginning_of_current() {
  beginning_of_current = current_location;
}

static int is_alphabetic(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' ||
         c == '_';
}

static int is_numeric(char c) { return (c >= '0' && c <= '9'); }

static void advance_to_end_of_current() {
  while (is_alphabetic(*current_location))
    current_location++;
}

static int is_whitespace() {
  return *current_location == ' ' || *current_location == '\t';
}

static void skip_whitespace() {
  while (is_whitespace())
    current_location++;
}

static int check_crlf() {
  return (*current_location == '\r' && current_location[1] == '\n');
}

static int consume_crlf() {
  if (*current_location == '\r')
    current_location++;
  else
    return 0;

  if (*current_location == '\n')
    current_location++;
  else
    return 0;

  return 1;
}

// expect that current_location is one byte past the end of the word we want the
// length of
static unsigned current_length() {
  return current_location - beginning_of_current;
}

static int check_keyword(const char *keyword) {
  int length = strlen(keyword);
  return current_length() == length &&
         memcmp(beginning_of_current, keyword, length) == 0;
}

static int is_safe() {
  char c = *current_location;
  return (c == '$' || c == '-' || c == '_' || c == '.');
}

static int is_reserved() {
  char c = *current_location;
  return (c == ';' | c == '/' || c == '?' || c == ':' || c == '@' || c == '&' ||
          c == '=' || c == '+');
}

static int is_extra() {
  char c = *current_location;
  return (c == '!' || c == '*' || c == '\'' || c == '(' || c == ')' ||
          c == ',');
}

// FIXME: Missing national

static int is_unreserved() {
  char c = *current_location;
  return (is_alphabetic(*current_location) || is_numeric(*current_location) ||
          is_safe() || is_extra());
}

// Missing escape
static int is_uchar() { return is_unreserved(); }

static int is_pchar() {
  char c = *current_location;
  return is_uchar() || c == ':' || c == '@' || c == '&' || c == '=' || c == '+';
}

static int is_scheme_char() {
  char c = *current_location;
  return is_alphabetic(c) || is_numeric(c) || c == '.' || c == '+' || c == '-';
}

static RelativePath new_relative_path() {
  RelativePath p;
  p.is_valid = 0;
  p.path = NULL;
  p.path_length = 0;
  p.params = NULL;
  p.params_length = 0;
  p.query = NULL;
  p.query_length = 0;
  return p;
}

// absolute path: "/" relative path
// relative path: [path] [";" params] ["?" query]
// path: ((1*pchar) "/")* [/]
// params: param *(";" param)
// param: *(pchar | "/")
// query: *(uchar | reserved)
RelativePath parse_relative_path() {
  RelativePath relative_path = new_relative_path();

  set_beginning_of_current();
  while (is_pchar() || *current_location == '/')
    ++current_location;

  relative_path.path = beginning_of_current;
  relative_path.path_length = current_length();

  // params
  if (*current_location == ';') {
    ++current_location;
    set_beginning_of_current();
    while (is_pchar() || *current_location == '/')
      ++current_location;

    relative_path.params = beginning_of_current;
    relative_path.params_length = current_length();
  }

  // query
  if (*current_location == '?') {
    ++current_location;
    set_beginning_of_current();
    while (is_uchar() || is_reserved())
      ++current_location;

    relative_path.query = beginning_of_current;
    relative_path.query_length = current_length();
  }

  // invalid
  if (!is_whitespace()) {
    return relative_path;
  }

  relative_path.is_valid = 1;
  return relative_path;
}

// https://datatracker.ietf.org/doc/html/rfc1945#section-3.2
// this function assumes that current_location points to the beginning of the
// URI on calling
// URI: (absoluteURI | relative URI) ["#" fragment]
//
// absolute URIs begin with either alphanumerics, +, -, or .
// relative URIs begin with //
// so its easy to disambiguate
// absoluteURI: scheme ":" *(uchar | reserved)
// https://datatracker.ietf.org/doc/html/rfc1945#section-5.1
// Request-Line = Method SP Request-URI SP HTTP-Version CRLF
static RequestLine parse_request_line(const char *text) {
  current_location = text;
  RequestLine request_line;
  memset(&request_line, 0, sizeof(request_line));

  skip_whitespace();
  set_beginning_of_current();
  advance_to_end_of_current();

  // method
  while (is_alphabetic(*current_location))
    ++current_location;
  request_line.method = beginning_of_current;
  request_line.method_length = current_length();

  // the spec says this should be a single space, but here we allow for any
  // number of spaces and tabs
  skip_whitespace();

  // URI
  // only parsing absolute path
  set_beginning_of_current();
  if (*current_location != '/') {
    return request_line;
  }
  RelativePath relative_path = parse_relative_path();
  if (relative_path.is_valid) {
    request_line.relative_path = relative_path;
  } else {
    return request_line;
  }

  skip_whitespace();

  // no HTTP version: found a simple request
  if (check_crlf()) {
    request_line.is_simple = 1;
    request_line.http_minor = 9;
    consume_crlf();
    request_line.is_valid = 1;
    return request_line;
  }

  // on the "HTTP"
  set_beginning_of_current();
  advance_to_end_of_current();
  int http_properly_formatted =
      (current_length() == 4 && (memcmp(beginning_of_current, "HTTP", 4) == 0));

  // FIXME handle this better
  if (!http_properly_formatted) {
    fprintf(stderr, "Request line HTTP not formatted\n");
    request_line.is_valid = 0;
    return request_line;
  }

  if (!(*current_location++ == '/')) {
    fprintf(stderr, "Request line HTTP not formatted\n");
    request_line.is_valid = 0;
    return request_line;
  }

  set_beginning_of_current();
  while (is_numeric(*current_location))
    current_location++;
  request_line.http_major = atoi(beginning_of_current);

  if (!(*current_location++ == '.')) {
    fprintf(stderr, "Request line HTTP not formatted\n");
    request_line.is_valid = 0;
    return request_line;
  }

  set_beginning_of_current();
  while (is_numeric(*current_location))
    current_location++;
  request_line.http_minor = atoi(beginning_of_current);

  skip_whitespace();

  if (!consume_crlf()) {
    fprintf(stderr, "Request line not followed by CRLF\n");
    request_line.is_valid = 0;
    return request_line;
  }

  request_line.is_valid = 1;
  return request_line;
}

static Header new_header() {
  Header header;
  header.header_string = NULL;
  header.header_length = 0;
  header.body_string = NULL;
  header.body_length = 0;
  return header;
}

// from picohttpparser
#define IS_PRINTABLE_ASCII(c) ((unsigned char)(c)-040u < 0137u)
static void parse_headers(HTTP_Request *request) {
  int num_headers = 0;
  // headers end when we get two CRLFs in a row
  // the first comes from the end of the last header
  while (!check_crlf()) {
    Header current_header = new_header();

    // header names
    set_beginning_of_current();
    while (IS_PRINTABLE_ASCII(*current_location) && *current_location != ':')
      current_location++;

    current_header.header_string = beginning_of_current;
    current_header.header_length = current_length();

    skip_whitespace();

    if (*current_location++ != ':') {
      fprintf(stderr, "header not formatted\n");
      request->is_valid = 0;
      return;
    }

    // header bodies
    skip_whitespace();
    set_beginning_of_current();
    while (IS_PRINTABLE_ASCII(*current_location))
      current_location++;
    current_header.body_string = beginning_of_current;
    current_header.body_length = current_length();

    consume_crlf();
    request->headers[num_headers++] = current_header;
  }

  consume_crlf();
  request->num_headers = num_headers;
  request->is_valid = 1;
}

// HTTP-message   = Simple-Request       ; HTTP/0.9 messages
//                  | Simple-Response
//                  | Full-Request       ; HTTP/1.0 messages
//                  | Full-Response
HTTP_Request parse_http_request(const char *text) {
  HTTP_Request request;
  request.is_valid = 0;
  request.headers = malloc(sizeof(Header) * MAX_HEADERS);
  request.request_line = parse_request_line(text);
#ifdef TUKE_DEBUG
  printf("about to parse headers, at\n%s", current_location);
#endif
  parse_headers(&request);
  return request;
}
