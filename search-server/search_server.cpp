#include "search_server.h"
#include "string_processing.h"
#include <list>
#include <numeric>

SearchServer::SearchServer(const string &stopwords) {
  for (auto word : SplitIntoWordsView(stopwords)) {
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
  for (auto word : SplitIntoWordsView(stopwords)) {
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
  for (auto word : SplitIntoWordsView(text)) {
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
  ++total_docs_;
  vector<string_view> words = SplitIntoWordsNoStop(document);
  const double inv_freq = 1.0 / words.size();
  for (string_view word_view : words) {
    auto it = word_to_docs_freq_.find(word_view);
    if (it == word_to_docs_freq_.end()) {
      const string &word =
          word_to_docs_freq_
              .insert({string{word_view},
                       map<int, double>{{document_id, inv_freq}}})
              .first->first;
      doc_to_words_freq_[document_id][word] += inv_freq;
    } else {
      it->second[document_id] += inv_freq;
      doc_to_words_freq_[document_id][it->first] += inv_freq;
    }
  }
  documents_[document_id] = {ComputeAverageRating(ratings), status};
  documents_ids_.insert(document_id);
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const char *raw_query, int document_id) const {
  tuple<vector<string_view>, DocumentStatus> tmp =
      MatchDocument(string_view{raw_query}, document_id);
  return {{get<0>(tmp).begin(), get<0>(tmp).end()}, get<1>(tmp)};
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(string &&raw_query, int document_id) const {
  tuple<vector<string_view>, DocumentStatus> tmp =
      MatchDocument(string_view{raw_query}, document_id);
  return {{get<0>(tmp).begin(), get<0>(tmp).end()}, get<1>(tmp)};
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const string &raw_query, int document_id) const {
  return MatchDocument(string_view{raw_query}, document_id);
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(string_view raw_query, int document_id) const {
  auto query = ParseQuery(raw_query);
  if (ContainsSpecialChars(raw_query) || !query) {
    throw invalid_argument("Incorrect search query");
  }
  set<string_view> words;
  for (const auto &[word, docs] : word_to_docs_freq_) {
    if (!docs.count(document_id)) { // no such word in document
      continue;
    }
    if (query->minus_words.count(word)) { // minus word found in query
      return {vector<string_view>(), documents_.at(document_id).status};
    }
    if (query->plus_words.count(word)) { // normal word found in query
      words.insert(word);
    }
  }
  vector<string_view> words_vector(words.begin(), words.end());
  sort(words_vector.begin(), words_vector.end());
  return {words_vector, documents_.at(document_id).status};
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const execution::sequenced_policy &,
                            const char *raw_query, int document_id) const {
  return MatchDocument(raw_query, document_id);
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const execution::sequenced_policy &,
                            string &&raw_query, int document_id) const {
  tuple<vector<string_view>, DocumentStatus> tmp =
      MatchDocument(string_view{raw_query}, document_id);
  return {{get<0>(tmp).begin(), get<0>(tmp).end()}, get<1>(tmp)};
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::sequenced_policy &,
                            const string &raw_query, int document_id) const {
  return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::sequenced_policy &,
                            string_view raw_query, int document_id) const {
  return MatchDocument(raw_query, document_id);
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const execution::parallel_policy &pol,
                            const char *raw_query, int document_id) const {
  tuple<vector<string_view>, DocumentStatus> tmp =
      MatchDocument(pol, string_view{raw_query}, document_id);
  return {{get<0>(tmp).begin(), get<0>(tmp).end()}, get<1>(tmp)};
}

tuple<vector<string>, DocumentStatus>
SearchServer::MatchDocument(const execution::parallel_policy &pol,
                            string &&raw_query, int document_id) const {
  tuple<vector<string_view>, DocumentStatus> tmp =
      MatchDocument(pol, string_view{raw_query}, document_id);
  return {{get<0>(tmp).begin(), get<0>(tmp).end()}, get<1>(tmp)};
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::parallel_policy &pol,
                            const string &raw_query, int document_id) const {
  return MatchDocument(pol, string_view{raw_query}, document_id);
}

tuple<vector<string_view>, DocumentStatus>
SearchServer::MatchDocument(const execution::parallel_policy &,
                            string_view raw_query, int document_id) const {
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

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
  if (ratings.empty()) {
    return 0;
  }
  return accumulate(ratings.begin(), ratings.end(), 0) /
         static_cast<int>(ratings.size());
}

double SearchServer::ComputeWordInvDocFreq(string_view word) const {
  double docs_with_word = word_to_docs_freq_.find(word)->second.size();
  // double docs_with_word = word_to_docs_freq_.at(word).size();
  return log(total_docs_ / docs_with_word);
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
  for (string_view word : SplitIntoWordsView(text)) {
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

optional<SearchServer::Query>
SearchServer::ParseQuery(const string &text) const {
  return ParseQuery(string_view{text});
}

optional<SearchServer::Query> SearchServer::ParseQuery(string_view text) const {
  Query query;
  for (string_view word : SplitIntoWordsView(text)) {
    const QueryWord query_word = ParseQueryWord(word);
    if (query_word.data.empty() || query_word.data[0] == '-') {
      return nullopt;
    }
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        query.minus_words.insert(query_word.data);
      } else {
        query.plus_words.insert(query_word.data);
      }
    }
  }
  return optional<SearchServer::Query>{move(query)};
}

optional<SearchServer::PQuery>
SearchServer::ParseQuery(execution::parallel_policy pol,
                         const string &text) const {
  return ParseQuery(pol, string_view{text});
}

optional<SearchServer::PQuery>
SearchServer::ParseQuery(execution::parallel_policy, string_view text) const {
  PQuery query;
  for (string_view word : SplitIntoWordsView(text)) {
    const QueryWord query_word = ParseQueryWord(word);
    if (query_word.data.empty() || query_word.data[0] == '-') {
      return nullopt;
    }
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        query.minus_words.push_back(query_word.data);
      } else {
        query.plus_words.push_back(query_word.data);
      }
    }
  }
  return optional<SearchServer::PQuery>{move(query)};
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
    word_to_docs_freq_.find(word)->second.erase(document_id);
  }
  doc_to_words_freq_.erase(document_id);
  documents_.erase(document_id);
  documents_ids_.erase(document_id);
  --total_docs_;
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
             this->word_to_docs_freq_.find(word)->second.erase(document_id);
           });
  doc_to_words_freq_.erase(document_id);
  documents_.erase(document_id);
  documents_ids_.erase(document_id);
  --total_docs_;
}