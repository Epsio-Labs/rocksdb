#pragma once

#include <memory>
#include "rocksdb/memory_allocator.h"
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "table/block_based/block_type.h"

namespace ROCKSDB_NAMESPACE {

// C function pointer types for FFI
typedef int (*custom_cache_lookup_fn)(const void* user_data, const char* key, int block_type, const char** data);

typedef int (*custom_cache_insert_fn)(const void* user_data, const char* key, int block_type,
                                      const char* data, size_t data_len);

typedef void (*custom_cache_delete_fn)(const void* user_data, const char* data);


// MemoryAllocator wrapper for custom cache delete function
class CustomCacheMemoryAllocator : public MemoryAllocator {
 public:
  CustomCacheMemoryAllocator(custom_cache_delete_fn delete_fn, const void* user_data)
      : delete_fn_(delete_fn),
        user_data_(user_data) {}

  const char* Name() const override { return "CustomCacheMemoryAllocator"; }

  void* Allocate(size_t size) override {
    // This allocator is only used for deallocation, not allocation
    (void)size;
    assert(false);
    return nullptr;
  }

  void Deallocate(void* p) override {
    if (delete_fn_ && p) {
      delete_fn_(user_data_, static_cast<const char*>(p));
    }
  }

 private:
  custom_cache_delete_fn delete_fn_;
  const void* user_data_;
};

// CustomCache provides caching for SST file blocks
// This will serve as an FFI wrapper to a Rust-based page cache implementation
class CustomCache {
 public:
  CustomCache(custom_cache_lookup_fn lookup_fn,
              custom_cache_insert_fn insert_fn,
              custom_cache_delete_fn delete_fn,
              const void* user_data);
  ~CustomCache() = default;

  // Lookup a block in the cache
  // key: 16-byte cache key (from CacheKey::AsSlice())
  // block_type: type of block (data, index, filter, etc.)
  // data: output buffer to store the block data if found
  // size: output parameter for the size of the data
  // Returns Status::OK() if found, Status::NotFound() if not in cache
  Status Lookup(const Slice& key, BlockType block_type, char** data);

  // Insert a block into the cache
  // key: 16-byte cache key (from CacheKey::AsSlice())
  // block_type: type of block (data, index, filter, etc.)
  // data: pointer to the block data
  // size: size of the block data
  Status Insert(const Slice& key, BlockType block_type, const char* data,
                size_t size);

  // Get the memory allocator for deallocating buffers from this cache
  MemoryAllocator *GetMemoryAllocator() const {
    return memory_allocator_.get();
  }

 private:
  custom_cache_lookup_fn lookup_fn_;
  custom_cache_insert_fn insert_fn_;
  custom_cache_delete_fn delete_fn_;
  const void* user_data_;
  std::unique_ptr<CustomCacheMemoryAllocator> memory_allocator_;
};

}  // namespace ROCKSDB_NAMESPACE