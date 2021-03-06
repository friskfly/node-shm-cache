#include "shmcache_wrapper.h"

// Wrapper Impl

Nan::Persistent<v8::Function> ShmCacheWrapper::constructor;

NAN_MODULE_INIT(ShmCacheWrapper::Init) {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ShmCacheWrapper").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "Set", Set);
  Nan::SetPrototypeMethod(tpl, "Get", Get);
  Nan::SetPrototypeMethod(tpl, "Remove", Remove);
  Nan::SetPrototypeMethod(tpl, "Stats", Stats);
  Nan::SetPrototypeMethod(tpl, "Clear", Clear);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("ShmCacheWrapper").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

ShmCacheWrapper::ShmCacheWrapper(struct shmcache_config config) {
  this->config = config;
  log_init();
  if (shmcache_init(&this->context, &config, true, true) != 0) {
    Nan::ThrowError(Nan::New<v8::String>("init shmcache err").ToLocalChecked());
  }
}

ShmCacheWrapper::~ShmCacheWrapper() {
}

NAN_METHOD(ShmCacheWrapper::New) {
  if (info.IsConstructCall()) {
    v8::Local<v8::Object> cfgObj = Nan::To<v8::Object>(info[0]).ToLocalChecked();
    v8::Local<v8::Object> vaObj = Nan::To<v8::Object>(Nan::Get(cfgObj, 
            Nan::New<v8::String>("vaPolicy").ToLocalChecked()).ToLocalChecked()).ToLocalChecked();
    v8::Local<v8::Object> lockObj = Nan::To<v8::Object>(Nan::Get(cfgObj, 
            Nan::New<v8::String>("lockPolicy").ToLocalChecked()).ToLocalChecked()).ToLocalChecked();
    struct shmcache_config config;

    v8::String::Utf8Value filenameString(cfgObj->Get(Nan::New<v8::String>("filename").ToLocalChecked()));

    strncpy(config.filename, *filenameString, filenameString.length());
    config.max_memory = cfgObj->Get(Nan::New<v8::String>("maxMemory").ToLocalChecked())->Int32Value();
    config.segment_size = cfgObj->Get(Nan::New<v8::String>("segmentSize").ToLocalChecked())->Int32Value();
    config.max_key_count = cfgObj->Get(Nan::New<v8::String>("maxKeyCount").ToLocalChecked())->Int32Value();
    config.max_value_size = cfgObj->Get(Nan::New<v8::String>("maxValueSize").ToLocalChecked())->Int32Value();
    config.type = cfgObj->Get(Nan::New<v8::String>("type").ToLocalChecked())->Int32Value();
    config.recycle_key_once = cfgObj
        ->Get(Nan::New<v8::String>("recycleKeyOnce").ToLocalChecked())->Int32Value();

    config.va_policy.avg_key_ttl = vaObj->Get(Nan::New<v8::String>("avgKeyTTL").ToLocalChecked())->Int32Value();
    config.va_policy.discard_memory_size = vaObj->Get(
            Nan::New<v8::String>("discardMemorySize").ToLocalChecked())->Int32Value();
    config.va_policy.max_fail_times = vaObj->Get(
            Nan::New<v8::String>("maxFailTimes").ToLocalChecked())->Int32Value();
    config.va_policy.sleep_us_when_recycle_valid_entries = vaObj->Get(
            Nan::New<v8::String>("seelpUsWhenRecycleValidEntries").ToLocalChecked())->Int32Value();

    config.lock_policy.trylock_interval_us = lockObj->Get(
            Nan::New<v8::String>("tryLockIntervalUs").ToLocalChecked())->Int32Value();
    config.lock_policy.detect_deadlock_interval_ms = lockObj->Get(
            Nan::New<v8::String>("detect_deadlock_interval_ms").ToLocalChecked())->Int32Value();

    config.hash_func = simple_hash;

    ShmCacheWrapper *obj = new ShmCacheWrapper(config);
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    const int argc = 1; 
    v8::Local<v8::Value> argv[argc] = {info[0]};
    v8::Local<v8::Function> cons = Nan::New(constructor);
    info.GetReturnValue().Set(cons->NewInstance(argc, argv));
  }
}

NAN_METHOD(ShmCacheWrapper::Get) {
  ShmCacheWrapper* obj = Nan::ObjectWrap::Unwrap<ShmCacheWrapper>(info.This());
  struct shmcache_key_info key;
  struct shmcache_value_info value;
  v8::String::Utf8Value keyString(Nan::To<v8::String>(info[0]).ToLocalChecked());

  key.data = *keyString;
  key.length = keyString.length();

  int result = shmcache_get(&obj->context, &key, &value);
  if (result == 0) {
      info.GetReturnValue().Set(Nan::New<v8::String>(value.data, value.length).ToLocalChecked());
  } else {
      info.GetReturnValue().Set(Nan::Undefined());
  }
}

NAN_METHOD(ShmCacheWrapper::Set) {
  ShmCacheWrapper* obj = Nan::ObjectWrap::Unwrap<ShmCacheWrapper>(info.This());
  struct shmcache_key_info key;
  v8::String::Utf8Value keyString(Nan::To<v8::String>(info[0]).ToLocalChecked());
  v8::String::Utf8Value valueString(Nan::To<v8::String>(info[1]).ToLocalChecked());
  int ttl = info[2]->Int32Value();

  key.data = *keyString;
  key.length = keyString.length();

  int result = shmcache_set(&obj->context, &key, *valueString, valueString.length(), ttl);
  if (result == 0) {
      info.GetReturnValue().Set(Nan::True());
  } else {
      info.GetReturnValue().Set(Nan::False());
  }
}

NAN_METHOD(ShmCacheWrapper::Remove) {
  ShmCacheWrapper* obj = Nan::ObjectWrap::Unwrap<ShmCacheWrapper>(info.This());
  struct shmcache_key_info key;
  v8::String::Utf8Value keyString(Nan::To<v8::String>(info[0]).ToLocalChecked());

  key.data = *keyString;
  key.length = keyString.length();

  int result = shmcache_delete(&obj->context, &key);
  if (result == 0) {
      info.GetReturnValue().Set(Nan::True());
  } else {
      info.GetReturnValue().Set(Nan::False());
  }
}

NAN_METHOD(ShmCacheWrapper::Stats) {
  ShmCacheWrapper* obj = Nan::ObjectWrap::Unwrap<ShmCacheWrapper>(info.This());
  struct shmcache_stats stats;
  shmcache_stats(&obj->context, &stats);

  v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
  v8::Local<v8::Object> memObj = Nan::New<v8::Object>();
  v8::Local<v8::Object> tableObj = Nan::New<v8::Object>();
  v8::Local<v8::Object> lockObj = Nan::New<v8::Object>();

  retObj->Set(Nan::New<v8::String>("memory").ToLocalChecked(), memObj);
  retObj->Set(Nan::New<v8::String>("hashTable").ToLocalChecked(), tableObj);
  retObj->Set(Nan::New<v8::String>("lock").ToLocalChecked(), tableObj);

  memObj->Set(Nan::New<v8::String>("total").ToLocalChecked(),
          Nan::New<v8::Number>(stats.memory.max));
  memObj->Set(Nan::New<v8::String>("allocated").ToLocalChecked(),
          Nan::New<v8::Number>(stats.memory.usage.alloced));
  memObj->Set(Nan::New<v8::String>("used").ToLocalChecked(),
          Nan::New<v8::Number>(stats.memory.used));

  tableObj->Set(Nan::New<v8::String>("maxKeyCount").ToLocalChecked(),
          Nan::New<v8::Number>(stats.max_key_count));
  tableObj->Set(Nan::New<v8::String>("currentKeyCount").ToLocalChecked(),
          Nan::New<v8::Number>(stats.hashtable.count));

  lockObj->Set(Nan::New<v8::String>("totalCount").ToLocalChecked(),
          Nan::New<v8::Number>(stats.shm.lock.total));
  lockObj->Set(Nan::New<v8::String>("retryCount").ToLocalChecked(),
          Nan::New<v8::Number>(stats.shm.lock.retry));
  lockObj->Set(Nan::New<v8::String>("detectDeadLockCount").ToLocalChecked(),
          Nan::New<v8::Number>(stats.shm.lock.detect_deadlock));
  lockObj->Set(Nan::New<v8::String>("unlockDeadLockCount").ToLocalChecked(),
          Nan::New<v8::Number>(stats.shm.lock.unlock_deadlock));

  info.GetReturnValue().Set(retObj);
}

NAN_METHOD(ShmCacheWrapper::Clear) {
  ShmCacheWrapper* obj = Nan::ObjectWrap::Unwrap<ShmCacheWrapper>(info.This());

  int result = shmcache_remove_all(&obj->context);
  if (result == 0) {
      info.GetReturnValue().Set(Nan::True());
  } else {
      info.GetReturnValue().Set(Nan::False());
  }
}
