#include "string_processing.h"

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

vector<string_view> SplitIntoWordsView(string_view str) {
  vector<string_view> result{};
  auto pos = str.find_first_not_of(' ');
  str.remove_prefix(pos == string_view::npos ? str.size() : pos);
  while (!str.empty()) {
    auto space = str.find(' ');
    result.push_back(space == string_view::npos ? str.substr(0, str.size())
                                                : str.substr(0, space));
    pos = str.find_first_not_of(' ', space);
    str.remove_prefix(pos == string_view::npos ? str.size() : pos);
  }

  return result;
}