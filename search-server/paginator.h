#pragma once

#include <cassert>
#include <iostream>
#include <vector>

using namespace std;

template <typename It> class IteratorRange {
public:
  explicit IteratorRange(It start, It end)
      : start_iter_{start}, end_iter_{end}, size_(distance(start, end)){};

  auto begin() const { return start_iter_; }

  auto end() const { return end_iter_; }

  auto size() const { return size_; }

private:
  It start_iter_, end_iter_;
  size_t size_;
};

template <typename It> class Paginator {
public:
  explicit Paginator(It range_begin, It range_end, size_t page_size) {
    assert(range_end >= range_begin && page_size > 0);
    tot_results_ = distance(range_begin, range_end);
    tot_pages_ = IntCeiling(tot_results_, page_size);
    for (size_t i = 0; i < tot_pages_; ++i) {
      It start = next(range_begin, page_size * i);
      It end = next(range_begin, min(tot_results_, page_size * (i + 1)));
      pages_.push_back(IteratorRange{start, end});
    }
  };

  auto begin() const {
    pages_.begin();
    return pages_.begin();
  }

  auto end() const { return pages_.end(); }

private:
  size_t tot_results_, tot_pages_;
  vector<IteratorRange<It>> pages_;

  size_t IntCeiling(size_t divisible, size_t divisor) {
    return 1 + ((divisible - 1) / divisor);
  }
};

template <typename Container>
auto Paginate(const Container &c, size_t page_size) {
  return Paginator(begin(c), end(c), page_size);
}

template <typename It>
ostream &operator<<(ostream &os, IteratorRange<It> range) {
  for (auto it = range.begin(); it != range.end(); ++it) {
    os << *it;
  };
  return os;
}
