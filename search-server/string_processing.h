#pragma once

#include <string>
#include <vector>

using namespace std;

vector<string> SplitIntoWords(const string &text);

vector<string_view> SplitIntoWordsView(string_view str);