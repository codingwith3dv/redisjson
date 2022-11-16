#include "jsonToValue.h"
#include "redismodule.h"
#include "value.h"
#include <string.h>
#include <math.h>

typedef struct {
  const char* json;
  size_t index;
  RedisModuleCtx* rctx;
} ParserContext;

JsonValue* parseValue(ParserContext* ctx);

void skipSpace(ParserContext* ctx) {
  while(
    ctx->json[ctx->index] == ' ' ||
    ctx->json[ctx->index] == '\n'
  ) {
    ++ctx->index;
  }
}

const char* parseStr(ParserContext* ctx, size_t* length) {
  skipSpace(ctx);
  const char* start = ctx->json + ctx->index;
  const char* end = start;
  if(
    ctx->json[ctx->index] == '"' ||
    ctx->json[ctx->index] == '\''
  ) {
    ++start;
    ++ctx->index;
    while(
      ctx->json[ctx->index] != '"' &&
      ctx->json[ctx->index] != '\''
    ) {
      ++ctx->index;
      ++end;
    }
    ++ctx->index;
    size_t len = (end - start) + 1;
    char* str = RedisModule_Alloc(len * sizeof(char));
    memcpy(str, start, len);
    str[len] = '\0';
    *length = len;
    return str;
  }
  *length = 0;
  return 0;
}

void parseObject(ParserContext* ctx, JsonValue* val) {
  ++ctx->index;
  size_t elemSize = 0;
  struct JsonObject object;
  object.size = 0;
  while(ctx->json[ctx->index] != '}') {
    JsonKeyVal* keyVal = RedisModule_Calloc(1, sizeof(JsonKeyVal));
    size_t len;

    skipSpace(ctx);
    const char* key = parseStr(ctx, &len);
    keyVal->key = key;
    skipSpace(ctx);

    if(ctx->json[ctx->index] == ':')
      ++ctx->index;

    skipSpace(ctx);
    JsonValue* objVal = parseValue(ctx);
    keyVal->value = objVal;
    skipSpace(ctx);

    ++elemSize;
    if(object.size == 0) {
      object.elements = RedisModule_Calloc(1, sizeof(JsonKeyVal*));
    } else if(elemSize > object.size) {
      object.elements = RedisModule_Realloc(
        object.elements,
        elemSize * 2 * sizeof(JsonKeyVal*)
      );
    }
    object.size = elemSize;
    object.elements[object.size - 1] = keyVal;

    if(ctx->json[ctx->index] == ',')
      ++ctx->index;
  }
  skipSpace(ctx);
  ++ctx->index;
  val->value.object = object;
  val->type = OBJECT;
}

void parseArray(ParserContext* ctx, JsonValue* val) {
  ++ctx->index;
  JsonArray array;
  array.size = 0;
  size_t elemSize = 0;
  while(ctx->json[ctx->index] != ']') {
    skipSpace(ctx);
    JsonValue* elem = parseValue(ctx);
    skipSpace(ctx);
    ++elemSize;
    if(array.size == 0) {
      array.array = RedisModule_Calloc(1, sizeof(JsonKeyVal*));
    } else if(elemSize > array.size) {
      array.array = RedisModule_Realloc(
        array.array,
        elemSize * 2 * sizeof(JsonValue*)
      );
    }
    array.size = elemSize;
    array.array[array.size - 1] = elem;
    if(ctx->json[ctx->index] == ',')
      ++ctx->index;
  }
  skipSpace(ctx);
  ++ctx->index;
  val->value.array = array;
  val->type = ARRAY;
}

JsonValue* parseValue(ParserContext* ctx) {
  JsonValue* val = RedisModule_Calloc(1, sizeof(JsonValue));
  skipSpace(ctx);
  if(ctx->json[ctx->index] == '{') {
    parseObject(ctx, val);
  } else if (
    ctx->json[ctx->index] == '"' ||
    ctx->json[ctx->index] == '\''
  ) {
    size_t len;
    const char* str = parseStr(ctx, &len);
    val->type = STRING;
    val->value.string.size = len;
    val->value.string.data = str;
  } else if (ctx->json[ctx->index] == '[') {
    parseArray(ctx, val);
  } else if (ctx->json[ctx->index] == 't') { // true
    ctx->index += 4;
    val->type = BOOLEAN;
    val->value.boolean = true;
  } else if (ctx->json[ctx->index] == 'f') { // false
    ctx->index += 5;
    val->type = BOOLEAN;
    val->value.boolean = false;
  } else {
    long long number = 0;
    skipSpace(ctx);
    while(ctx->json[ctx->index] >= '0' && ctx->json[ctx->index] <= '9') {
      number = (number * 10) + (ctx->json[ctx->index] - 48);
      ++ctx->index;
    }
    val->value.number = number;
    val->type = NUMBER;
  }
  return val;
}

JsonValue* parseJson(
  RedisModuleCtx* ctx,
  const char* json
) {
  ParserContext pctx;
  pctx.json = json;
  pctx.index = 0;
  pctx.rctx = ctx;

  return parseValue(&pctx);
}

static void valueToString(
  RedisModuleCtx* ctx,
  JsonValue* val,
  RedisModuleString* out
);

static void arrayToString(
  RedisModuleCtx* ctx,
  JsonValue* val,
  RedisModuleString* out
) {
  RedisModule_StringAppendBuffer(ctx, out, "[", 1);
  for(size_t i = 0; i < val->value.array.size; i++) {
    valueToString(ctx, val->value.array.array[i], out);
    RedisModule_StringAppendBuffer(ctx, out, ",", 1);
  }
  RedisModule_StringAppendBuffer(ctx, out, "]", 1);
}

static void objectToString(
  RedisModuleCtx* ctx,
  JsonValue* val,
  RedisModuleString* out
) {
  RedisModule_StringAppendBuffer(ctx, out, "{", 1);
  for(size_t i = 0; i < val->value.object.size; i++) {
    RedisModule_StringAppendBuffer(ctx, out, "\"", 1);
    const char* key = val->value.object.elements[i]->key;
    RedisModule_StringAppendBuffer(ctx, out, key, strlen(key));
    RedisModule_StringAppendBuffer(ctx, out, "\":", 2);

    valueToString(ctx, val->value.object.elements[i]->value, out);
    RedisModule_StringAppendBuffer(ctx, out, ",", 1);
  }
  RedisModule_StringAppendBuffer(ctx, out, "}", 1);
}

static void valueToString(
  RedisModuleCtx* ctx,
  JsonValue* val,
  RedisModuleString* out
) {
  switch(val->type) {
    case OBJECT:
      objectToString(ctx, val, out);
      break;
    case ARRAY:
      arrayToString(ctx, val, out);
      break;
    case NUMBER: {
      int len = (int)((ceil(log10(val->value.number)))*sizeof(char));
      char buf[len];
      sprintf(buf, "%lld", val->value.number);
      RedisModule_StringAppendBuffer(ctx, out, buf, len);
      break;
    }
    case STRING: {
      RedisModule_StringAppendBuffer(ctx, out, "\"", 1);
      RedisModule_StringAppendBuffer(
        ctx,
        out,
        val->value.string.data,
        val->value.string.size
      );
      RedisModule_StringAppendBuffer(ctx, out, "\"", 1);
      break;
    }
    case BOOLEAN: {
      bool isTrue = val->value.boolean;
      RedisModule_StringAppendBuffer(
        ctx,
        out,
        isTrue ? "true" : "false",
        isTrue ? 4 : 5
      );
      break;
    }
    default:
      break;
  }
}

RedisModuleString* jsonToString(
  RedisModuleCtx* ctx,
  JsonValue* val
) {
  RedisModuleString* str = RedisModule_CreateString(ctx, "", 0);
  valueToString(ctx, val, str);
  return str;
}
