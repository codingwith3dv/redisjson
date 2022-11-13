#include "redismodule.h"
#include "string.h"
#include "value.h"

void loadObject(
  RedisModuleIO* rdb,
  struct JsonObject* object
) {
  object->elements = RedisModule_Calloc(object->size, sizeof(JsonKeyVal*));
  for(size_t i = 0; i < object->size; i++) {
    JsonKeyVal* keyVal = RedisModule_Calloc(1, sizeof(JsonKeyVal));
    size_t len;
    keyVal->key = RedisModule_LoadStringBuffer(rdb, &len);
    keyVal->value = RedisModule_Calloc(1, sizeof(JsonValue));
    JsonTypeRdbLoadImpl(rdb, keyVal->value);
    object->elements[i] = keyVal;
  }
}

static void loadSimpleJson(RedisModuleIO* rdb, JsonValue* value) {
  value->type = RedisModule_LoadUnsigned(rdb);
  switch(value->type) {
    case NUMBER: {
      value->value.number = RedisModule_LoadSigned(rdb);
      break;
    }
    case OBJECT: {
      value->value.object.size = RedisModule_LoadUnsigned(rdb);
      break;
    }
    case STRING: {
      value->value.string.data =
        RedisModule_LoadStringBuffer(
          rdb,
          &value->value.string.size
        );
      break;
    }
  }
}

void JsonTypeRdbLoadImpl(RedisModuleIO* rdb, JsonValue* value) {
  loadSimpleJson(rdb, value);
  switch(value->type) {
    case OBJECT: {
      loadObject(rdb, &value->value.object);
      break;
    }
    default: break;
  }
}

static void saveSimpleJson(RedisModuleIO* rdb, JsonValue* value) {
  RedisModule_SaveUnsigned(rdb, value->type);
  switch(value->type) {
    case NUMBER: {
      RedisModule_SaveSigned(rdb, value->value.number);
      break;
    }
    case OBJECT: {
      RedisModule_SaveUnsigned(rdb, value->value.object.size);
      break;
    }
    case STRING: {
      RedisModule_SaveStringBuffer(
        rdb,
        value->value.string.data,
        value->value.string.size
      );
      break;
    }
  }
}

void JsonTypeRdbSaveImpl(RedisModuleIO* rdb, JsonValue* value) {
  saveSimpleJson(rdb, value);
  if(value->type == OBJECT) {
    struct JsonObject* object = &value->value.object;
    for(int i = 0; i < object->size; i++) {
      JsonKeyVal* keyVal = object->elements[i];
      size_t keySize = strlen(keyVal->key);
      RedisModule_SaveStringBuffer(rdb, keyVal->key, keySize);
      JsonTypeRdbSaveImpl(rdb, keyVal->value);
    }
  }
}

static void freeKeyValue(JsonKeyVal* keyValue) {
  JsonTypeFreeImpl(keyValue->value);
  RedisModule_Free(keyValue);
}

static void freeObject(struct JsonObject* object) {
  for(size_t i = 0; i < object->size; i++) {
    freeKeyValue(object->elements[i]);
  }
  if(object->elements) RedisModule_Free(object->elements);
  RedisModule_Free(object);
}

void JsonTypeFreeImpl(JsonValue* value) {
  switch(value->type) {
    case OBJECT:
      freeObject(&value->value.object);
      break;
    case STRING:
      RedisModule_Free((void*)value->value.string.data);
      RedisModule_Free(value);
      break;
    default: RedisModule_Free(value);
  }
}
