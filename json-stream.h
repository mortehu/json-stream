// json-stream.h: Stream parser for JSON -- uses minimal memory
//
// Copyright (C) 2011  Morten Hustveit <morten.hustveit@gmail.com>

#ifndef JSON_STREAM_H_
#define JSON_STREAM_H_ 1

#include <stdlib.h>

#define JSON_STREAM_VERSION 100

// Maximum nesting depth of JSON datastructure.
#define JSON_STREAM_MAX_DEPTH 64

#ifdef __cplusplus
extern "C" {
#endif

enum json_stream_event {
  JSON_OBJECT_BEGIN,
  JSON_OBJECT_END,
  JSON_ARRAY_BEGIN,
  JSON_ARRAY_END,
  JSON_STRING,
  JSON_NUMBER,
  JSON_BOOLEAN,
  JSON_NULL,

  JSON_END,

  JSON_PARSE_ERROR
};

union json_stream_value {
  double number;
  char *string;
  int boolean;
};

typedef int (*json_stream_callback)(enum json_stream_event type,
                                    const char *name,
                                    const union json_stream_value *value,
                                    void *user);

struct json_stream;

struct json_stream *json_stream_init(json_stream_callback callback, void *user);

ssize_t json_stream_consume(struct json_stream *stream, const char *input,
                            size_t size);

void json_stream_end(struct json_stream *stream);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* !JSON_STREAM_H_ */
