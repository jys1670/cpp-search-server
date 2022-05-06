#pragma once
#include "document.h"
#include "search_server.h"
#include <list>
#include <string>
#include <vector>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer &search_server,
                                        const vector<string> &queries);

list<Document> ProcessQueriesJoined(const SearchServer &search_server,
                                    const std::vector<std::string> &queries);
