#include "remove_duplicates.h"
#include <iostream>
#include <map>
#include <set>

void RemoveDuplicates(SearchServer &search_server) {
  map<set<string_view>, set<int>> duplicates;
  for (auto &doc_id : search_server) {
    set<string_view> words;
    for (auto &[word, _] : search_server.GetWordFrequencies(doc_id)) {
      words.insert(word);
    }
    duplicates[words].insert(doc_id);
  }
  for (auto &[_, ids] : duplicates) {
    for (auto it = next(ids.begin()); it != ids.end(); ++it) {
      cout << "Found duplicate document id " << *it << endl;
      search_server.RemoveDocument(*it);
    }
  }
}