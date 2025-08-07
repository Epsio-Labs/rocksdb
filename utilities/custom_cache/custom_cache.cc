#include "utilities/custom_cache/custom_cache.h"

#include <cstdlib>

namespace ROCKSDB_NAMESPACE {

CustomCache::CustomCache(custom_cache_lookup_fn lookup_fn,
                         custom_cache_insert_fn insert_fn,
                         custom_cache_delete_fn delete_fn,
                         const void* user_data)
    : lookup_fn_(lookup_fn),
      insert_fn_(insert_fn),
      delete_fn_(delete_fn),
      user_data_(user_data),
      memory_allocator_(delete_fn
                            ? std::make_unique<CustomCacheMemoryAllocator>(
                                  delete_fn, user_data)
                            : nullptr) {}

Status CustomCache::Lookup(const Slice& key, BlockType block_type, char** data) {
  const char* result_data = nullptr;

  int status = lookup_fn_(user_data_, key.data(), static_cast<int>(block_type), &result_data);

  if (status == 0) {  // Success
    *data = (char *) result_data;
    return Status::OK();
  } else if (status == 1) {  // Not found
    return Status::NotFound();
  } else {  // Error
    return Status::IOError("Custom cache lookup failed");
  }
}

Status CustomCache::Insert(const Slice& key, BlockType block_type,
                           const char* data, size_t size) {
  if (!insert_fn_) {
    return Status::OK();  // No-op if no insert function
  }

  int status = insert_fn_(user_data_, key.data(), static_cast<int>(block_type), data, size);

  if (status == 0) {  // Success
    return Status::OK();
  } else {  // Error
    return Status::IOError("Custom cache insert failed");
  }
}

}  // namespace ROCKSDB_NAMESPACE