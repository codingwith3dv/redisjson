#include "redismodule.h"
#include "value.h"
#include "jsonToValue.h"
#include "path.h"

static RedisModuleType* jsonType;

void* JsonTypeRdbLoad(RedisModuleIO* rdb, int encver) {
  struct JsonValue* json;
  json = RedisModule_Calloc(1, sizeof(JsonValue));
  JsonTypeRdbLoadImpl(rdb, json);
  return json;
}

void JsonTypeRdbSave(RedisModuleIO* rdb, void* value) {
  JsonValue* json = (JsonValue*)value;
  JsonTypeRdbSaveImpl(rdb, json);
}

void JsonTypeFree(void* value) {
  JsonValue* json = (JsonValue*)value;
  if(json) {
    JsonTypeFreeImpl(json);
    RedisModule_Free(json);
  }
}

int JsonSetRedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if(argc != 4) {
    RedisModule_WrongArity(ctx);
    return REDISMODULE_ERR;
  }
  RedisModule_AutoMemory(ctx);
  
  RedisModuleKey* key = RedisModule_OpenKey(
    ctx,
    argv[1],
    REDISMODULE_READ | REDISMODULE_WRITE
  );
  int keyType = RedisModule_KeyType(key);
  if(
    REDISMODULE_KEYTYPE_EMPTY != keyType &&
    RedisModule_ModuleTypeGetType(key) != jsonType
  ) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    return REDISMODULE_ERR;
  }

  size_t len;
  const char* json = RedisModule_StringPtrLen(argv[3], &len);
  if(!len) {
    RedisModule_ReplyWithError(ctx, "ERR empty string is an invalid json value");
    return REDISMODULE_ERR;
  }

  JsonValue* val = parseJson(ctx, json);

  if(keyType == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ModuleTypeSetValue(key, jsonType, val);
  } else {
    JsonValue* v1 = RedisModule_ModuleTypeGetValue(key);
    *v1 = *val;
  }

  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

int JsonGetRedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if(argc < 3) {
    RedisModule_WrongArity(ctx);
    return REDISMODULE_ERR;
  }
  RedisModule_AutoMemory(ctx);
  
  if(RedisModule_KeyExists(ctx, argv[1]) == 0) {
    RedisModule_ReplyWithError(ctx, "Key does not exist");
    return REDISMODULE_ERR;
  }
  RedisModuleKey* key = RedisModule_OpenKey(
    ctx,
    argv[1],
    REDISMODULE_READ
  );
  int keyType = RedisModule_KeyType(key);
  if(
    REDISMODULE_KEYTYPE_EMPTY != keyType &&
    RedisModule_ModuleTypeGetType(key) != jsonType
  ) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    return REDISMODULE_ERR;
  }

  RedisModuleString* path = argv[2];
  JsonValue* v = RedisModule_ModuleTypeGetValue(key);
  v = evalPath(ctx, v, path)[0];
  if(!v) {
    RedisModule_ReplyWithError(ctx, "Path error");
    return REDISMODULE_ERR;
  }
  RedisModuleString* str = jsonToString(ctx, v);
  RedisModule_ReplyWithString(ctx, str);
  return REDISMODULE_OK;
}

int JsonDelRedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if(argc < 2) {
    RedisModule_WrongArity(ctx);
    return REDISMODULE_ERR;
  }
  RedisModule_AutoMemory(ctx);
  
  RedisModuleKey* key = RedisModule_OpenKey(
    ctx,
    argv[1],
    REDISMODULE_READ | REDISMODULE_WRITE
  );
  int keyType = RedisModule_KeyType(key);
  if(keyType == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ReplyWithLongLong(ctx, 0);
    return REDISMODULE_OK;
  }
  if(RedisModule_ModuleTypeGetType(key) != jsonType) {
    RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    return REDISMODULE_ERR;
  }

  RedisModule_DeleteKey(key);

  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (RedisModule_Init(ctx, "redisjson", 0, REDISMODULE_APIVER_1) ==
          REDISMODULE_ERR)
          return REDISMODULE_ERR;

  RedisModuleTypeMethods typeMethods = {
    .version = REDISMODULE_TYPE_METHOD_VERSION,
    .rdb_load = JsonTypeRdbLoad,
    .rdb_save = JsonTypeRdbSave,
    .aof_rewrite = NULL,
    .free = JsonTypeFree
  };

  jsonType = RedisModule_CreateDataType(
    ctx,
    "redisjson",
    0,
    &typeMethods
  );
  if(jsonType == NULL)
    return REDISMODULE_ERR;

  if(RedisModule_CreateCommand(
    ctx,
    "json.set",
    JsonSetRedisCommand,
    "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if(RedisModule_CreateCommand(
    ctx,
    "json.get",
    JsonGetRedisCommand,
    "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if(RedisModule_CreateCommand(
    ctx,
    "json.del",
    JsonDelRedisCommand,
    "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
