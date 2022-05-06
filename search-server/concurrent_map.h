#pragma once

#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace std;

template <typename Key, typename Value> class ConcurrentMap {
private:
  struct Bucket {
    mutex mut;
    map<Key, Value> dict;
  };

public:
  static_assert(is_integral_v<Key>, "ConcurrentMap supports only integer keys");

  struct Access {
    lock_guard<mutex> guard;
    Value &ref_to_value;

    Access(const Key &key, Bucket &bucket)
        : guard(bucket.mut), ref_to_value(bucket.dict[key]) {}
  };

  explicit ConcurrentMap(size_t bucket_count) : buckets_(bucket_count) {}

  Access operator[](const Key &key) {
    auto &bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
    return {key, bucket};
  }

  map<Key, Value> BuildOrdinaryMap() {
    map<Key, Value> result;
    for (auto &[mut, dict] : buckets_) {
      lock_guard g(mut);
      result.insert(dict.begin(), dict.end());
    }
    return result;
  }

private:
  vector<Bucket> buckets_;
};