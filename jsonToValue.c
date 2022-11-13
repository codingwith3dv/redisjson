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
    do {
      ++ctx->index;
      ++end;
    } while(
      ctx->json[ctx->index] != '"' &&
      ctx->json[ctx->index] != '\''
    );
    ++ctx->index;
    size_t len = (end - start);
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
    size_t len;
    const char* key = parseStr(ctx, &len);
    skipSpace(ctx);
    if(ctx->json[ctx->index] == ':')
      ++ctx->index;
    skipSpace(ctx);
    JsonValue* objVal = parseValue(ctx);
    JsonKeyVal* keyVal = RedisModule_Calloc(1, sizeof(JsonKeyVal));
    keyVal->key = key;
    keyVal->value = objVal;
    ++elemSize;
    if(elemSize > object.size) {
      object.elements = RedisModule_Realloc(
        object.elements,
        elemSize * 1.5 * sizeof(JsonKeyVal*)
      );
      object.size = elemSize;
    }
    object.elements[object.size - 1] = keyVal;
    skipSpace(ctx);
    if(ctx->json[ctx->index] == ',')
      ++ctx->index;
  }
  val->value.object = object;
  val->type = OBJECT;
}

JsonValue* parseValue(ParserContext* ctx) {
  JsonValue* val = RedisModule_Calloc(1, sizeof(JsonValue));
  if(ctx->json[ctx->index] == '{') {
    parseObject(ctx, val);
  } else if(
    ctx->json[ctx->index] == '"' ||
    ctx->json[ctx->index] == '\''
  ) {
    size_t len;
    const char* str = parseStr(ctx, &len);
    val->type = STRING;
    val->value.string.size = len;
    val->value.string.data = str;
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
