#pragma once

#include <string>
#include <iostream>
#include "document.h"
#include "search_server.h"

using namespace std;

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query);

void MatchDocuments(const SearchServer &search_server, const string &query);