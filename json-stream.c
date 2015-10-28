#include <ctype.h>
#include <string.h>

#include <json-stream.h>

struct json_stream {
  enum json_stream_event stack[JSON_STREAM_MAX_DEPTH];
  unsigned int sp;

  char *key;
  int expecting_delimiter;

  json_stream_callback callback;
  void *user;
};

struct json_stream *json_stream_init(json_stream_callback callback,
                                     void *user) {
  struct json_stream *result;
  result = calloc(1, sizeof(*result));
  if (!result) return result;

  result->callback = callback;
  result->user = user;

  return result;
}

static size_t JSON_decode_string(char **result, const char *input,
                                 const char *end) {
  unsigned int ch;
  const char *c, *string_end;
  char *o;

  c = input;

  if (*c != '"') return 0;

  string_end = ++c;

  while (string_end != end && *string_end != '"') {
    if (*string_end == '\\' && string_end + 1 != end)
      string_end += 2;
    else
      ++string_end;
  }

  if (string_end == end) return 0;

  *result = malloc(string_end - c + 1);

  if (!*result) return 0;

  o = *result;

  while (c != string_end) {
    switch (*c) {
      case '\\':

        ++c;

        switch (ch = *c++) {
          case '0':
            ch = strtol(c, (char **)&c, 8);
            break;
          case 'a':
            ch = '\a';
            break;
          case 'b':
            ch = '\b';
            break;
          case 't':
            ch = '\t';
            break;
          case 'n':
            ch = '\n';
            break;
          case 'v':
            ch = '\v';
            break;
          case 'f':
            ch = '\f';
            break;
          case 'r':
            ch = '\r';
            break;
          case 'u':
            ch = strtol(c, (char **)&c, 16);
            break;
          default:
            break;
        }

        if (ch < 0x80) {
          *o++ = (ch);
        } else if (ch < 0x800) {
          *o++ = (0xc0 | (ch >> 6));
          *o++ = (0x80 | (ch & 0x3f));
        } else if (ch < 0x10000) {
          *o++ = (0xe0 | (ch >> 12));
          *o++ = (0x80 | ((ch >> 6) & 0x3f));
          *o++ = (0x80 | (ch & 0x3f));
        } else if (ch < 0x200000) {
          *o++ = (0xf0 | (ch >> 18));
          *o++ = (0x80 | ((ch >> 12) & 0x3f));
          *o++ = (0x80 | ((ch >> 6) & 0x3f));
          *o++ = (0x80 | (ch & 0x3f));
        } else if (ch < 0x4000000) {
          *o++ = (0xf8 | (ch >> 24));
          *o++ = (0x80 | ((ch >> 18) & 0x3f));
          *o++ = (0x80 | ((ch >> 12) & 0x3f));
          *o++ = (0x80 | ((ch >> 6) & 0x3f));
          *o++ = (0x80 | (ch & 0x3f));
        } else {
          *o++ = (0xfc | (ch >> 30));
          *o++ = (0x80 | ((ch >> 24) & 0x3f));
          *o++ = (0x80 | ((ch >> 18) & 0x3f));
          *o++ = (0x80 | ((ch >> 12) & 0x3f));
          *o++ = (0x80 | ((ch >> 6) & 0x3f));
          *o++ = (0x80 | (ch & 0x3f));
        }

        break;

      default:
        *o++ = *c++;
    }
  }

  *o = 0;

  return string_end - input + 1;
}

ssize_t json_stream_consume(struct json_stream *stream, const char *input,
                            size_t size) {
  size_t skip;
  const char *c, *end;
  union json_stream_value value;

  if (!size) return 0;

  c = input;
  end = input + size;

  while (c != end) {
    if (isspace(*c)) {
      ++c;

      continue;
    }

    switch (*c) {
      case '}':

        if (!stream->sp || stream->stack[stream->sp - 1] != JSON_OBJECT_BEGIN ||
            stream->key) {
          stream->callback(JSON_PARSE_ERROR, "Unexpected '}'", 0, stream->user);

          return -1;
        }

        stream->callback(JSON_OBJECT_END, 0, 0, stream->user);
        --stream->sp;
        ++c;
        stream->expecting_delimiter = ',';

        continue;

      case ']':

        if (!stream->sp || stream->stack[stream->sp - 1] != JSON_ARRAY_BEGIN) {
          stream->callback(JSON_PARSE_ERROR, "Unexpected ']'", 0, stream->user);

          return -1;
        }

        stream->callback(JSON_ARRAY_END, 0, 0, stream->user);
        --stream->sp;
        ++c;
        stream->expecting_delimiter = ',';

        continue;
    }

    if (stream->expecting_delimiter) {
      if (*c != stream->expecting_delimiter) {
        stream->callback(JSON_PARSE_ERROR, "Incorrect delimiter", 0,
                         stream->user);

        return -1;
      }

      stream->expecting_delimiter = 0;
      ++c;

      continue;
    }

    if (stream->sp && stream->stack[stream->sp - 1] == JSON_OBJECT_BEGIN &&
        !stream->key) {
      skip = JSON_decode_string(&stream->key, c, end);

      if (!skip) break;

      c += skip;
      stream->expecting_delimiter = ':';

      continue;
    }

    switch (*c) {
      case '{':

        if (stream->sp == JSON_STREAM_MAX_DEPTH) {
          stream->callback(JSON_PARSE_ERROR, "Stack exhausted", 0,
                           stream->user);

          return -1;
        }

        stream->stack[stream->sp++] = JSON_OBJECT_BEGIN;

        if (-1 ==
            stream->callback(JSON_OBJECT_BEGIN, stream->key, 0, stream->user))
          return -1;

        stream->expecting_delimiter = 0;

        ++c;

        break;

      case '[':

        if (stream->sp == JSON_STREAM_MAX_DEPTH) {
          stream->callback(JSON_PARSE_ERROR, "Stack exhausted", 0,
                           stream->user);

          return -1;
        }

        stream->stack[stream->sp++] = JSON_ARRAY_BEGIN;

        stream->callback(JSON_ARRAY_BEGIN, stream->key, 0, stream->user);
        stream->expecting_delimiter = 0;

        ++c;

        break;

      case 't':

        if (c + 4 > end) goto done;

        if (memcmp(c, "true", 4)) {
          stream->callback(JSON_PARSE_ERROR, "Unrecognized value", 0,
                           stream->user);

          return -1;
        }

        memset(&value, 0, sizeof(value));
        value.boolean = 1;

        stream->callback(JSON_BOOLEAN, stream->key, &value, stream->user);

        c += 4;

        stream->expecting_delimiter = ',';

        break;

      case 'f':

        if (c + 5 > end) goto done;

        if (memcmp(c, "false", 5)) {
          stream->callback(JSON_PARSE_ERROR, "Unrecognized value", 0,
                           stream->user);

          return -1;
        }

        memset(&value, 0, sizeof(value));

        stream->callback(JSON_BOOLEAN, stream->key, &value, stream->user);

        c += 5;

        stream->expecting_delimiter = ',';

        break;

      case 'n':

        if (c + 4 > end) goto done;

        if (memcmp(c, "null", 4)) {
          stream->callback(JSON_PARSE_ERROR, "Unrecognized value", 0,
                           stream->user);

          return -1;
        }

        stream->callback(JSON_NULL, stream->key, 0, stream->user);

        c += 4;

        stream->expecting_delimiter = ',';

        break;

      case '\"':

        memset(&value, 0, sizeof(value));

        skip = JSON_decode_string(&value.string, c, end);

        if (!skip) goto done;

        c += skip;

        if (-1 ==
            stream->callback(JSON_STRING, stream->key, &value, stream->user))
          return -1;

        free(value.string);

        stream->expecting_delimiter = ',';

        break;

      case '-':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':

        /* XXX: Doesn't work if entire response is a number */

        if (memchr(c, ',', end - c) || memchr(c, '}', end - c) ||
            memchr(c, ']', end - c)) {
          memset(&value, 0, sizeof(value));

          value.number = strtod(c, (char **)&c);

          if (-1 ==
              stream->callback(JSON_NUMBER, stream->key, &value, stream->user))
            return -1;
        } else
          goto done;

        stream->expecting_delimiter = ',';

        break;

      default:

        stream->callback(JSON_PARSE_ERROR, "Unrecognized value", 0,
                         stream->user);

        return -1;
    }

    free(stream->key);
    stream->key = 0;
  }

done:

  return c - input;
}

void json_stream_end(struct json_stream *stream) {
  stream->callback(JSON_END, 0, 0, stream->user);

  free(stream->key);
  free(stream);
}
