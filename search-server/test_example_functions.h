#pragma once

#include "search_server.h"
#include <iostream>

using namespace std;

#define ASSERT_EQUAL(a, b)                                                     \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint)                                          \
  AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr)                                                           \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint)                                                \
  AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query);

void MatchDocuments(const SearchServer &search_server, const string &query);

void AddDocument(SearchServer &search_server, int document_id,
                 const string &document, DocumentStatus status,
                 const vector<int> &ratings);

void AssertImpl(bool value, const string &expr_str, const string &file,
                const string &func, unsigned line, const string &hint);

template <typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str,
                     const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint);

SearchServer GenerateTestServer();

const SearchServer TEST_SERVER = GenerateTestServer();

void TestExcludeStopWordsFromAddedDocumentContent();

void TestMinusWordsSupport();

void TestSorting();

void TestRating();

void TestRelevance();

void TestMatching();

void TestStatus();

void TestFilter();

void TestSearchServer();

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