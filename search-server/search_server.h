#pragma once

#include <algorithm>
#include <cmath>
#include <execution>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "concurrent_map.h"
#include "document.h"

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_PRECISION = 1e-6;

class SearchServer {
public:
  SearchServer() = default;

  explicit SearchServer(string_view stopwords);

  explicit SearchServer(const string &stopwords);

  template <typename Iterable> explicit SearchServer(Iterable stopwords);

  bool SetStopWords(string_view text);

  // -------------------------------------------

  template <typename StringAlikeObject>
  void AddDocument(int document_id, StringAlikeObject document,
                   DocumentStatus status, const vector<int> &ratings);

  void AddDocument(int document_id, string_view document, DocumentStatus status,
                   const vector<int> &ratings);

  // -------------------------------------------

  template <typename StringAlikeObject>
  [[nodiscard]] vector<Document>
  FindTopDocuments(StringAlikeObject raw_query) const;

  template <typename StringAlikeObject>
  [[nodiscard]] vector<Document>
  FindTopDocuments(const execution::sequenced_policy &,
                   StringAlikeObject raw_query) const;

  template <typename StringAlikeObject>
  [[nodiscard]] vector<Document>
  FindTopDocuments(const execution::parallel_policy &,
                   StringAlikeObject raw_query) const;

  template <typename StringAlikeObject, typename DocumentFilter>
  [[nodiscard]] vector<Document>
  FindTopDocuments(StringAlikeObject raw_query,
                   DocumentFilter doc_filter) const;

  template <typename ExecPolicy, typename StringAlikeObject,
            typename DocumentFilter>
  [[nodiscard]] vector<Document>
  FindTopDocuments(ExecPolicy &, StringAlikeObject raw_query,
                   DocumentFilter doc_filter) const;

  // ---------------------------------------------

  template <typename StringAlikeObject>
  [[nodiscard]] tuple<vector<string_view>, DocumentStatus>
  MatchDocument(StringAlikeObject raw_query, int document_id) const;

  template <typename StringAlikeObject>
  [[nodiscard]] tuple<vector<string_view>, DocumentStatus>
  MatchDocument(const execution::sequenced_policy &,
                StringAlikeObject raw_query, int document_id) const;

  template <typename StringAlikeObject>
  [[nodiscard]] tuple<vector<string_view>, DocumentStatus>
  MatchDocument(const execution::parallel_policy &, StringAlikeObject raw_query,
                int document_id) const;

  // ---------------------------------------------

  [[nodiscard]] int GetDocumentCount() const { return total_docs_; }

  [[nodiscard]] const map<string_view, double> &
  GetWordFrequencies(int document_id) const;

  void RemoveDocument(int document_id);

  void RemoveDocument(execution::sequenced_policy, int document_id);

  void RemoveDocument(execution::parallel_policy, int document_id);

  [[nodiscard]] auto begin() const { return documents_ids_.begin(); }
  [[nodiscard]] auto end() const { return documents_ids_.end(); }

private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
    string document;
  };

  struct QueryWord {
    string_view data;
    bool is_minus;
    bool is_stop;
  };

  struct Query {
    vector<string_view> plus_words{};
    vector<string_view> minus_words{};
  };

  map<string_view, map<int, double>> word_to_docs_freq_;
  map<int, map<string_view, double>> doc_to_words_freq_;
  map<int, DocumentData> documents_;
  set<string, less<>> stop_words_;
  set<int> documents_ids_;
  int total_docs_ = 0;

  static int ComputeAverageRating(const vector<int> &ratings);

  double ComputeWordInvDocFreq(string_view word) const;

  bool IsStopWord(string_view word) const;

  static bool ContainsSpecialChars(string_view text);

  vector<string_view> SplitIntoWordsNoStop(string_view text) const;

  QueryWord ParseQueryWord(string_view text) const;

  optional<Query> ParseQuery(string_view text) const;

  template <typename DocumentFilter>
  vector<Document> FindAllDocuments(const Query &query,
                                    DocumentFilter doc_filter) const;

  template <typename DocumentFilter>
  vector<Document> FindAllDocuments(const execution::sequenced_policy &,
                                    const Query &query,
                                    DocumentFilter doc_filter) const;

  template <typename DocumentFilter>
  vector<Document> FindAllDocuments(const execution::parallel_policy &,
                                    Query &query,
                                    DocumentFilter doc_filter) const;
};

template <typename Iterable> SearchServer::SearchServer(Iterable stopwords) {
  for (auto &word : stopwords) {
    if (!word.empty()) {
      if (ContainsSpecialChars(word)) {
        throw invalid_argument("Special characters are not allowed");
      } else {
        stop_words_.insert(word);
      }
    }
  };
}

template <typename StringAlikeObject>
void SearchServer::AddDocument(int document_id, StringAlikeObject document,
                               DocumentStatus status,
                               const vector<int> &ratings) {
  AddDocument(document_id, string_view{document}, status, ratings);
}

template <typename StringAlikeObject>
vector<Document>
SearchServer::FindTopDocuments(StringAlikeObject raw_query) const {
  return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

template <typename StringAlikeObject, typename DocumentFilter>
[[nodiscard]] vector<Document>
SearchServer::FindTopDocuments(StringAlikeObject raw_query,
                               DocumentFilter doc_filter) const {
  constexpr bool is_status = is_same_v<decay_t<DocumentFilter>, DocumentStatus>;
  if constexpr (is_status) {
    return FindTopDocuments(
        execution::seq, string_view{raw_query},
        [doc_filter](int document_id, DocumentStatus status, int rating) {
          return status == doc_filter;
        });
  }
  return FindTopDocuments(execution::seq, string_view{raw_query}, doc_filter);
}

template <typename StringAlikeObject>
[[nodiscard]] vector<Document>
SearchServer::FindTopDocuments(const execution::sequenced_policy &,
                               StringAlikeObject raw_query) const {
  return FindTopDocuments(raw_query);
}

template <typename StringAlikeObject>
[[nodiscard]] vector<Document>
SearchServer::FindTopDocuments(const execution::parallel_policy &policy,
                               StringAlikeObject raw_query) const {
  return FindTopDocuments(
      policy, string_view{raw_query},
      [](int document_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::ACTUAL;
      });
}

template <typename ExecPolicy, typename StringAlikeObject,
          typename DocumentFilter>
vector<Document>
SearchServer::FindTopDocuments(ExecPolicy &policy, StringAlikeObject raw_query,
                               DocumentFilter doc_filter) const {
  optional<Query> query = ParseQuery(string_view{raw_query});
  if (ContainsSpecialChars(string_view{raw_query}) || !query) {
    throw invalid_argument("Incorrect search query");
  }

  vector<Document> matched_documents{};
  constexpr bool is_status = is_same_v<decay_t<DocumentFilter>, DocumentStatus>;
  if constexpr (is_status) {
    matched_documents = FindAllDocuments(
        policy, *query,
        [doc_filter](int document_id, DocumentStatus status, int rating) {
          return status == doc_filter;
        });
  } else {
    matched_documents = FindAllDocuments(policy, *query, doc_filter);
  }

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

template <typename DocumentFilter>
vector<Document>
SearchServer::FindAllDocuments(const SearchServer::Query &query,
                               DocumentFilter doc_filter) const {
  return FindAllDocuments(execution::seq, query, doc_filter);
}

template <typename DocumentFilter>
vector<Document>
SearchServer::FindAllDocuments(const execution::sequenced_policy &,
                               const SearchServer::Query &query,
                               DocumentFilter doc_filter) const {
  vector<Document> matched_documents;
  map<int, double> doc_to_relev;
  set<int> bad_docs;
  for (const auto &[word, docs] : word_to_docs_freq_) {
    if (binary_search(query.minus_words.begin(), query.minus_words.end(),
                      word)) { // Minus word, ignore document
      for (auto &[id, _] : docs)
        bad_docs.insert(id);
      continue;
    }
    if (binary_search(query.plus_words.begin(), query.plus_words.end(),
                      word)) { // Good word, compute relevance
      double inv_doc_freq = ComputeWordInvDocFreq(word);
      for (const auto &[id, term_freq] : docs) {
        if (doc_filter(id, documents_.at(id).status,
                       documents_.at(id).rating)) {
          doc_to_relev[id] += inv_doc_freq * term_freq;
        }
      }
    }
  }

  // Remove documents that have minus words from result
  for (const auto id : bad_docs)
    doc_to_relev.erase(id);

  for (const auto &[id, rel] : doc_to_relev) {
    matched_documents.push_back({id, rel, documents_.at(id).rating});
  }

  return matched_documents;
}

template <typename DocumentFilter>
vector<Document>
SearchServer::FindAllDocuments(const execution::parallel_policy &policy,
                               SearchServer::Query &query,
                               DocumentFilter doc_filter) const {

  vector<Document> matched_documents;
  ConcurrentMap<int, double> doc_to_relev_par{100};

  for_each(policy, query.plus_words.begin(), query.plus_words.end(),
           [&](const auto word) {
             const auto it = word_to_docs_freq_.find(word);
             if (it != word_to_docs_freq_.end()) {
               double inv_doc_freq = ComputeWordInvDocFreq(word);
               for (const auto &[id, term_freq] : it->second) {
                 if (doc_filter(id, documents_.at(id).status,
                                documents_.at(id).rating)) {
                   doc_to_relev_par[id].ref_to_value +=
                       inv_doc_freq * term_freq;
                 }
               }
             }
           });

  for_each(policy, query.minus_words.begin(), query.minus_words.end(),
           [&](const auto word) {
             const auto it = word_to_docs_freq_.find(word);
             if (it != word_to_docs_freq_.end()) {
               for (const auto &[id, _] : it->second) {
                 doc_to_relev_par[id].ref_to_value = -10;
               }
             }
           });

  for (const auto &[id, rel] : doc_to_relev_par.BuildOrdinaryMap()) {
    if (rel >= 0) {
      matched_documents.push_back({id, rel, documents_.at(id).rating});
    }
  }

  return matched_documents;
}

template <typename StringAlikeObject>
tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(StringAlikeObject raw_query,
                            int document_id) const {
  auto query = ParseQuery(string_view{raw_query});
  if (ContainsSpecialChars(raw_query) || !query) {
    throw invalid_argument("Incorrect search query");
  }
  set<string_view> words;
  for (const auto &[word, docs] : word_to_docs_freq_) {
    if (!docs.count(document_id)) { // no such word in document
      continue;
    }
    if (binary_search(query->minus_words.begin(), query->minus_words.end(),
                      word)) { // minus word found in query
      return {vector<string_view>(), documents_.at(document_id).status};
    }
    if (binary_search(query->plus_words.begin(), query->plus_words.end(),
                      word)) { // normal word found in query
      words.insert(word);
    }
  }
  vector<string_view> words_vector(words.begin(), words.end());
  sort(words_vector.begin(), words_vector.end());
  return {words_vector, documents_.at(document_id).status};
}

template <typename StringAlikeObject>
tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::sequenced_policy &,
                            StringAlikeObject raw_query,
                            int document_id) const {
  return MatchDocument(string_view{raw_query}, document_id);
}

template <typename StringAlikeObject>
tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::parallel_policy &,
                            StringAlikeObject raw_query,
                            int document_id) const {
  auto query = ParseQuery(execution::par, raw_query);
  if (ContainsSpecialChars(raw_query) || !query) {
    throw invalid_argument("Incorrect search query");
  }
  vector<string_view> &mwords = query->minus_words, &pwords = query->plus_words;

  if (any_of(execution::par, mwords.begin(), mwords.end(),
             [this, document_id](string_view word) {
               auto it = word_to_docs_freq_.find(word);
               if (it != word_to_docs_freq_.end()) {
                 return bool((*it).second.count(document_id));
               }
               return false;
             })) {
    return {vector<string_view>(), documents_.at(document_id).status};
  }

  for_each(execution::par, pwords.begin(), pwords.end(),
           [this, &document_id](string_view &word) {
             auto it = word_to_docs_freq_.find(word);
             if (!(it != word_to_docs_freq_.end() &&
                   (*it).second.count(document_id))) {
               word = "";
             }
           });

  sort(pwords.begin(), pwords.end());
  pwords.erase(unique(pwords.begin(), pwords.end()), pwords.end());
  if (pwords.front().empty()) {
    pwords.erase(query->plus_words.begin());
  }
  return {pwords, documents_.at(document_id).status};
}