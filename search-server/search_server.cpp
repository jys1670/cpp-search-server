#include <numeric>
#include "search_server.h"
#include "string_processing.h"

SearchServer::SearchServer(string stopwords) {
    for (string &word: SplitIntoWords(stopwords)) {
        if (!word.empty()) {
            if (ContainsSpecialChars(word)) {
                throw invalid_argument(
                        "Special characters are not allowed in stop-words");
            } else {
                stop_words_.insert(word);
            }
        }
    };
}

bool SearchServer::SetStopWords(const string &text) {
    for (const string &word: SplitIntoWords(text)) {
        stop_words_.insert(word);
    }
    return true;
}

void SearchServer::AddDocument(int document_id, const string &document,
                               DocumentStatus status,
                               const vector<int> &ratings) {
    if (ContainsSpecialChars(document) || document_id < 0 ||
        documents_.count(document_id) > 0) {
        throw invalid_argument("Either document ID or content is incorrect");
    }
    ++total_docs_;
    vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_freq = 1.0 / words.size();
    for (const auto &word: words) {
        word_to_docs_freq_[word][document_id] += inv_freq;
        doc_to_words_freq_[document_id][word] += inv_freq;
    }
    documents_[document_id] = {ComputeAverageRating(ratings), status};
    documents_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(
        const string &raw_query, DocumentStatus allowed_status) const {
    try {
        return FindTopDocuments(
                raw_query,
                [allowed_status](int document_id, DocumentStatus status, int rating) {
                    return status == allowed_status;
                });
    } catch (invalid_argument &err) {
        throw err;
    }
}

vector<Document> SearchServer::FindTopDocuments(
        const string &raw_query) const {
    try {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    } catch (invalid_argument &err) {
        throw err;
    }
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(
        const string &raw_query, int document_id) const {
    auto query = ParseQuery(raw_query);
    if (ContainsSpecialChars(raw_query) || !query) {
        throw invalid_argument("Incorrect search query");
    }
    set<string> words;
    for (const auto &[word, docs]: word_to_docs_freq_) {
        if (!docs.count(document_id)) {  // no such word in document
            continue;
        }
        if (query->minus_words.count(word)) {  // minus word found in query
            return {vector<string>(), documents_.at(document_id).status};
        }
        if (query->plus_words.count(word)) {  // normal word found in query
            words.insert(word);
        }
    }
    vector<string> words_vector(words.begin(), words.end());
    sort(words_vector.begin(), words_vector.end());
    return {words_vector, documents_.at(document_id).status};
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return accumulate(ratings.begin(), ratings.end(), 0) /
           static_cast<int>(ratings.size());
}

double SearchServer::ComputeWordInvDocFreq(const string &word) const {
    double docs_with_word = word_to_docs_freq_.at(word).size();
    return log(total_docs_ / docs_with_word);
}

bool SearchServer::IsStopWord(const string &word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::ContainsSpecialChars(const string &text) {
    for (char ch: text) {
        if (int{ch} >= 0 && int{ch} <= 31) {
            return true;
        }
    }
    return false;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string &text) const {
    vector<string> words;
    for (const string &word: SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return {text, is_minus, IsStopWord(text)};
}

optional<SearchServer::Query> SearchServer::ParseQuery(
        const string &text) const {
    Query query;
    for (const string &word: SplitIntoWords(text)) {
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
    return optional<SearchServer::Query>{query};
}

const map<string, double> &SearchServer::GetWordFrequencies(int document_id) const {
    const auto result = doc_to_words_freq_.find(document_id);
    if (result == doc_to_words_freq_.end()) {
        const static map<string, double> tmp;
        return tmp;
    }
    return doc_to_words_freq_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    for (auto&[word, _]: doc_to_words_freq_[document_id]) {
        word_to_docs_freq_[word].erase(document_id);
    }
    doc_to_words_freq_.erase(document_id);
    documents_.erase(document_id);
    documents_ids_.erase(document_id);
    --total_docs_;
}