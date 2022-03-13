#include "request_queue.h"

void RequestQueue::AddRequest(int results_num) {
    ++current_time_;
    while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
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