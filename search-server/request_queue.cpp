#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer &search_server)
    : search_server_{search_server}, empty_requests_{0}, current_time_{0} {}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query,
                                              DocumentStatus status) {
  const auto result = search_server_.FindTopDocuments(raw_query, status);
  AddRequest(result.size());
  return result;
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query) {
  const auto result = search_server_.FindTopDocuments(raw_query);
  AddRequest(result.size());
  return result;
}

int RequestQueue::GetNoResultRequests() const { return empty_requests_; }

void RequestQueue::AddRequest(int results_num) {
  ++current_time_;
  while (!requests_.empty() &&
         min_in_day_ <= current_time_ - requests_.front().timestamp) {
    if (0 == requests_.front().result_size) {
      --empty_requests_;
    }
    requests_.pop_front();
  }
  requests_.push_back({results_num, current_time_});
  if (0 == results_num) {
    ++empty_requests_;
  }
}
