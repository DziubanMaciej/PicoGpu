#pragma once

#include <cstddef>

template <typename KeyT, typename ValueT, size_t cacheSize, KeyT invalidKey>
class EntryCache {
public:
    struct Entry {
        KeyT key = invalidKey;
        ValueT value = {};
    };

    EntryCache() {
        for (int i = 0; i < cacheSize; i++) {
            lru[i] = i;
        }
    }

    ValueT *get(KeyT key);
    ValueT *put(KeyT key, ValueT &&value);

private:
    Entry entries[cacheSize] = {};
    int lru[cacheSize]; // first element ` index of last recently used cache entry, last element is will be removed in case of cache miss
};

template <typename KeyT, typename ValueT, size_t cacheSize, KeyT invalidKey>
ValueT *EntryCache<KeyT, ValueT, cacheSize, invalidKey>::get(KeyT key) {
    for (int i = 0; i < cacheSize; i++) {
        if (entries[i].key == key) {
            // TODO promote this index to the beginning of LRU

            return &entries[i].value;
        }
    }
    return nullptr;
}

template <typename KeyT, typename ValueT, size_t cacheSize, KeyT invalidKey>
ValueT *EntryCache<KeyT, ValueT, cacheSize, invalidKey>::put(KeyT key, ValueT &&value) {
    // Index of least (not last) recently used entry is at the end of LRU structure. Shift it to the right and
    // store new entry at index 0. This could be implemented more efficiently with a ring buffer, but cache size
    // will generally be small, so it shouldn't matter that much.
    int indexInCache = lru[cacheSize - 1];
    for (int i = cacheSize - 1; i >= 1; i--) {
        lru[i] = lru[i - 1];
    }
    lru[0] = indexInCache;

    // Move the new value into our new slot
    entries[indexInCache].key = key;
    entries[indexInCache].value = std::move(value);

    return &entries[indexInCache].value;
}
