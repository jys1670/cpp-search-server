#include "remove_duplicates.h"
#include <set>
#include <map>
#include <iostream>

void RemoveDuplicates(SearchServer &search_server) {
    map<set<string>, set<int>> duplicates;
    for (auto &doc_id: search_server) {
        set<string> words;
        for (auto&[word, _]: search_server.GetWordFrequencies(doc_id)) {
            words.insert(word);
        }
        duplicates[words].insert(doc_id);
    }
    for (auto&[_, ids]: duplicates) {
        for (auto it = next(ids.begin()); it != ids.end(); ++it) {
            cout << "Found duplicate document id "s << *it << endl;
            search_server.RemoveDocument(*it);
        }
    }
}