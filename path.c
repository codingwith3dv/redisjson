#include "path.h"
#include <string.h>
#include <ctype.h>

typedef enum {
  CSNONE = 0,
  CSOBJECT = 1,
  CSARRAY = 2,
  CSRDESCENT = 3
} CharState;

typedef union {
  size_t index;
  const char* key;
  CharState sstate;
} Path;

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

void vecPush(Vector* v, void* value) {
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

void vecClear(Vector* v) {
  if(!v) return;
  if(v->data) RedisModule_Free(v->data);
  v->len = 0;
  vecNew(v, 1, v->elemSize);
}

typedef enum {
  START,
  ROOT,
  DOT,
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

Vector* recursiveSearch(
  JsonValue* val,
  const char* key,
  Vector* vals) {
  if(val->type == OBJECT) {
    struct JsonObject object = val->value.object;
    for(size_t i = 0; i < object.size; i++) {
      JsonKeyVal* keyVal = object.elements[i];
      if(!strcmp(keyVal->key, key)) {
        vecPush(vals, &keyVal->value);
      }
      vals = recursiveSearch(keyVal->value, key, vals);
    }
  } else if(val->type == ARRAY) {
    JsonArray array = val->value.array;
    for(size_t i = 0; i < array.size; i++) {
      vals = recursiveSearch(array.array[i], key, vals);
    }
  }
  return vals;
}

JsonValue** evalPath(
  RedisModuleCtx* ctx,
  JsonValue* value,
  RedisModuleString* path,
  size_t* outLen
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
  CharState sstate = CSNONE;

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
      case DOT:
      case ROOT: {
        if(isalpha(ch) || ch == '$') {
          ++tokLen;
          state = WORD;
        } else if(ch == '.') {
          ++tokLen;
          state = DOT;
          sstate = CSRDESCENT;
          ++cpath2;
          ++i;
          goto tokEnd;
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
          sstate = CSARRAY;
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
          state = ch == '.' ? DOT : SBRACKET;
          sstate = CSOBJECT;
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
      sstate = CSOBJECT;
      isTokNum = false;
      goto tokEnd;
    }
    continue;
    tokEnd: {
      Path path;
      
      path.sstate = sstate;
      switch(sstate) {
        case CSOBJECT:
          path.key = _strdup(tok, tokLen);
          break;
        case CSARRAY: {
          size_t index = 0;
          for(size_t j = 0; j < tokLen; j++) {
            index = index * 10 + (tok[j] - 48);
          }
          path.index = index;
          break;
        }
        default: break;
      }
      sstate = CSNONE;
      vecPush(&paths, &path);
      tok = cpath2;
      tokLen = 0;
    }
  }

  Vector currArr;
  vecNew(&currArr, 1, sizeof(JsonValue*));
  vecPush(&currArr, &value);
  JsonValue** data = (JsonValue**)currArr.data;
  Path* pdata = (Path*)paths.data;
  if(paths.len == 1 && !strcmp(pdata[0].key, "$")) {
    *outLen = 1;
    return data;
  }
  for(size_t i = 0; i < paths.len; i++) {
    if(pdata[i].sstate == CSRDESCENT) {
      vecClear(&currArr);
      currArr = *recursiveSearch(data[0], pdata[i + 1].key, &currArr);
      continue;
    }
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
  *outLen = currArr.len;
  return data;
}
