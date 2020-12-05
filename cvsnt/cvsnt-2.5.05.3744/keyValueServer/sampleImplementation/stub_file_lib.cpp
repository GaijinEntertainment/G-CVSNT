#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include "../blob_server_func_deps.h"
//this is sample implementation of low-level functions for key-value server
//it is totally working, but inmem only (no files)


//container that accessed only under mutex
//value inside of container allows concurrent reads
//so, it is important that pointers to data not invalidates on push. I.e. value has to be on heap or in files.
static std::mutex mtx;
static std::unordered_map<std::string, std::vector<uint8_t>> storage;

size_t blob_get_hash_blob_size(const char* htype, const char* hhex) {
  std::lock_guard<std::mutex> lck (mtx);
  auto it = storage.find(std::string(hhex));
  if (it != storage.end())
    return it->second.size();
  return 0;
}

bool blob_does_hash_blob_exist(const char* htype, const char* hhex) {
  std::lock_guard<std::mutex> lck (mtx);
  return storage.find(std::string(hhex)) != storage.end();
}

typedef std::pair<std::string, std::vector<uint8_t>> PushData;

uintptr_t blob_start_push_data(const char* htype, const char* hhex, uint64_t size){
  std::lock_guard<std::mutex> lck (mtx);
  auto it = storage.find(std::string(hhex));
  if (it != storage.end())
    return uintptr_t(1);//magic value, nothing to push
  return uintptr_t(new PushData(hhex, std::vector<uint8_t>()));
}

bool blob_push_data(const void *data, size_t data_size, uintptr_t up)
{
  if (up == uintptr_t(1))
    return true;
  if (up == uintptr_t(0))
    return false;
  ((PushData*)up)->second.insert(((PushData*)up)->second.end(), (const uint8_t*)data, (const uint8_t*)data + data_size);
  return true;
}

bool blob_end_push_data(uintptr_t up){
  if (up == uintptr_t(1))
    return true;
  if (up == uintptr_t(0))
    return false;
  std::unique_ptr<PushData> pd((PushData*)up);
  std::lock_guard<std::mutex> lck (mtx);
  if (storage.find(pd->first) != storage.end())//already put
    return true;
  //this is sample, so we just trust that hash was valid
  storage.emplace(std::move(pd->first), std::move(pd->second));
  return true;
}

void blob_destroy_push_data(uintptr_t up)
{
  if (up == uintptr_t(1) || up == uintptr_t(0))
    return;
  delete (PushData*)up;
}

uintptr_t blob_start_pull_data(const char* htype, const char* hhex, uint64_t &sz)
{
  std::lock_guard<std::mutex> lck (mtx);
  auto it = storage.find(std::string(hhex));
  return (it != storage.end()) ? uintptr_t(&it->second) : uintptr_t(0);//pointer never changes, as we don't allow overwriting data
}

const char *blob_pull_data(uintptr_t up, uint64_t from, size_t &read){
  if (!up){read = 0;return nullptr;}
  auto &v = *(const std::vector<uint8_t>*)up;
  if (v.size() < from)
    {read = 0;return nullptr;}
  read = v.size() - from;
  return (const char*)(v.data()+from);
}

bool blob_end_pull_data(uintptr_t up){return true;}

