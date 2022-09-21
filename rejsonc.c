#include "redismodule.h"
#include "value.h"

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

  long long num;
  RedisModule_StringToLongLong(argv[3], &num);
  
  JsonValue* ageV = allocNumber(num);

  JsonValue* v =  allocObject(1);
  JsonKeyVal* kv = RedisModule_Calloc(1, sizeof(JsonKeyVal));
  kv->key = "age";
  kv->value = ageV;
  v->value.object.elements[0] = kv;
  if(keyType == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ModuleTypeSetValue(key, jsonType, v);
  } else {
    JsonValue* v1 = RedisModule_ModuleTypeGetValue(key);
    *v1 = *v;
  }

  RedisModule_ReplyWithStringBuffer(ctx, json, len);

  return REDISMODULE_OK;
}

int JsonGetRedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if(argc < 2) {
    RedisModule_WrongArity(ctx);
    return REDISMODULE_ERR;
  }
  RedisModule_AutoMemory(ctx);
  
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

  JsonValue* v = RedisModule_ModuleTypeGetValue(key);
  RedisModule_Log(
    ctx,
    REDISMODULE_LOGLEVEL_VERBOSE,
    "type %d objlen %llu key %s valuetype %d value %lld",
    v->type,
    v->value.object.size,
    v->value.object.elements[0]->key,
    v->value.object.elements[0]->value->type,
    v->value.object.elements[0]->value->value.number
  );

  RedisModule_ReplyWithLongLong(ctx, v->type);
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
  if (RedisModule_Init(ctx, "rejsoncpp", 0, REDISMODULE_APIVER_1) ==
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
    "rejsoncpp",
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
