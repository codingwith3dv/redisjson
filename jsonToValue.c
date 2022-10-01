#include "jsonToValue.h"
#include <string.h>

typedef struct {
  const char* json;
  size_t index;
  RedisModuleCtx* rctx;
} ParserContext;

void skipSpace(ParserContext* ctx) {
  while(
    ctx->json[ctx->index] == ' ' ||
    ctx->json[ctx->index] == '\n'
  ) {
    ++ctx->index;
  }
}

const char* parseStr(ParserContext* ctx) {
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
    size_t len = (end - start) + 1;
    char* str = RedisModule_Alloc(len * sizeof(char));
    memcpy(str, start, len);
    str[len - 1] = '\0';
    return str;
  }
  return 0;
}

void parseObject(ParserContext* ctx, JsonValue* val) {
  ++ctx->index;
  while(ctx->json[ctx->index] != '}') {
    const char* key = parseStr(ctx);
    ++ctx->index;
  }
}

JsonValue* parseValue(ParserContext* ctx) {
  JsonValue* val = RedisModule_Calloc(1, sizeof(JsonValue));
  if(ctx->json[ctx->index] == '{') {
    parseObject(ctx, val);
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
