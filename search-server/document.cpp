#include "document.h"
#include <stdexcept>


Document::Document(int a, double b, int c)
        : id{a}, relevance{b}, rating{c} {}

ostream &operator<<(ostream &os, const Document &document) {
    os << "{ "s
       << "document_id = "s << document.id << ", "s
       << "relevance = "s << document.relevance << ", "s
       << "rating = "s << document.rating << " }"s;
    return os;
}

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string> &words,
                              DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string &word: words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}
