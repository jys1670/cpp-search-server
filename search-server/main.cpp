#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_PRECISION = 1e-6;

struct Document {
    Document(int a = 0, double b = 0.0, int c = 0)
            : id{a}, relevance{b}, rating{c} {};
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus { ACTUAL, IRRELEVANT, BANNED, REMOVED };

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
    SearchServer() = default;
    explicit SearchServer(string stopwords);
    template <typename Iterable>
    explicit SearchServer(Iterable stopwords);

    bool SetStopWords(const string &text);
    void AddDocument(int document_id, const string &document,
                     DocumentStatus status, const vector<int> &ratings);

    template <typename DocumentFilter>
    [[nodiscard]] vector<Document> FindTopDocuments(
            const string &raw_query, DocumentFilter doc_filter) const;
    [[nodiscard]] vector<Document> FindTopDocuments(
            const string &raw_query, DocumentStatus allowed_status) const;
    [[nodiscard]] vector<Document> FindTopDocuments(
            const string &raw_query) const;
    [[nodiscard]] tuple<vector<string>, DocumentStatus> MatchDocument(
            const string &raw_query, int document_id) const;
    int GetDocumentCount() const { return total_docs_; }
    int GetDocumentId(int num) const {
        if (num < 0 || num >= total_docs_) {
            throw out_of_range("Incorrect document number");
        }
        return documents_ids_.at(num);
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
    map<int, DocumentData> documents_;
    vector<int> documents_ids_;
    set<string> stop_words_;
    int total_docs_ = 0;

    static int ComputeAverageRating(const vector<int> &ratings);
    double ComputeWordInvDocFreq(const string &word) const {
        double docs_with_word = word_to_docs_freq_.at(word).size();
        return log(total_docs_ / docs_with_word);
    }
    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }
    static bool ContainsSpecialChars(const string &text) {
        for (char ch : text) {
            if (int{ch} >= 0 && int{ch} <= 31) {
                return true;
            }
        }
        return false;
    }
    vector<string> SplitIntoWordsNoStop(const string &text) const;
    QueryWord ParseQueryWord(string text) const;
    optional<Query> ParseQuery(const string &text) const;

    template <typename DocumentFilter>
    vector<Document> FindAllDocuments(const Query &query,
                                      DocumentFilter doc_filter) const;
};

SearchServer::SearchServer(string stopwords) {
    for (string &word : SplitIntoWords(stopwords)) {
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

template <typename Iterable>
SearchServer::SearchServer(Iterable stopwords) {
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

bool SearchServer::SetStopWords(const string &text) {
    for (const string &word : SplitIntoWords(text)) {
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
    for (const auto &word : words) {
        word_to_docs_freq_[word][document_id] += inv_freq;
    }
    documents_[document_id] = {ComputeAverageRating(ratings), status};
    documents_ids_.push_back(document_id);
}

template <typename DocumentFilter>
[[nodiscard]] vector<Document> SearchServer::FindTopDocuments(
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

[[nodiscard]] vector<Document> SearchServer::FindTopDocuments(
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

[[nodiscard]] vector<Document> SearchServer::FindTopDocuments(
        const string &raw_query) const {
    try {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    } catch (invalid_argument &err) {
        throw err;
    }
}

[[nodiscard]] tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(
        const string &raw_query, int document_id) const {
    auto query = ParseQuery(raw_query);
    if (ContainsSpecialChars(raw_query) || !query) {
        throw invalid_argument("Incorrect search query");
    }
    set<string> words;
    for (const auto &[word, docs] : word_to_docs_freq_) {
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

vector<string> SearchServer::SplitIntoWordsNoStop(const string &text) const {
    vector<string> words;
    for (const string &word : SplitIntoWords(text)) {
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
    for (const string &word : SplitIntoWords(text)) {
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

template <typename DocumentFilter>
vector<Document> SearchServer::FindAllDocuments(
        const SearchServer::Query &query, DocumentFilter doc_filter) const {
    vector<Document> matched_documents;
    map<int, double> doc_to_relev;
    set<int> bad_docs;
    for (const auto &[word, docs] : word_to_docs_freq_) {
        if (query.minus_words.count(word)) {  // Minus word, ignore document
            for (auto &[id, _] : docs) bad_docs.insert(id);
            continue;
        }
        if (query.plus_words.count(word)) {  // Good word, compute relevance
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
    for (const auto id : bad_docs) doc_to_relev.erase(id);

    for (const auto &[id, rel] : doc_to_relev) {
        matched_documents.push_back({id, rel, documents_.at(id).rating});
    }
    return matched_documents;
}

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string> &words,
                              DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string &word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer &search_server, int document_id,
                 const string &document, DocumentStatus status,
                 const vector<int> &ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception &e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what()
             << endl;
    }
}

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document &document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception &e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer &search_server, const string &query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] =
            search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception &e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s
             << e.what() << endl;
    }
}

// -------- Начало модульных тестов поисковой системы ----------

template <typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str,
                     const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string &expr_str, const string &file,
                const string &func, unsigned line, const string &hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

SearchServer GenerateTestServer() {
    SearchServer server{"и в на"s};
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL,
                       {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL,
                       {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                       DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "веселый бегемот на палубе"s, DocumentStatus::BANNED,
                       {6, -2, 6, 1});
    server.AddDocument(4, "большой кит в доме"s, DocumentStatus::IRRELEVANT,
                       {1, 5, -5, 1});
    server.AddDocument(5, "пёс пушистый и модный"s, DocumentStatus::IRRELEVANT,
                       {0, 0, 2, -1});
    server.AddDocument(6, "обед вкусный и пушистый"s, DocumentStatus::ACTUAL,
                       {0, 0, 2, -1});
    server.AddDocument(7, "бегемот выразительные глаза"s, DocumentStatus::BANNED,
                       {4, 3, 2, -1});
    return server;
}

const SearchServer TEST_SERVER = GenerateTestServer();

void TestExcludeStopWordsFromAddedDocumentContent() {
    ASSERT_HINT(
            TEST_SERVER.FindTopDocuments("в на и"s).empty(),
            "Search query is made from stop-words only, result must be empty");
    ASSERT(!TEST_SERVER.FindTopDocuments("и кот на пёс"s).empty());
}

void TestMinusWordsSupport() {
    vector<Document> result =
            TEST_SERVER.FindTopDocuments("пушистый модный пёс -кот"s);
    ASSERT(!result.empty());
    ASSERT_EQUAL(result[0].id, 2);
    vector<Document> no_result =
            TEST_SERVER.FindTopDocuments("-пушистый вкусный модный хвост -пёс -кот"s);
    ASSERT(no_result.empty());
}

void TestSorting() {
    vector<Document> result =
            TEST_SERVER.FindTopDocuments("пушистый ухоженный кот пёс -обед"s);
    vector<int> expected_id = {2, 1, 0};
    ASSERT_EQUAL(result.size(), size_t{3});
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_EQUAL(result[i].id, expected_id[i]);
    }
}

void TestRating() {
    vector<Document> result =
            TEST_SERVER.FindTopDocuments("пушистый ухоженный кот пёс -обед"s);
    vector<int> expected_rating = {-1, 5, 2};
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_EQUAL(result[i].rating, expected_rating[i]);
    }
}

void TestRelevance() {
    vector<Document> result =
            TEST_SERVER.FindTopDocuments("пушистый ухоженный кот пёс -обед"s);
    vector<double> expected_relev = {0.8664339756999, 0.8369882167858,
                                     0.3465735902799};
    for (size_t i = 0; i < 3; ++i) {
        ASSERT_HINT(
                abs(result[i].relevance - expected_relev[i]) < RELEVANCE_PRECISION,
                "Relevance values do not match within given precision");
    }
}

void TestMatching() {
    auto result =
            TEST_SERVER.MatchDocument("пушистый ухоженный кот пёс -обед"s, 1);
    vector<string> &received_words = get<0>(result);
    vector<string> expected_words = {"кот"s, "пушистый"s};
    ASSERT_EQUAL(received_words.size(), size_t{2});
    for (int i = 0; i < 2; ++i) {
        ASSERT_EQUAL(expected_words[i], received_words[i]);
    }
}

void TestStatus() {
    size_t banned = TEST_SERVER
            .FindTopDocuments("пушистый бегемот кот"s,
                              DocumentStatus::BANNED)
            .size(),
            irrelevant = TEST_SERVER
            .FindTopDocuments("пушистый ухоженный кот"s,
                              DocumentStatus::IRRELEVANT)
            .size(),
            actual = TEST_SERVER
            .FindTopDocuments("пушистый ухоженный кот"s,
                              DocumentStatus::ACTUAL)
            .size();
    ASSERT_EQUAL_HINT(banned, size_t{2},
                      "Amount of entries with BANNED status is wrong");
    ASSERT_EQUAL_HINT(irrelevant, size_t{1},
                      "Amount of entries with IRRELEVANT status is wrong");
    ASSERT_EQUAL_HINT(actual, size_t{4},
                      "Amount of entries with ACTUAL status is wrong");
}

void TestFilter() {
    vector<Document> docs_even_ids = TEST_SERVER.FindTopDocuments(
            "пушистый ухоженный кот"s,
            [](int document_id, DocumentStatus status, int rating) {
                return document_id % 2 == 0;
            });
    for (auto doc : docs_even_ids) {
        ASSERT_HINT(doc.id % 2 == 0,
                    "Document with odd ID was found, filter seems to be broken");
    }
}

void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestMinusWordsSupport();
    TestMatching();
    TestSorting();
    TestRating();
    TestRelevance();
    TestStatus();
    TestFilter();
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;

    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s,
                DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s,
                DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s,
                DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s,
                DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s,
                DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}