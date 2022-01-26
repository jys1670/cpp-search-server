#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus { ACTUAL, IRRELEVANT, BANNED, REMOVED };

/*
string ReadLine() {
  string s;
  getline(cin, s);
  return s;
}

int ReadLineWithNumber() {
  int result = 0;
  cin >> result;
  ReadLine();
  return result;
}
*/

vector<string> SplitIntoWords(const string &text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

class SearchServer {
public:
    int GetDocumentCount() const { return total_docs_; }

    void SetStopWords(const string &text) {
        for (const string &word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document,
                     DocumentStatus status, const vector<int> &ratings) {
        ++total_docs_;
        vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_freq = 1.0 / words.size();
        for (const auto &word : words) {
            word_to_docs_freq_[word][document_id] += inv_freq;
        }
        documents_[document_id] = {ComputeAverageRating(ratings), status};
    }

    template <typename DocumentFilter>
    vector<Document> FindTopDocuments(const string &raw_query,
                                      DocumentFilter doc_filter) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, doc_filter);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs) {
                 if (abs(rhs.relevance - lhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 }
                 return lhs.relevance > rhs.relevance;
             });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string &raw_query,
                                      DocumentStatus allowed_status) const {
        auto matched_documents = FindTopDocuments(
                raw_query,
                [allowed_status](int document_id, DocumentStatus status, int rating) {
                    return status == allowed_status;
                });
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string &raw_query) const {
        auto matched_documents =
                FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
        return matched_documents;
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
                                                        int document_id) const {
        Query query = ParseQuery(raw_query);
        set<string> words;
        for (const auto &[word, docs] : word_to_docs_freq_) {
            if (!docs.count(document_id)) { // no such word in document
                continue;
            }
            if (query.minus_words.count(word)) { // minus word found in query
                return {vector<string>(), documents_.at(document_id).status};
            }
            if (query.plus_words.count(word)) { // normal word found in query
                words.insert(word);
            }
        }
        vector<string> words_vector(words.begin(), words.end());
        sort(words_vector.begin(), words_vector.end());
        return {words_vector, documents_.at(document_id).status};
    };

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
    map<int, DocumentData> documents_;
    set<string> stop_words_;
    int total_docs_ = 0;

    static int ComputeAverageRating(const vector<int> &ratings) {
        if (ratings.empty()) {
            return 0;
        }
        return accumulate(ratings.begin(), ratings.end(), 0) /
               static_cast<int>(ratings.size());
    }

    double ComputeWordInvDocFreq(const string &word) const {
        double docs_with_word = word_to_docs_freq_.at(word).size();
        return log(total_docs_ / docs_with_word);
    }

    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const {
        vector<string> words;
        for (const string &word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    Query ParseQuery(const string &text) const {
        Query query;
        for (const string &word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    template <typename DocumentFilter>
    vector<Document> FindAllDocuments(const Query &query,
                                      DocumentFilter doc_filter) const {
        vector<Document> matched_documents;
        map<int, double> doc_to_relev;
        set<int> bad_docs;
        for (const auto &[word, docs] : word_to_docs_freq_) {
            if (query.minus_words.count(word)) { // Minus word, ignore document
                for (auto &[id, _] : docs)
                    bad_docs.insert(id);
                continue;
            }
            if (query.plus_words.count(word)) { // Good word, compute relevance
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
};

/*
SearchServer CreateSearchServer() {
  SearchServer search_server;
  search_server.SetStopWords(ReadLine());
  const int document_count = ReadLineWithNumber();
  for (int document_id = 0; document_id < document_count; ++document_id) {
    string document = ReadLine();
    // Number of rating entries always presents, thus sizeof(rating_str) > 0
    vector<string> ratings_str = SplitIntoWords(ReadLine());
    vector<int> ratings;
    for (auto it = ratings_str.begin() + 1; it != ratings_str.end(); ++it) {
      ratings.push_back(stoi(*it));
    }

    search_server.AddDocument(document_id, document, ratings);
  }
  return search_server;
}
*/

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,
                              DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                              DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                              DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,
                              DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document &document :
            search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document &document : search_server.FindTopDocuments(
            "пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document &document : search_server.FindTopDocuments(
            "пушистый ухоженный кот"s,
            [](int document_id, DocumentStatus status, int rating) {
                return document_id % 2 == 0;
            })) {
        PrintDocument(document);
    }

    return 0;
}