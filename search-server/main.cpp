#include "document.h"
#include "search_server.h"
#include "request_queue.h"

using namespace std;


// -------- Начало модульных тестов поисковой системы ----------

template<typename T, typename U>
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
    for (auto doc: docs_even_ids) {
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
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});

    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
}