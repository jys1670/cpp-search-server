#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view str) {
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