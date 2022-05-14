#include "search_server.h"
#include "string_processing.h"
#include <list>
#include <numeric>

SearchServer::SearchServer(const string &stopwords) {
  for (auto word : SplitIntoWords(stopwords)) {
    if (!word.empty()) {
      if (ContainsSpecialChars(word)) {
        throw invalid_argument(
            "Special characters are not allowed in stop-words");
      } else {
        stop_words_.insert(string{word});
      }
    }
  };
}

SearchServer::SearchServer(string_view stopwords) {
  for (auto word : SplitIntoWords(stopwords)) {
    if (!word.empty()) {
      if (ContainsSpecialChars(word)) {
        throw invalid_argument(
            "Special characters are not allowed in stop-words");
      } else {
        stop_words_.insert(string{word});
      }
    }
  };
}

bool SearchServer::SetStopWords(string_view text) {
  for (auto word : SplitIntoWords(text)) {
    stop_words_.insert(string{word});
  }
  return true;
}

void SearchServer::AddDocument(int document_id, string_view document,
                               DocumentStatus status,
                               const vector<int> &ratings) {
  if (ContainsSpecialChars(document) || document_id < 0 ||
      documents_.count(document_id) > 0) {
    throw invalid_argument("Either document ID or content is incorrect");
  }
  documents_[document_id] = {ComputeAverageRating(ratings), status,
                             string{document}};
  documents_ids_.insert(document_id);

  vector<string_view> words =
      SplitIntoWordsNoStop(documents_[document_id].document);
  const double inv_freq = 1.0 / words.size();
  for (string_view word_view : words) {
    word_to_docs_freq_[word_view][document_id] += inv_freq;
    doc_to_words_freq_[document_id][word_view] += inv_freq;
  }
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
  if (ratings.empty()) {
    return 0;
  }
  return accumulate(ratings.begin(), ratings.end(), 0) /
         static_cast<int>(ratings.size());
}

double SearchServer::ComputeWordInvDocFreq(string_view word) const {
  double docs_with_word = word_to_docs_freq_.at(word).size();
  return log(documents_.size() / docs_with_word);
}

bool SearchServer::IsStopWord(string_view word) const {
  return stop_words_.count(word) > 0;
}

bool SearchServer::ContainsSpecialChars(string_view text) {
  for (char ch : text) {
    if (int{ch} >= 0 && int{ch} <= 31) {
      return true;
    }
  }
  return false;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
  vector<string_view> words;
  for (string_view word : SplitIntoWords(text)) {
    if (!IsStopWord(word)) {
      words.push_back(word);
    }
  }
  return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
  bool is_minus = false;
  if (text[0] == '-') {
    is_minus = true;
    text = text.substr(1);
  }
  return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
  if (ContainsSpecialChars(text)) {
    throw invalid_argument("Incorrect search query");
  }
  Query query;
  for (string_view word : SplitIntoWords(text)) {
    const QueryWord query_word = ParseQueryWord(word);
    if (query_word.data.empty() || query_word.data[0] == '-') {
      throw invalid_argument("Incorrect search query");
    }
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        query.minus_words.push_back(query_word.data);
      } else {
        query.plus_words.push_back(query_word.data);
      }
    }
  }
  sort(query.minus_words.begin(), query.minus_words.end());
  query.minus_words.erase(
      unique(query.minus_words.begin(), query.minus_words.end()),
      query.minus_words.end());
  sort(query.plus_words.begin(), query.plus_words.end());
  query.plus_words.erase(
      unique(query.plus_words.begin(), query.plus_words.end()),
      query.plus_words.end());
  return query;
}

const map<string_view, double> &
SearchServer::GetWordFrequencies(int document_id) const {
  const auto result = doc_to_words_freq_.find(document_id);
  if (result == doc_to_words_freq_.end()) {
    const static map<string_view, double> tmp;
    return tmp;
  }
  return doc_to_words_freq_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
  for (auto &[word, _] : doc_to_words_freq_[document_id]) {
    word_to_docs_freq_[word].erase(document_id);
  }
  doc_to_words_freq_.erase(document_id);
  documents_.erase(document_id);
  documents_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(execution::sequenced_policy,
                                  int document_id) {
  RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(execution::parallel_policy, int document_id) {
  const map<string_view, double> &words_map =
      doc_to_words_freq_.at(document_id);
  vector<string_view> words;
  for (auto &[word, _] : words_map) {
    words.push_back(word);
  }
  for_each(execution::par, words.begin(), words.end(),
           [this, &document_id](string_view word) {
             this->word_to_docs_freq_[word].erase(document_id);
           });
  doc_to_words_freq_.erase(document_id);
  documents_.erase(document_id);
  documents_ids_.erase(document_id);
}