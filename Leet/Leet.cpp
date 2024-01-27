#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <stack>
#include <deque>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

// Функция для чтения строки с консоли
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

// Функция для чтения числа из строки
int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

// Функция для разделения текста на слова
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

// Структура для представления документа
struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

// Шаблонная функция для создания уникального множества непустых строк
template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

// Перечисление статусов документа
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

// Класс для поиска документов
class SearchServer {
public:
    // Конструктор, принимающий контейнер стоп-слов
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Извлекаем непустые стоп-слова
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw invalid_argument("Some of stop words are invalid"s);
        }
    }

    // Конструктор, принимающий текст стоп-слов
    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text))  // Вызываем делегирующий конструктор с контейнером строк
    {
    }

    // Метод для добавления документа
    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        if ((document_id < 0) || (documents_.count(document_id) > 0)) {
            throw invalid_argument("Invalid document_id"s);
        }
        const auto words = SplitIntoWordsNoStop(document);

        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_ids_.push_back(document_id);
    }

    // Шаблонный метод для поиска документов по запросу
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
        DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        // Сортируем документы по релевантности и рейтингу
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    // Метод для поиска документов по запросу и статусу
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    // Метод для поиска документов по запросу с учетом статуса ACTUAL
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    // Метод для получения общего количества документов
    size_t GetDocumentCount() const {

        size_t docsize = documents_.size();
        return docsize;
    }

    // Метод для получения идентификатора документа по индексу
    int GetDocumentId(int index) const {
        return document_ids_.at(index);
    }

    // Метод для поиска совпадений слов в запросе для указанного документа
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        const auto query = ParseQuery(raw_query);

        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    // Структура данных для хранения информации о документе
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

    // Метод для проверки, является ли слово стоп-словом
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    // Проверка, является ли слово допустимым (не содержит спецсимволов)
    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    // Разделение текста на слова, исключая стоп-слова и проверяя их допустимость
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Word "s + word + " is invalid"s);
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    // Вычисление среднего рейтинга по вектору рейтингов
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    // Структура для представления слова из запроса
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    // Анализ слова из запроса
    QueryWord ParseQueryWord(const string& text) const {
        if (text.empty()) {
            throw invalid_argument("Query word is empty"s);
        }
        string word = text;
        bool is_minus = false;
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
            throw invalid_argument("Query word "s + text + " is invalid");
        }

        return { word, is_minus, IsStopWord(word) };
    }

    // Структура для представления запроса
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    // Анализ запроса
    Query ParseQuery(const string& text) const {
        Query result;
        for (const string& word : SplitIntoWords(text)) {
            const auto query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    result.minus_words.insert(query_word.data);
                }
                else {
                    result.plus_words.insert(query_word.data);
                }
            }
        }
        return result;
    }

    // Вычисление обратной частоты документов для слова
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    // Поиск всех документов, соответствующих запросу с учетом предиката
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) : begin_(begin), end_(end), page_size_(page_size) {


        size_p = distance(begin, end);
        if (size_p % page_size != 0) {
            size_p = size_p / page_size;
            size_p += 1;
        }
        else {
            size_p = size_p / page_size;
        }

        docsort_.resize(size_p);
        while (begin != end) {

            alldoc_.push_back(*begin);
            ++begin;

        }


    }

    auto Sort() {
        int x = 0;
        int lists = 0;
        int convert_pageS = static_cast<int>(page_size_);
        for (int i = 0; i < alldoc_.size(); i++) {
            if (x < convert_pageS) {
                docsort_[lists].insert(docsort_[lists].begin() + x, alldoc_[i]);
                ++x;
            }
            if (x == convert_pageS) {
                x = 0;
                lists += 1;
            }
        }
        return docsort_;
    }

    auto GetAllDoc() {
        return alldoc_;
    }



private:
    int size_p = 0;
    Document last;
    vector<Document> alldoc_;
    vector<vector<Document>> docsort_;
    Iterator begin_, end_;
    size_t page_size_ = 0, page_count_ = 0;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    Paginator ret = Paginator(c.begin(), c.end(), page_size);
    
    return ret.Sort();
}

ostream& operator << (ostream& os, const vector<Document> &obj) {

        os << "{ document_id = " << obj[0].id << ", relevance = " << obj[0].relevance << ", rating = " << obj[0].rating << " }";
        if (obj.size() > 1) {
            os << "{ document_id = " << obj[1].id << ", relevance = " << obj[1].relevance << ", rating = " << obj[1].rating << " }";
        }
    
    return os;
}


class RequestQueue {
public:

    explicit RequestQueue(const SearchServer& search_server) : search_request_(search_server) {
    }

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    void AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {    //void должно быть vector<Document>
        // напишите реализацию
    }
    void AddFindRequest(const string& raw_query, DocumentStatus status) {               //void должно быть vector<Document>
        // напишите реализацию
    }
    void AddFindRequest(const string& raw_query) {                                                  //void должно быть vector<Document>
        // напишите реализацию
        if (requests_.empty() || requests_.size() < min_in_day_) {
            auto a = search_request_.FindTopDocuments(raw_query);
            if (a.empty() || a.size() == 0) {
                empty_req_ += 1;
                requests_.push_back(false);

            }
            else {
                requests_.push_back(true);
            }
        }
        else {
            if (requests_.front() == false) {
                empty_req_ -= 1;
            }
            requests_.pop_front();

            auto a = search_request_.FindTopDocuments(raw_query);
            if (a.empty() || a.size() == 0) {
                requests_.push_back(false);
                empty_req_ += 1;
            }
            else {
                requests_.push_back(true);
            }
        }
    }
    int GetNoResultRequests() const {

        return empty_req_;

    }
private:

    struct QueryResult {
        bool actual;
    };
    deque<bool> requests_;
    int empty_req_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_request_;
    
};

int main() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, { 1, 1, 1 });
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
}