#pragma once

#include <iostream>
#include <vector>

using namespace std;

enum class DocumentStatus {
    ACTUAL, IRRELEVANT, BANNED, REMOVED
};

struct Document {
    Document(int a = 0, double b = 0.0, int c = 0)
            : id{a}, relevance{b}, rating{c} {};
    int id;
    double relevance;
    int rating;
};

ostream &operator<<(ostream &os, const Document &document);

void PrintDocument(const Document &document);

void PrintMatchDocumentResult(int document_id, const vector<string> &words,
                              DocumentStatus status);