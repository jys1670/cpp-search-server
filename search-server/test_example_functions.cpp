#include "test_example_functions.h"

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query) {
  cout << "Request search results: "s << raw_query << endl;
  try {
    for (const Document &document : search_server.FindTopDocuments(raw_query)) {
      PrintDocument(document);
    }
  } catch (const exception &e) {
    cout << "Error occurred: "s << e.what() << endl;
  }
}

void MatchDocuments(const SearchServer &search_server, const string &query) {
  try {
    cout << "Documents which match request: "s << query << endl;
    for (const int document_id : search_server) {
      const auto [words, status] =
          search_server.MatchDocument(query, document_id);
      PrintMatchDocumentResult(document_id, words, status);
    }
  } catch (const exception &e) {
    cout << "Error matching documents upon request "s << query << ": "s
         << e.what() << endl;
  }
}

void AddDocument(SearchServer &search_server, int document_id,
                 const string &document, DocumentStatus status,
                 const vector<int> &ratings) {
  try {
    search_server.AddDocument(document_id, document, status, ratings);
  } catch (const exception &e) {
    cout << "Error occurred while adding document "s << document_id << ": "s
         << e.what() << endl;
  }
}

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

SearchServer GenerateTestServer() {
  SearchServer server{"and in on with"s};
  server.AddDocument(0, "white cat and fancy collar"s, DocumentStatus::ACTUAL,
                     {8, -3});
  server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL,
                     {7, 2, 7});
  server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::ACTUAL,
                     {5, -12, 2, 1});
  server.AddDocument(3, "funny hippo on deck"s, DocumentStatus::BANNED,
                     {6, -2, 6, 1});
  server.AddDocument(4, "big whale in house"s, DocumentStatus::IRRELEVANT,
                     {1, 5, -5, 1});
  server.AddDocument(5, "dog fluffy and fancy"s, DocumentStatus::IRRELEVANT,
                     {0, 0, 2, -1});
  server.AddDocument(6, "dinner tasty and fluffy"s, DocumentStatus::ACTUAL,
                     {0, 0, 2, -1});
  server.AddDocument(7, "hippo expressive eyes"s, DocumentStatus::BANNED,
                     {4, 3, 2, -1});
  return server;
}

void TestExcludeStopWordsFromAddedDocumentContent() {
  ASSERT_HINT(
      TEST_SERVER.FindTopDocuments("in on and"s).empty(),
      "Search query is made from stop-words only, result must be empty");
  ASSERT(!TEST_SERVER.FindTopDocuments("and cat on dog"s).empty());
}

void TestMinusWordsSupport() {
  vector<Document> result =
      TEST_SERVER.FindTopDocuments("fluffy fancy dog -cat"s);
  ASSERT(!result.empty());
  ASSERT_EQUAL(result[0].id, 2);
  vector<Document> no_result =
      TEST_SERVER.FindTopDocuments("-fluffy tasty fancy tail -dog -cat"s);
  ASSERT(no_result.empty());
}

void TestSorting() {
  vector<Document> result =
      TEST_SERVER.FindTopDocuments("fluffy groomed cat dog -dinner"s);
  vector<int> expected_id = {2, 1, 0};
  ASSERT_EQUAL(result.size(), size_t{3});
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_EQUAL(result[i].id, expected_id[i]);
  }
}

void TestRating() {
  vector<Document> result =
      TEST_SERVER.FindTopDocuments("fluffy groomed cat dog -dinner"s);
  vector<int> expected_rating = {-1, 5, 2};
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_EQUAL(result[i].rating, expected_rating[i]);
  }
}

void TestRelevance() {
  vector<Document> result =
      TEST_SERVER.FindTopDocuments("fluffy groomed cat dog -dinner"s);
  vector<double> expected_relev = {0.8664339756999, 0.8369882167858,
                                   0.3465735902799};
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_HINT(abs(result[i].relevance - expected_relev[i]) <
                    RELEVANCE_PRECISION,
                "Relevance values do not match within given precision");
  }
}

void TestMatching() {
  auto result = TEST_SERVER.MatchDocument("fluffy groomed cat dog -dinner"s, 1);
  auto &received_words = get<0>(result);
  vector<string> expected_words = {"cat"s, "fluffy"s};
  ASSERT_EQUAL(received_words.size(), size_t{2});
  for (int i = 0; i < 2; ++i) {
    ASSERT_EQUAL(expected_words[i], received_words[i]);
  }
}

void TestStatus() {
  size_t banned =
             TEST_SERVER
                 .FindTopDocuments("fluffy hippo cat"s, DocumentStatus::BANNED)
                 .size(),
         irrelevant = TEST_SERVER
                          .FindTopDocuments("fluffy groomed cat"s,
                                            DocumentStatus::IRRELEVANT)
                          .size(),
         actual = TEST_SERVER
                      .FindTopDocuments("fluffy groomed cat"s,
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
      "fluffy groomed cat"s, [](int document_id, DocumentStatus status,
                                int rating) { return document_id % 2 == 0; });
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
