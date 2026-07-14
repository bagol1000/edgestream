//Robin Hood open-addressing hash set for uint64 edge keys.
//Sentinel UINT64_MAX is never a valid edge key (it would require a self-loop, which add_edge rejects).
//Power-of-two capacity from 1024, rehash to 2x past 0.7 load, linear probing;
//erase uses backward-shift deletion (no tombstones, probe invariants preserved).
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace edgestream {

class FlatHashSet {
public:
    static constexpr uint64_t EMPTY = UINT64_MAX;

    explicit FlatHashSet(size_t initial_capacity = 1024) {
        size_t cap = 1024;
        while (cap < initial_capacity) cap <<= 1;
        keys_.assign(cap, EMPTY);
        mask_ = cap - 1;
        size_ = 0;
    }

    size_t size() const { return size_; }

    void reserve(size_t n) {
        size_t cap = keys_.size();
        while (n * 10 > cap * 7) cap <<= 1;
        if (cap > keys_.size()) rehash(cap);
    }

    void clear() {
        std::fill(keys_.begin(), keys_.end(), EMPTY);
        size_ = 0;
    }

    bool contains(uint64_t key) const {
        size_t pos = home(key);
        size_t dist = 0;
        while (true) {
            uint64_t slot = keys_[pos];
            if (slot == EMPTY) return false;
            if (slot == key) return true;
            //occupant closer to home than our probe distance => key cannot be further along
            if (probe_distance(slot, pos) < dist) return false;
            pos = (pos + 1) & mask_;
            ++dist;
        }
    }

    //returns true if newly inserted, false if already present
    bool insert(uint64_t key) {
        if ((size_ + 1) * 10 > keys_.size() * 7) rehash(keys_.size() << 1);
        return insert_no_resize(key);
    }

    //returns true if the key was present and removed
    bool erase(uint64_t key) {
        size_t pos = home(key);
        size_t dist = 0;
        while (true) {
            uint64_t slot = keys_[pos];
            if (slot == EMPTY) return false;
            if (slot == key) break;
            if (probe_distance(slot, pos) < dist) return false;
            pos = (pos + 1) & mask_;
            ++dist;
        }
        //backward shift: pull each displaced successor one slot closer to home
        size_t next = (pos + 1) & mask_;
        while (keys_[next] != EMPTY && probe_distance(keys_[next], next) > 0) {
            keys_[pos] = keys_[next];
            pos = next;
            next = (next + 1) & mask_;
        }
        keys_[pos] = EMPTY;
        --size_;
        return true;
    }

private:
    std::vector<uint64_t> keys_;
    size_t mask_ = 0;
    size_t size_ = 0;

    static uint64_t mix(uint64_t key) {
        //splitmix64 finaliser
        key ^= key >> 30;
        key *= 0xbf58476d1ce4e5b9ULL;
        key ^= key >> 27;
        key *= 0x94d049bb133111ebULL;
        key ^= key >> 31;
        return key;
    }

    size_t home(uint64_t key) const { return mix(key) & mask_; }

    size_t probe_distance(uint64_t slot_key, size_t pos) const { return (pos - home(slot_key)) & mask_; }

    bool insert_no_resize(uint64_t key) {
        uint64_t cur = key;
        size_t pos = home(cur);
        size_t curdist = 0;
        bool committed = false;   //once true the original key is already placed
        while (true) {
            uint64_t slot = keys_[pos];
            if (slot == EMPTY) {
                keys_[pos] = cur;
                ++size_;
                return true;
            }
            if (!committed && slot == key) return false;
            size_t slotdist = probe_distance(slot, pos);
            if (slotdist < curdist) {
                std::swap(keys_[pos], cur);   //steal the richer slot
                curdist = slotdist;
                committed = true;
            }
            pos = (pos + 1) & mask_;
            ++curdist;
        }
    }

    void rehash(size_t new_capacity) {
        std::vector<uint64_t> old = std::move(keys_);
        keys_.assign(new_capacity, EMPTY);
        mask_ = new_capacity - 1;
        size_ = 0;
        for (uint64_t k : old)
            if (k != EMPTY) insert_no_resize(k);
    }
};

}
