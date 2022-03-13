#include "read_input_functions.h"

void FindTopDocuments(const SearchServer &search_server,
                      const string &raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document &document: search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception &e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer &search_server, const string &query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto[words, status] =
            search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception &e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s
             << e.what() << endl;
    }
}