#include "ConlangDictionary.h"
#include <QFile>
#include <QTextStream>
#include <cctype>
#include <QRegularExpression>
#include <algorithm>
#include <rapidfuzz/fuzz.hpp>
// QDebug 发布时删除
#include <QDebug>

// 中文停用词
const QSet<QString> ConlangDictionary::chineseStopWords = {
    "的", "之", "与", "和", "或", "在", "于", "对", "从", "而", "但"
};

QVector<QString> ConlangDictionary::createExSentence(const QString& word, const QString& wordTrans, const QString& pos){
    QVector<QString> sentenceAndTrans = {"\n","  例句中文意义："};
    if (pos.contains("ko")){
        sentenceAndTrans = {QString("Ber ini na %1?\n").arg(word), QString("  例句中文意义：你有%1吗？").arg(wordTrans)};
    } else if (pos.contains("kiu")) {
        sentenceAndTrans = {QString("Ber ellin ber %1?\n").arg(word), QString("  例句中文意义：埃林是不是%1的？").arg(wordTrans)};
    } else if (pos.contains("ka")) {
        sentenceAndTrans = {QString("Ber %1 rado?\n").arg(word), QString("  例句中文意义：%1是不是安全的？").arg(wordTrans)};
    }
    return sentenceAndTrans;
}

bool ConlangDictionary::loadDictionaryFiles(
    const QString& valuesFile,       // 米水语单词文件（values.txt）
    const QString& keysFile,         // 中文翻译文件（keys.txt）
    const QString& posFile,          // 词性文件（wordtype.txt）
    const QString& phrasesFile,      // 词组文件（phrases.txt）
    const QString& phraseTransFile   // 词组翻译文件（cn_phrases.txt）
    ) {
    clearAllData();

    QFile fWords(valuesFile), fTrans(keysFile), fPos(posFile),
        fPhrases(phrasesFile), fPhraseTrans(phraseTransFile);

    QList<QFile*> files = {&fWords, &fTrans, &fPos, &fPhrases, &fPhraseTrans};
    QStringList filenames = {valuesFile, keysFile, posFile, phrasesFile, phraseTransFile};

    for (int i = 0; i < files.size(); ++i) {
        if (!files[i]->open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }
    }

    QTextStream inWords(&fWords), inTrans(&fTrans), inPos(&fPos),
        inPhrases(&fPhrases), inPhraseTrans(&fPhraseTrans);

    QString word, trans, pos, phraseLine, phraseTransLine;
    int lineNum = 0;
    int loadedCount = 0;

    while (!inWords.atEnd()) {
        word = inWords.readLine().trimmed();
        lineNum++;

        trans = inTrans.readLine().trimmed();
        pos = inPos.readLine().trimmed();
        phraseLine = inPhrases.readLine().trimmed();
        phraseTransLine = inPhraseTrans.readLine().trimmed();

        if (trans.isNull() || pos.isNull()) {
            break;
        }

        if (word.isEmpty() && trans.isEmpty()) {
            continue;
        }

        QStringList phrasesVec = splitString(phraseLine, ',');
        QStringList phraseTransVec = splitString(phraseTransLine, ',');

        if (phrasesVec.size() != phraseTransVec.size()) {
            int minSize = qMin(phrasesVec.size(), phraseTransVec.size());
            if (phrasesVec.size() > minSize) {
                phrasesVec = phrasesVec.mid(0, minSize);
            }
            if (phraseTransVec.size() > minSize) {
                phraseTransVec = phraseTransVec.mid(0, minSize);
            }
        }

        DictionaryEntry entry;
        entry.word = word;                            // 原始单词
        entry.word_cn = trans;                        // 中文翻译
        entry.partOfSpeech = pos;                     // 词性
        entry.phrases = phrasesVec.toVector();        // 词组数组
        entry.phrases_cn = phraseTransVec.toVector(); // 词组翻译数组
        entry.index = lineNum;


        QString wordLower = word.toLower();
        wordToEntry[wordLower] = entry;

        indexToEntry[lineNum] = entry;

        wordToEntry[wordLower] = entry;

        allWords.insert(wordLower);

        if (!wordLower.isEmpty()) {
            firstLetterIndex[wordLower[0]].append(wordLower);
        }

        if (!trans.isEmpty()) {
            translationToWords[trans].append(wordLower);
        }

        if (!pos.isEmpty()) {
            QStringList posList = splitString(pos, ';');
            for (const QString& singlePos : std::as_const(posList)) {
                QString cleanPos = singlePos.trimmed().toLower();
                if (!cleanPos.isEmpty()) {
                    posToWords[cleanPos].append(wordLower);
                }
            }
        }
        loadedCount++;
    }

    // 关闭
    for (auto f : std::as_const(files)) {
        f->close();
    }

    if (wordToEntry.size() != loadedCount) {
        //qDebug() << "=======存在大小写不敏感的重复单词";
    }

    // 验证数据完整性
    if (loadedCount > 0) {
        int count = 0;
        for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd() && count < 3; ++it) {
            const DictionaryEntry& entry = it.value();
            if (!entry.phrases.isEmpty()) {
                // qDebug() << "词组:";
                for (int i = 0; i < entry.phrases.size(); ++i) {
                    //qDebug() << "  " << entry.phrases[i] << " -> " << entry.phrases_cn[i];
                }
            }
            count++;
        }

        int totalPhrases = 0;
        for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd(); ++it) {
            totalPhrases += it.value().phrases.size();
        }
    }

    int count = 0;
    for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd(); ++it) {
        const DictionaryEntry& entry = it.value();
        if (entry.word.contains("mesi", Qt::CaseInsensitive)) {

        }
        if (count++ < 10) {
            //qDebug() << count << ". 单词:" << entry.word << " 翻译:" << entry.word_cn;
        }
    }
    return loadedCount > 0;
}

QStringList ConlangDictionary::splitString(const QString& str, QChar delimiter) const {
    if (str.isEmpty()) {
        return QStringList();
    }

    return str.split(delimiter, Qt::SkipEmptyParts);
}

void ConlangDictionary::clearAllData() {
    wordToEntry.clear();
    translationToWords.clear();
    posToWords.clear();
    allWords.clear();
    firstLetterIndex.clear();
}

const DictionaryEntry* ConlangDictionary::lookupByNumIndex(int index) const {
    auto it = indexToEntry.find(index);
    return (it != indexToEntry.end()) ? &(it.value()) : nullptr;
}


bool ConlangDictionary::isMainlyChinese(const QString& str) const {
    if (str.isEmpty()) return false;

    int chineseCount = 0;
    for (const QChar& c : str) {
        ushort unicode = c.unicode();
        if ((unicode >= 0x4E00 && unicode <= 0x9FFF) ||
            (unicode >= 0x3400 && unicode <= 0x4DBF)) {
            chineseCount++;
        }
    }

    return (chineseCount * 100 / str.length()) > 40;
}

QVector<SearchResult> ConlangDictionary::searchChineseTranslation(
    const QString& query) const {

    QVector<SearchResult> results;
    QString queryTrimmed = query.trimmed();

    if (queryTrimmed.isEmpty()) return results;

    QSet<const DictionaryEntry*> addedEntries;

    // 精确匹配
    auto exactIt = translationToWords.find(queryTrimmed);
    if (exactIt != translationToWords.end()) {
        const QVector<QString>& words = exactIt.value();
        for (const QString& word : words) {
            if (const DictionaryEntry* entry = lookupByWord(word)) {
                addedEntries.insert(entry);
                results.append(SearchResult(
                    entry,
                    "translation_exact",
                    100.0,
                    QString("翻译完全匹配: %1").arg(queryTrimmed)
                    ));
            }
        }
    }

    // 部分匹配
    if (results.isEmpty()) {
        // 遍历所有翻译
        for (auto it = translationToWords.constBegin();
             it != translationToWords.constEnd(); ++it) {

            const QString& translation = it.key();

            // 检查是否包含查询词
            if (translation.contains(queryTrimmed)) {
                double score = 80.0;  // 基础分

                // 如果查询词在翻译的开头，给予更高分
                if (translation.startsWith(queryTrimmed)) {
                    score = 90.0;
                }

                const QVector<QString>& words = it.value();
                for (const QString& word : words) {
                    if (const DictionaryEntry* entry = lookupByWord(word)) {
                        if (!addedEntries.contains(entry)) {
                            addedEntries.insert(entry);
                            results.append(SearchResult(
                                entry,
                                "translation_partial",
                                score,
                                QString("翻译包含: %1").arg(queryTrimmed)
                                ));
                        }
                    }
                }
            }
        }
    }

    // 按分数排序
    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });

    return results;
}

const DictionaryEntry* ConlangDictionary::lookupByWord(const QString& word) const {
    QString wordLower = word.toLower();
    auto it = wordToEntry.find(wordLower);
    return (it != wordToEntry.end()) ? &(it.value()) : nullptr;
}

QList<int> ConlangDictionary::getAllIndices() const {
    return indexToEntry.keys();
}

QVector<const DictionaryEntry*> ConlangDictionary::lookupByExactChinese(const QString& chinese) const {
    QVector<const DictionaryEntry*> results;

    auto it = translationToWords.find(chinese);
    if (it != translationToWords.end()) {
        const QVector<QString>& words = it.value();
        results.reserve(words.size());
        for (const QString& word : words) {
            if (const DictionaryEntry* entry = lookupByWord(word)) {
                results.append(entry);
            }
        }
    }

    return results;
}

QVector<const DictionaryEntry*> ConlangDictionary::lookupByPartOfSpeech(const QString& pos) const {
    QVector<const DictionaryEntry*> results;

    QString cleanPos = pos.trimmed().toLower();
    auto it = posToWords.find(cleanPos);
    if (it != posToWords.end()) {
        const QVector<QString>& words = it.value();
        results.reserve(words.size());
        for (const QString& word : words) {
            if (const DictionaryEntry* entry = lookupByWord(word)) {
                results.append(entry);
            }
        }
    }

    return results;
}
QVector<std::pair<QString, double>> ConlangDictionary::fuzzyWordSearch(
    const QString& userInput, int maxSuggestions, double threshold) const {

    QVector<std::pair<QString, double>> candidates;

    if (userInput.isEmpty()) return candidates;

    QString inputLower = userInput.toLower();


    QVector<QString> searchSpace;
    if (!inputLower.isEmpty() && firstLetterIndex.contains(inputLower[0])) {
        searchSpace = firstLetterIndex[inputLower[0]]; 
    } else {
        searchSpace = allWords.values().toVector();
    }

    std::string inputStd = inputLower.toStdString();

    for (const QString& word : std::as_const(searchSpace)) {
        std::string wordStd = word.toStdString();
        double score = rapidfuzz::fuzz::WRatio(inputStd, wordStd);

        if (score >= threshold) {
            candidates.append(qMakePair(word, score));
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    if (candidates.size() > maxSuggestions) {
        candidates.resize(maxSuggestions);
    }

    return candidates;
}

void ConlangDictionary::handleMixedQuery(const QString& query,
                                         QVector<SearchResult>& results,
                                         QSet<const DictionaryEntry*>& addedEntries) const {
    QStringList parts = splitString(query, ' ');

    if (parts.size() < 2) return;

    QHash<const DictionaryEntry*, double> entryScores;

   
    for (const QString& part : std::as_const(parts)) {
        if (part.isEmpty()) continue;

        QVector<SearchResult> partResults = smartSearch(part, 20);
       
        for (const SearchResult& result : std::as_const(partResults)) {
            entryScores[result.entry] += result.score;
        }
    }
    for (auto it = entryScores.constBegin(); it != entryScores.constEnd(); ++it) {
        const DictionaryEntry* entry = it.key();
        if (!addedEntries.contains(entry)) {
            addedEntries.insert(entry);
            double avgScore = it.value() / parts.size();
            results.append(SearchResult(entry, "mixed_query", avgScore, "混合查询匹配"));
        }
    }
}

bool ConlangDictionary::isInteger(const QString& s) {
    bool ifok;
    s.toInt(&ifok);
    return ifok;
}

QVector<SearchResult> ConlangDictionary::smartSearch(const QString& query, int maxResults) const {

    QVector<SearchResult> allResults;
    QSet<const DictionaryEntry*> addedEntries;
    QString trimmedQuery = query.trimmed();

    if (query.isEmpty()) {
        return allResults;
    }

    if (isInteger(query)) {
        bool ok;
        int index = query.toInt(&ok);
        if (ok && index > 0) {   // 索引从1开始
            const DictionaryEntry* entry = lookupByNumIndex(index);
            if (entry) {
                addedEntries.insert(entry);
                allResults.append(SearchResult(
                    entry,
                    "index_match",
                    100.0,
                    QString("索引 %1 匹配").arg(index)
                    ));
                return allResults;
            } else {
                return allResults;
            }
        }
    }
    bool isChinese = isMainlyChinese(trimmedQuery);

    const DictionaryEntry* wordEntry = lookupByWord(trimmedQuery);
    if (wordEntry) {

        addedEntries.insert(wordEntry);
        allResults.append(SearchResult(
            wordEntry,
            "word_exact",
            100.0,
            "单词完全匹配"
            ));
    } else {
        //qDebug() << "未找到精确匹配单词";
    }

    auto phraseResults = findPhrasesContainingWord(trimmedQuery);

    for (const SearchResult& result : std::as_const(phraseResults)) {
        if (!addedEntries.contains(result.entry)) {
            addedEntries.insert(result.entry);
            allResults.append(result);
        }
    }

    if (isChinese || allResults.isEmpty()) {
        auto chineseResults = searchChineseDynamic(trimmedQuery);

        for (const SearchResult& result : std::as_const(chineseResults)) {
            if (!addedEntries.contains(result.entry)) {
                addedEntries.insert(result.entry);
                allResults.append(result);
            }
        }
    }

    if (trimmedQuery.contains(' ')) {
        auto exactPhraseResults = findExactPhrase(trimmedQuery);
        for (const SearchResult& result : std::as_const(exactPhraseResults)) {
            if (!addedEntries.contains(result.entry)) {
                addedEntries.insert(result.entry);
                allResults.append(result);
            }
        }

        if (isChinese && exactPhraseResults.isEmpty()) {

            auto phraseTransResults = findPhraseTranslation(trimmedQuery);


            for (const SearchResult& result : std::as_const(phraseTransResults)) {
                if (!addedEntries.contains(result.entry)) {
                    addedEntries.insert(result.entry);
                    allResults.append(result);
                }
            }
        }
    }

    if (allResults.isEmpty()) {

        if (!isChinese) {
            auto fuzzyResults = fuzzyWordSearch(trimmedQuery, maxResults, 50.0);

            for (const auto& [fuzzyWord, score] : std::as_const(fuzzyResults)) {
                if (const DictionaryEntry* entry = lookupByWord(fuzzyWord)) {
                    if (!addedEntries.contains(entry)) {
                        addedEntries.insert(entry);
                        allResults.append(SearchResult(
                            entry,
                            "word_fuzzy",
                            score,
                            QString("模糊匹配: %1").arg(fuzzyWord)
                            ));

                        auto morePhraseResults = findPhrasesContainingWord(fuzzyWord);
                        for (const SearchResult& phraseResult : std::as_const(morePhraseResults)) {
                            if (!addedEntries.contains(phraseResult.entry)) {
                                addedEntries.insert(phraseResult.entry);
                                allResults.append(phraseResult);
                            }
                        }
                    }
                }
            }
        }
    }

    QVector<SearchResult> finalResults;
    QSet<const DictionaryEntry*> uniqueEntries;

    for (const SearchResult& result : std::as_const(allResults)) {
        if (!uniqueEntries.contains(result.entry)) {
            uniqueEntries.insert(result.entry);
            finalResults.append(result);
        }
    }

    auto comparator = [](const SearchResult& a, const SearchResult& b) {
        static const QHash<QString, int> priority = {
            {"word_exact", 100},
            {"translation_exact", 100},
            {"phrase_exact_match", 95},
            {"phrase_exact_word", 95},
            {"part_of_speech", 95},
            {"phrase_contains_word", 90},
            {"word_fuzzy", 85},
            {"translation_partial", 75}
        };

        int prioA = priority.value(a.matchedby, 50);
        int prioB = priority.value(b.matchedby, 50);

        if (prioA != prioB) return prioA > prioB;
        return a.score > b.score;
    };

    std::sort(finalResults.begin(), finalResults.end(), comparator);

    if (finalResults.size() > maxResults) {
        finalResults.resize(maxResults);
    }


    return finalResults;
}


void ConlangDictionary::printStatistics() const {
    // qDebug() << "\n=== 词典统计信息 ===";
    // qDebug() << "总词条数:" << wordToEntry.size();
    // qDebug() << "中文翻译数:" << translationToWords.size();
    // qDebug() << "词性索引数:" << posToWords.size();
    // qDebug() << "首字母索引数:" << firstLetterIndex.size();
    // qDebug() << "所有单词数:" << allWords.size();

    // 显示首字母分布
    // qDebug() << "\n首字母分布:";
    QMap<QChar, int> letterCounts;
    for (auto it = firstLetterIndex.constBegin(); it != firstLetterIndex.constEnd(); ++it) {
        letterCounts[it.key()] = it.value().size();
    }

    for (auto it = letterCounts.constBegin(); it != letterCounts.constEnd(); ++it) {
        // qDebug() << "  " << it.key() << ":" << it.value() << "个单词";
    }
}

void ConlangDictionary::runTests() const {
    // qDebug() << "\n=== 运行词典测试 ===";

    // 测试基本搜索
    if (const DictionaryEntry* entry = lookupByWord("mesi")) {
        // qDebug() << "✓ 单词查询测试通过:" << entry->word << "->" << entry->word_cn;
        if (!entry->phrases.isEmpty()) {
            // qDebug() << "  包含词组:";
            for (int i = 0; i < entry->phrases.size(); ++i) {
                // qDebug() << "    " << entry->phrases[i] << "->" << entry->phrases_cn[i];
            }
        }
    } else {
        // qDebug() << "✗ 单词查询测试失败";
    }

    // 测试模糊搜索
    auto fuzzyResults = fuzzyWordSearch("meis", 3, 50.0);
    if (!fuzzyResults.isEmpty()) {
        // qDebug() << "✓ 模糊查询测试通过，找到" << fuzzyResults.size() << "个建议";
        // 修复：使用传统的 for 循环避免警告
        for (int i = 0; i < fuzzyResults.size(); ++i) {
            const auto& [word, score] = fuzzyResults[i];
            // qDebug() << "  建议:" << word << "(" << score << "%)";
        }
    } else {
        // qDebug() << "✗ 模糊查询测试失败";
    }

    // 测试词性搜索
    auto posResults = lookupByPartOfSpeech("n.");
    if (!posResults.isEmpty()) {
        // qDebug() << "✓ 词性搜索测试通过，找到" << posResults.size() << "个结果";
    } else {
        // qDebug() << "✗ 词性搜索测试失败";
    }

    // 测试词组搜索
    auto phraseResults = findExactPhrase("mesi huhuhu");
    if (!phraseResults.isEmpty()) {
        // qDebug() << "✓ 词组搜索测试通过，找到" << phraseResults.size() << "个结果";
       
        for (const SearchResult& sr : std::as_const(phraseResults)) {
            // qDebug() << "  结果:" << sr.entry->word << "->" << sr.entry->word_cn;
        }
    } else {
        // qDebug() << "✗ 词组搜索测试失败";
    }

    // 测试中文词组搜索
    auto phraseChineseResults = findPhraseTranslation("米水航空");
    if (!phraseChineseResults.isEmpty()) {
        // qDebug() << "✓ 中文词组搜索测试通过，找到" << phraseChineseResults.size() << "个结果";
    } else {
        // qDebug() << "✗ 中文词组搜索测试失败";
    }

    // qDebug() << "=== 测试完成 ===";
}

QVector<SearchResult> ConlangDictionary::searchChineseDynamic(const QString& query) const {
    QVector<SearchResult> results;
    QString queryTrimmed = query.trimmed();

    if (queryTrimmed.isEmpty()) return results;

    QSet<const DictionaryEntry*> addedEntries;
    bool likelyChinese = isMainlyChinese(queryTrimmed);

    // 方法1：精确匹配
    auto exactIt = translationToWords.find(queryTrimmed);
    if (exactIt != translationToWords.end()) {
        const QVector<QString>& words = exactIt.value();
        for (const QString& word : words) {
            if (const DictionaryEntry* entry = lookupByWord(word)) {
                addedEntries.insert(entry);
                results.append(SearchResult(
                    entry,
                    "translation_exact",
                    100.0,
                    QString("翻译完全匹配: %1").arg(queryTrimmed)
                    ));
            }
        }
    }

    // 方法2：部分匹配（只有中文查询才做）
    if (likelyChinese && results.isEmpty()) {
        // 遍历所有翻译
        for (auto it = translationToWords.constBegin();
             it != translationToWords.constEnd(); ++it) {

            const QString& translation = it.key();

            // 检查是否包含查询词
            if (translation.contains(queryTrimmed)) {
                double score = 80.0;  // 基础分

                // 如果查询词在翻译的开头，给予更高分
                if (translation.startsWith(queryTrimmed)) {
                    score = 90.0;
                }

                // 如果查询词长度较长，给予更高分
                if (queryTrimmed.length() >= 3) {
                    score += (queryTrimmed.length() - 2) * 5;
                    score = qMin(score, 95.0);
                }

                const QVector<QString>& words = it.value();
                for (const QString& word : words) {
                    if (const DictionaryEntry* entry = lookupByWord(word)) {
                        if (!addedEntries.contains(entry)) {
                            addedEntries.insert(entry);
                            results.append(SearchResult(
                                entry,
                                "translation_partial",
                                score,
                                QString("翻译包含: %1").arg(queryTrimmed)
                                ));
                        }
                    }
                }
            }
        }
    }

    // 按分数排序
    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });

    return results;
}

// 查找包含单词的词组 - O(n*m)，n=词条数，m=平均词组数
QVector<SearchResult> ConlangDictionary::findPhrasesContainingWord(const QString& word) const {
    QVector<SearchResult> results;
    QString wordLower = word.trimmed().toLower();

    if (wordLower.isEmpty()) return results;

    // 遍历所有词条
    for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd(); ++it) {
        const DictionaryEntry& entry = it.value();

        // 检查这个词条的所有词组
        for (int i = 0; i < entry.phrases.size(); ++i) {
            const QString& phrase = entry.phrases[i];
            QString phraseLower = phrase.toLower();

            // 方法1：词组完全等于单词
            if (phraseLower == wordLower) {
                results.append(SearchResult(
                    &entry,
                    "phrase_exact_word",
                    100.0,
                    QString("词组等于单词: %1").arg(phrase)
                    ));
                break;  // 一个词条找到一个即可
            }

            // 方法2：词组包含单词
            QStringList phraseWords = phraseLower.split(' ', Qt::SkipEmptyParts);
            if (phraseWords.contains(wordLower)) {
                // 计算匹配分数
                double score = 95.0;
                // 如果词组以单词开头，分数更高
                if (phraseLower.startsWith(wordLower + " ")) {
                    score = 98.0;
                }

                results.append(SearchResult(
                    &entry,
                    "phrase_contains_word",
                    score,
                    QString("词组包含: %1").arg(phrase)
                    ));
                break;  // 一个词条找到一个即可
            }
        }
    }

    return results;
}

// 查找完整词组 - O(n*m)
QVector<SearchResult> ConlangDictionary::findExactPhrase(const QString& phrase) const {
    QVector<SearchResult> results;
    QString phraseLower = phrase.trimmed().toLower();

    if (phraseLower.isEmpty()) return results;

    // 遍历所有词条
    for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd(); ++it) {
        const DictionaryEntry& entry = it.value();

        // 检查是否完全匹配某个词组
        for (const QString& entryPhrase : entry.phrases) {
            if (entryPhrase.trimmed().toLower() == phraseLower) {
                results.append(SearchResult(
                    &entry,
                    "phrase_exact_match",
                    100.0,
                    QString("完全匹配词组: %1").arg(phrase)
                    ));
                break;  // 一个词条找到一个即可
            }
        }
    }

    return results;
}

// 查找中文词组翻译 - O(n*m)
QVector<SearchResult> ConlangDictionary::findPhraseTranslation(const QString& translation) const {
    QVector<SearchResult> results;
    QString translationTrimmed = translation.trimmed();

    if (translationTrimmed.isEmpty()) return results;

    // 遍历所有词条
    for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd(); ++it) {
        const DictionaryEntry& entry = it.value();

        // 检查词组翻译
        for (const QString& phraseTrans : entry.phrases_cn) {
            if (phraseTrans.trimmed() == translationTrimmed) {
                results.append(SearchResult(
                    &entry,
                    "phrase_translation_exact",
                    100.0,
                    QString("词组翻译: %1").arg(translation)
                    ));
                break;  // 一个词条找到一个即可
            }
        }
    }

    return results;
}

void ConlangDictionary::testSearchFunctions() {
    ConlangDictionary dict;

    // 加载词典
    bool success = dict.loadDictionaryFiles(
        "../values.txt",
        "../keys.txt",
        "../wordtype.txt",
        "../phrases.txt",
        "../cn_phrases.txt"
        );

    if (!success) {
        // qDebug() << "加载词典失败";
        return;
    }

    // qDebug() << "\n=== 测试搜索功能 ===";

    // 测试1：搜索单词 "mesi"
    // qDebug() << "\n1. 测试搜索 'mesi':";
    auto results1 = dict.smartSearch("mesi", 10);

    if (results1.isEmpty()) {
        // qDebug() << "   没有找到结果";
    } else {
        // qDebug() << "   找到 " << results1.size() << " 个结果:";
        for (int i = 0; i < results1.size(); ++i) {
            const auto& result = results1[i];
            // qDebug() << "   " << (i+1) << ". " << result.entry->word
            //         << " [" << result.matchedby << "]"
            //         << " 分数: " << result.score;
            // qDebug() << "       高亮信息: " << result.highlightinfo;
        }
    }

    // 测试2：搜索完整词组 "mesi huhuhu"
    // qDebug() << "\n2. 测试搜索 'mesi huhuhu':";
    auto results2 = dict.smartSearch("mesi huhuhu", 10);

    if (results2.isEmpty()) {
        // qDebug() << "   没有找到结果";
    } else {
        // qDebug() << "   找到 " << results2.size() << " 个结果";
        for (int i = 0; i < results2.size(); ++i) {
            const auto& result = results2[i];
            // qDebug() << "   " << (i+1) << ". " << result.entry->word
            //         << " [" << result.matchedby << "]";
        }
    }

    // 测试3：搜索中文翻译 "米水"
    // qDebug() << "\n3. 测试搜索 '米水':";
    auto results3 = dict.smartSearch("米水", 10);

    if (results3.isEmpty()) {
        // qDebug() << "   没有找到结果";
    } else {
        // qDebug() << "   找到 " << results3.size() << " 个结果";
    }
}

QString ConlangDictionary::translateSentence(const QString& query) const {
    QString result;
    // 判断长句语言，是中文为真，是英语为假
    bool languageisChinese = isMainlyChinese(query);
    // 使用空格切割
    QStringList splitedQuery = query.split(" ");
    for (const QString& word : std::as_const(splitedQuery)) {
        // .txt文件中的解释是给人看的，不是用来翻译的。这里用来检测并替换需要从给人看变成正是翻译的词语
        // 。。。。。等待实现↑

        QVector<SearchResult> wordResult = smartSearch(word, 1);
        // 根据语言判断翻译后词条
        QString transedWord = languageisChinese ? wordResult[0].entry->word : wordResult[0].entry->word_cn;

        // 不是中文的话，需要添加空格
        if (!languageisChinese){
            result += transedWord + " ";
        }
    }
    // 是中文加上中文逗号，是英文加上英文逗号
    result += languageisChinese ? "。" : ".";
    qDebug() << "translate函数通过";
    return result;
}

bool ConlangDictionary::isSentence(QString &query){
    QChar querylast = query[query.length() - 1];
    qDebug() << querylast;
    if (querylast == '.') {
        //qDebug() << "进入了if";
        return true;
    }
    //qDebug() << "过了";
    return false;
}
