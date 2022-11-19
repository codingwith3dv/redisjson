#include "path.h"
#include <string.h>
#include <ctype.h>

typedef union {
  size_t index;
  const char* key;
} Path;

typedef struct {
  Path* data;
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

void vecPush(Vector* v, Path value) {
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
  WORD,
  SBRACKET,
  NUMBER
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
  bool isTokNum = false;

  while(i < clen) {
    char ch = *cpath2;
    switch(state) {
      case START: {
        if(isalpha(ch) || ch == '$') {
          ++tokLen;
          state = WORD;
        }
        break;
      }
      case ROOT: {
        if(isalpha(ch) || ch == '$') {
          ++tokLen;
          state = WORD;
        }
        break;
      }
      case SBRACKET: {
        if(ch >= '0' && ch <= '9') {
          ++tokLen;
          state = NUMBER;
        }
        break;
      }
      case NUMBER: {
        if(ch == ']') {
          state = START;
          isTokNum = true;
          ++cpath2;
          ++i;
          goto tokEnd;
        }
        if(ch >= '0' && ch <= '9') {
          ++tokLen;
        }
        break;
      }
      case WORD: {
        if(ch == '.' || ch == '[') {
          state = ch == '.' ? ROOT : SBRACKET;
          isTokNum = false;
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
      isTokNum = false;
      goto tokEnd;
    }
    continue;
    tokEnd: {
      Path path;
      if(!isTokNum) {
        path.key = _strdup(tok, tokLen);
      } else {
        size_t index = 0;
        for(size_t j = 0; j < tokLen; j++) {
          index = index * 10 + (tok[j] - 48);
        }
        path.index = index;
      }
      vecPush(&paths, path);
      tok = cpath2;
      tokLen = 0;
    }
  }

  if(paths.len == 1 && !strcmp(paths.data[0].key, "$")) {
    return value;
  }
  JsonValue* curr = value;
  for(size_t i = 0; i < paths.len; i++) {
    if(curr->type == OBJECT) {
      struct JsonObject obj = curr->value.object;
      for(size_t j = 0; j < obj.size; j++) {
        if(!strcmp(obj.elements[j]->key, paths.data[i].key)) {
          curr = obj.elements[j]->value;
        }
      }
    } else if(curr->type == ARRAY) {
      JsonArray arr = curr->value.array;
      size_t index = paths.data[i].index;
      if(index >= 0 && index < arr.size) {
        curr = arr.array[index];
      }
    }
  }

  vecDel(&paths);
  return curr;
}
