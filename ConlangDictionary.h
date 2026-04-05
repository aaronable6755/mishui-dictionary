#ifndef CONLANGDICTIONARY_H
#define CONLANGDICTIONARY_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QList>
#include <utility>

// 词条结构体
struct DictionaryEntry {
    QString word;           // 单词（米水语）
    QString word_cn;        // 中文翻译
    QString partOfSpeech;   // 词性
    QVector<QString> phrases;      // 词组列表（米水语）
    QVector<QString> phrases_cn;   // 词组中文翻译列表
    int index;

    DictionaryEntry(const QString& w = QString(),
                    const QString& t = QString(),
                    const QString& p = QString(),
                    const QVector<QString>& ph = QVector<QString>(),
                    const QVector<QString>& pht = QVector<QString>(),
                    const int ind = int())
        : word(w), word_cn(t), partOfSpeech(p),
        phrases(ph), phrases_cn(pht), index(ind) {}

    // 方便调试的方法
    QString toString() const {
        return QString("%1 (%2) -> %3 | 词组数: %4").arg(word, partOfSpeech, word_cn).arg(phrases.size());
    }
};

// 搜索结果结构体
struct SearchResult {
    const DictionaryEntry* entry;   // 指向匹配的词条
    QString matchedby;              // 匹配方式
    double score;                   // 相似度分数
    QString highlightinfo;          // 高亮信息

    SearchResult(const DictionaryEntry* e = nullptr,
                 const QString& m = QString(),
                 double s = 0.0,
                 const QString& h = QString())
        : entry(e), matchedby(m), score(s), highlightinfo(h) {}

    // 判断是否包含词组
    bool hasPhrases() const {
        return entry && !entry->phrases.isEmpty();
    }
};

// 主词典类
// ConlangDictionary.h
class ConlangDictionary {
private:
    // ========== 快速索引（针对单词/翻译/词性）==========
    QHash<QString, DictionaryEntry> wordToEntry;          // 单词 -> 完整词条
    QHash<QString, QVector<QString>> translationToWords;  // 翻译 -> 单词列表
    QHash<QString, QVector<QString>> posToWords;          // 词性 -> 单词列表
    QHash<QChar, QVector<QString>> firstLetterIndex;      // 首字母 -> 单词列表
    QHash<int, DictionaryEntry> indexToEntry;
    // ========== 动态搜索（针对词组）==========
    // 不需要词组索引！词组存储在 DictionaryEntry 中

    // ========== 辅助结构 ==========
    QSet<QString> allWords;                                // 所有单词集合

    // 停用词（如果需要）
    static const QSet<QString> chineseStopWords;

public:
    // 核心接口
    static bool isInteger(const QString& s);
    const DictionaryEntry* getEntryByIndex(int index) const;
    QList<int> getAllIndices() const;
    int getEntryCount() const { return indexToEntry.size(); }
    void testSearchFunctions();
    bool loadDictionaryFiles(const QString& wordsFile,
                             const QString& transFile,
                             const QString& posFile,
                             const QString& phrasesFile,
                             const QString& phraseTransFile);
    QVector<SearchResult> smartSearch(const QString& query, int maxResults) const;
    void extracted(QStringList &splitedQuery) const;
    QString translateSentence(const QString &query) const;
    bool isSentence(QString& query);
    QVector<QString> createExSentence(const QString& word, const QString& wordTrans, const QString& pos);
    void printStatistics() const;
    void runTests() const;
    bool isMainlyChinese(const QString& str) const;

private:
    const DictionaryEntry* lookupByNumIndex(int index) const;
    // 单词/翻译快速搜索（使用索引）
    const DictionaryEntry* lookupByWord(const QString& word) const;
    QVector<const DictionaryEntry*> lookupByExactChinese(const QString& chinese) const;
    QVector<std::pair<QString, double>> fuzzyWordSearch(const QString& userInput, int maxSuggestions, double threshold) const;

    // 词组动态搜索（遍历）
    QVector<SearchResult> findPhrasesContainingWord(const QString& word) const;
    QVector<SearchResult> findExactPhrase(const QString& phrase) const;
    QVector<SearchResult> findPhraseTranslation(const QString& translation) const;
    // 已弃用
    QVector<const DictionaryEntry*> lookupByPartOfSpeech(const QString& pos) const;
    void handleMixedQuery(const QString& query,
                                             QVector<SearchResult>& results,
                                             QSet<const DictionaryEntry*>& addedEntries) const;
    // 中文翻译搜索（混合：使用索引+部分遍历）
    QVector<SearchResult> searchChineseDynamic(const QString& query) const;
    QVector<SearchResult> searchChineseTranslation(const QString& query) const;
    // 辅助函数
    void clearAllData();
    QStringList splitString(const QString& str, const QChar delimiter) const ;
};

#endif // CONLANGDICTIONARY_H
