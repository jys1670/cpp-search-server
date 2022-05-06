#include "process_queries.h"
#include <execution>
#include <numeric>

vector<vector<Document>> ProcessQueries(const SearchServer &search_server,
                                        const vector<string> &queries) {
  vector<vector<Document>> result(queries.size());
  transform(execution::par, queries.begin(), queries.end(), result.begin(),
            [&search_server](const string &raw_query) {
              return search_server.FindTopDocuments(raw_query);
            });
  return result;
}

list<Document> ProcessQueriesJoined(const SearchServer &search_server,
                                    const std::vector<std::string> &queries) {

  return transform_reduce(
      execution::par, queries.begin(), queries.end(), list<Document>{},
      [](list<Document> list1, list<Document> list2) {
        list1.splice(list1.end(), list2);
        return list1;
      },
      [&search_server](const string &raw_query) {
        auto doc_vec = search_server.FindTopDocuments(raw_query);
        return list<Document>{make_move_iterator(doc_vec.begin()),
                              make_move_iterator(doc_vec.end())};
      });
}