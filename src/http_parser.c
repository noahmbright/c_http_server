#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *current_location;
const char *beginning_of_current;

static void set_beginning_of_current() {
  beginning_of_current = current_location;
}

static int is_alphabetic(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
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

// https://datatracker.ietf.org/doc/html/rfc1945#section-3.2
static URI parse_uri() {

  set_beginning_of_current();
  while (!is_whitespace())
    current_location++;

  URI uri;
  uri.length = current_length();
  uri.beginning_of_uri = beginning_of_current;

  return uri;
}

// https://datatracker.ietf.org/doc/html/rfc1945#section-5.1
// Request-Line = Method SP Request-URI SP HTTP-Version CRLF
static RequestLine parse_request_line(const char *text) {
  RequestLine request_line;

  skip_whitespace();
  set_beginning_of_current();
  advance_to_end_of_current();

  switch (*current_location) {
  case 'G':
    if (current_length() == 3 && memcmp(beginning_of_current, "GET", 3) == 0)
      request_line.method = METHOD_GET;
  case 'P':
    if (current_length() == 4 && memcmp(beginning_of_current, "POST", 4) == 0)
      request_line.method = METHOD_POST;
  case 'H':
    if (current_length() == 4 && memcmp(beginning_of_current, "HEAD", 4) == 0)
      request_line.method = METHOD_HEAD;
  default:
    request_line.method = METHOD_EXTENSION;
  }

  // the spec says this should be a single space, but here we allow for any
  // number of spaces and tabs
  skip_whitespace();

  request_line.uri = parse_uri();
  skip_whitespace();

  // HTTP
  //
  set_beginning_of_current();
  advance_to_end_of_current();
  int http_properly_formatted =
      (current_length() == 4 && memcmp(beginning_of_current, "HTTP", 4) == 0);

  // FIXME handle this better
  if (!http_properly_formatted) {
    fprintf(stderr, "Request line HTTP not formatted\n");
    exit(1);
  }

  if (!(*current_location++ == '/')) {
    fprintf(stderr, "Request line HTTP not formatted\n");
    exit(1);
  }

  set_beginning_of_current();
  while (is_numeric(*current_location))
    current_location++;
  request_line.http_major = atoi(beginning_of_current);

  if (!(*current_location++ == '.')) {
    fprintf(stderr, "Request line HTTP not formatted\n");
    exit(1);
  }

  set_beginning_of_current();
  while (is_numeric(*current_location))
    current_location++;
  request_line.http_minor = atoi(beginning_of_current);

  skip_whitespace();

  // FIXME handle this better
  if (!consume_crlf()) {
    fprintf(stderr, "Request line not followed by CRLF\n");
    exit(1);
  }

  return request_line;
}

// HTTP-message   = Simple-Request       ; HTTP/0.9 messages
//                  | Simple-Response
//                  | Full-Request       ; HTTP/1.0 messages
//                  | Full-Response
void parse_http(const char *text) {
  current_location = text;
  beginning_of_current = text;
}
