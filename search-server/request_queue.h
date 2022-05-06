#pragma once

#include "document.h"
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
  explicit RequestQueue(const SearchServer &search_server);

  template <typename DocumentFilter>
  vector<Document> AddFindRequest(const string &raw_query,
                                  DocumentFilter doc_filter);

  vector<Document> AddFindRequest(const string &raw_query,
                                  DocumentStatus status);

  vector<Document> AddFindRequest(const string &raw_query);

  int GetNoResultRequests() const;

private:
  const SearchServer &search_server_;
  struct QueryResult {
    int result_size;
    uint64_t timestamp;
  };
  deque<QueryResult> requests_;
  int empty_requests_;
  uint64_t current_time_;
  const static int min_in_day_ = 1440;

  void AddRequest(int results_num);
};

template <typename DocumentFilter>
vector<Document> RequestQueue::AddFindRequest(const string &raw_query,
                                              DocumentFilter doc_filter) {
  const auto result = search_server_.FindTopDocuments(raw_query, doc_filter);
  AddRequest(result.size());
  return result;
}