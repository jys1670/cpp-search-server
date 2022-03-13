#pragma once

#include <deque>
#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer &search_server) : search_server_{search_server}, empty_requests_{0},
                                                               current_time_{0} {};

    template<typename DocumentFilter>
    vector<Document> AddFindRequest(const string &raw_query, DocumentFilter doc_filter) {
        const auto result = search_server_.FindTopDocuments(raw_query, doc_filter);
        AddRequest(result.size());
        return result;
    }

    vector<Document> AddFindRequest(const string &raw_query, DocumentStatus status) {
        const auto result = search_server_.FindTopDocuments(raw_query, status);
        AddRequest(result.size());
        return result;
    }

    vector<Document> AddFindRequest(const string &raw_query) {
        const auto result = search_server_.FindTopDocuments(raw_query);
        AddRequest(result.size());
        return result;
    }

    int GetNoResultRequests() const {
        return empty_requests_;
    }

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