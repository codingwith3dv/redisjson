#include "path.h"
#include <string.h>
#include <ctype.h>

typedef struct {
  void** data;
  size_t cap;
  size_t len;
} Vector;

void vecNew(Vector* v, size_t cap) {
  v->cap = cap;
  v->len = 0;
  v->data = RedisModule_Alloc(sizeof(void*) * v->cap);
}

void vecResize(Vector* v, size_t cap) {
  if(!v) return;
  v->data = RedisModule_Realloc(v->data, sizeof(void*) * cap);
  v->cap = cap;
}

void vecPush(Vector* v, void* value) {
  if(!v) return;
  if(v->cap <= v->len) {
    vecResize(v, v->cap * 2);
  }
  v->data[v->len++] = value;
}

void vecDel(Vector* v) {
  if(!v) return;
  RedisModule_Free(v->data);
}

typedef enum {
  START,
  ROOT,
  WORD
} State;

char* _strdup(const char *str1, size_t len) {
  char* str2 = RedisModule_Calloc(len + 1, sizeof(char));
  if (str2)
    memcpy(str2, str1, len);
  return str2;
}

JsonValue* evalPath(
  RedisModuleCtx* ctx,
  JsonValue* value,
  RedisModuleString* path
) {
  size_t clen;
  const char* cpath = RedisModule_StringPtrLen(path, &clen);
  const char* cpath2 = cpath;
  State state = START;

  Vector paths;
  vecNew(&paths, 1);

  const char* tok = cpath2;
  size_t tokLen = 0;
  size_t i = 0;
  while(i < clen) {
    char ch = *cpath2;
    switch(state) {
      case START: {
        if(isalpha(ch) || ch == '$') {
          ++tokLen;
          state = WORD;
          break;
        }
      }
      case ROOT: {
        if(isalpha(ch) || ch == '$') {
          ++tokLen;
          state = WORD;
        }
        break;
      }
      case WORD: {
        if(ch == '.') {
          state = ROOT;
          ++cpath2;
          ++i;
          goto tokEnd;
        }
        ++tokLen;
        break;
      }
    }
    ++i;
    ++cpath2;
    if((clen == i) && (state == WORD)) {
      state = START;
      goto tokEnd;
    }
    continue;
  tokEnd:
    vecPush(&paths, _strdup(tok, tokLen));
    tok = cpath2;
    tokLen = 0;
  }

  if(paths.len == 1 && !strcmp(paths.data[0], "$")) {
    return value;
  }
  JsonValue* curr = value;
  for(size_t i = 0; i < paths.len; i++) {
    if(curr->type == OBJECT) {
      struct JsonObject obj = curr->value.object;
      for(size_t j = 0; j < obj.size; j++) {
        if(!strcmp(obj.elements[j]->key, paths.data[i])) {
          curr = obj.elements[j]->value;
        }
      }
    }
  }

  vecDel(&paths);
  return curr;
}
