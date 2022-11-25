#include "path.h"
#include <string.h>
#include <ctype.h>

typedef union {
  size_t index;
  const char* key;
} Path;

typedef struct {
  char* data;
  size_t cap;
  size_t len;
  size_t elemSize;
} Vector;

void vecNew(Vector* v, size_t cap, size_t elemSize) {
  v->cap = cap;
  v->len = 0;
  v->elemSize = elemSize;
  v->data = RedisModule_Alloc(v->elemSize * v->cap);
}

void vecResize(Vector* v, size_t cap) {
  if(!v) return;
  v->data = RedisModule_Realloc(v->data, v->elemSize * cap);
  v->cap = cap;
}

void vecPush(Vector* v, char* value) {
  if(!v) return;
  if(v->cap <= v->len) {
    vecResize(v, v->cap * 2);
  }
  memcpy(v->data + (v->elemSize * v->len++), value, v->elemSize);
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

JsonValue** evalPath(
  RedisModuleCtx* ctx,
  JsonValue* value,
  RedisModuleString* path
) {
  size_t clen;
  const char* cpath = RedisModule_StringPtrLen(path, &clen);
  const char* cpath2 = cpath;
  State state = START;

  Vector paths;
  vecNew(&paths, 1, sizeof(Path));

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
      vecPush(&paths, (char*)&path);
      tok = cpath2;
      tokLen = 0;
    }
  }

  Vector currArr;
  vecNew(&currArr, 1, sizeof(JsonValue*));
  vecPush(&currArr, (char*)&value);
  JsonValue** data = (JsonValue**)currArr.data;
  Path* pdata = (Path*)paths.data;
  if(paths.len == 1 && !strcmp(pdata[0].key, "$")) {
    return data;
  }
  for(size_t i = 0; i < paths.len; i++) {
    for(size_t j = 0; j < currArr.len; j++) {
      if(data[j]->type == OBJECT) {
        struct JsonObject obj = data[j]->value.object;
        for(size_t k = 0; k < obj.size; k++) {
          if(!strcmp(obj.elements[k]->key, pdata[i].key)) {
            data[j]= obj.elements[k]->value;
          }
        }
      } else if(data[j]->type == ARRAY) {
        JsonArray arr = data[j]->value.array;
        size_t index = pdata[i].index;
        if(index >= 0 && index < arr.size) {
          data[j] = arr.array[index];
        }
      }
    }
  }

  vecDel(&paths);
  return (JsonValue**)currArr.data;
}
