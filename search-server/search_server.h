#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include "document.h"

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_PRECISION = 1e-6;

class SearchServer {
public:
    SearchServer() = default;

    explicit SearchServer(string stopwords);

    template<typename Iterable>
    explicit SearchServer(Iterable stopwords);

    bool SetStopWords(const string &text);

    void AddDocument(int document_id, const string &document,
                     DocumentStatus status, const vector<int> &ratings);

    template<typename DocumentFilter>
    [[nodiscard]] vector<Document> FindTopDocuments(
            const string &raw_query, DocumentFilter doc_filter) const;

    [[nodiscard]] vector<Document> FindTopDocuments(
            const string &raw_query, DocumentStatus allowed_status) const;

    [[nodiscard]] vector<Document> FindTopDocuments(
            const string &raw_query) const;

    [[nodiscard]] tuple<vector<string>, DocumentStatus> MatchDocument(
            const string &raw_query, int document_id) const;

    int GetDocumentCount() const { return total_docs_; }

    const map<string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    auto begin() const {
        return documents_ids_.begin();
    }
    auto end() const {
        return documents_ids_.end();
    }


private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    map<string, map<int, double>> word_to_docs_freq_;
    map<int, map<string, double>> doc_to_words_freq_;
    map<int, DocumentData> documents_;
    set<string> stop_words_;
    set<int> documents_ids_;
    int total_docs_ = 0;

    static int ComputeAverageRating(const vector<int> &ratings);

    double ComputeWordInvDocFreq(const string &word) const;

    bool IsStopWord(const string &word) const;

    static bool ContainsSpecialChars(const string &text);

    vector<string> SplitIntoWordsNoStop(const string &text) const;

    QueryWord ParseQueryWord(string text) const;

    optional<Query> ParseQuery(const string &text) const;

    template<typename DocumentFilter>
    vector<Document> FindAllDocuments(const Query &query,
                                      DocumentFilter doc_filter) const;
};

template<typename Iterable>
SearchServer::SearchServer(Iterable stopwords) {
    for (auto &word: stopwords) {
        if (!word.empty()) {
            if (ContainsSpecialChars(word)) {
                throw invalid_argument("Special characters are not allowed");
            } else {
                stop_words_.insert(word);
            }
        }
    };
}

template<typename DocumentFilter>
vector<Document> SearchServer::FindTopDocuments(
        const string &raw_query, DocumentFilter doc_filter) const {
    auto query = ParseQuery(raw_query);
    if (ContainsSpecialChars(raw_query) || !query) {
        throw invalid_argument("Incorrect search query");
    }
    auto matched_documents = FindAllDocuments(*query, doc_filter);
    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
             if (abs(rhs.relevance - lhs.relevance) < RELEVANCE_PRECISION) {
                 return lhs.rating > rhs.rating;
             }
             return lhs.relevance > rhs.relevance;
         });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename DocumentFilter>
vector<Document> SearchServer::FindAllDocuments(
        const SearchServer::Query &query, DocumentFilter doc_filter) const {
    vector<Document> matched_documents;
    map<int, double> doc_to_relev;
    set<int> bad_docs;
    for (const auto &[word, docs]: word_to_docs_freq_) {
        if (query.minus_words.count(word)) {  // Minus word, ignore document
            for (auto &[id, _]: docs) bad_docs.insert(id);
            continue;
        }
        if (query.plus_words.count(word)) {  // Good word, compute relevance
            double inv_doc_freq = ComputeWordInvDocFreq(word);
            for (const auto &[id, term_freq]: docs) {
                if (doc_filter(id, documents_.at(id).status,
                               documents_.at(id).rating)) {
                    doc_to_relev[id] += inv_doc_freq * term_freq;
                }
            }
        }
    }

    // Remove documents that have minus words from result
    for (const auto id: bad_docs) doc_to_relev.erase(id);

    for (const auto &[id, rel]: doc_to_relev) {
        matched_documents.push_back({id, rel, documents_.at(id).rating});
    }
    return matched_documents;
}
