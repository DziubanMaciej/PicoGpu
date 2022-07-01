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
    void promoteInLru(int index) {
        // Promotes a value store at given index in LRU structure to the top (make it last recently used). 
        // This is essentially a rotation of values at a specified range - from 0 to index.
        int promotedIndex = lru[index];
        for (int i = index; i >= 1; i--) {
            lru[i] = lru[i - 1];
        }
        lru[0] = promotedIndex;
    }

    Entry entries[cacheSize] = {};
    int lru[cacheSize]; // first element ` index of last recently used cache entry, last element is will be removed in case of cache miss
};

template <typename KeyT, typename ValueT, size_t cacheSize, KeyT invalidKey>
ValueT *EntryCache<KeyT, ValueT, cacheSize, invalidKey>::get(KeyT key) {
    for (int lruIndex = 0; lruIndex < cacheSize; lruIndex++) {
        Entry &entry = entries[lru[lruIndex]];
        if (entry.key == key) {
            promoteInLru(lruIndex);
            return &entry.value;
        }
    }
    return nullptr;
}

template <typename KeyT, typename ValueT, size_t cacheSize, KeyT invalidKey>
ValueT *EntryCache<KeyT, ValueT, cacheSize, invalidKey>::put(KeyT key, ValueT &&value) {
    // Index of least (not last) recently used entry is at the end of LRU structure.
    promoteInLru(cacheSize - 1);
    int indexInCache = lru[0];

    // Move the new value into our new slot
    entries[indexInCache].key = key;
    entries[indexInCache].value = std::move(value);

    return &entries[indexInCache].value;
}
