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

// ========== 核心数据管理 ==========

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
    // 清除所有现有数据
    clearAllData();

    // 打开所有文件
    QFile fWords(valuesFile), fTrans(keysFile), fPos(posFile),
        fPhrases(phrasesFile), fPhraseTrans(phraseTransFile);

    // 创建文件列表便于统一处理
    QList<QFile*> files = {&fWords, &fTrans, &fPos, &fPhrases, &fPhraseTrans};
    QStringList filenames = {valuesFile, keysFile, posFile, phrasesFile, phraseTransFile};

    // 检查文件是否成功打开
    for (int i = 0; i < files.size(); ++i) {
        if (!files[i]->open(QIODevice::ReadOnly | QIODevice::Text)) {
            // qDebug() << "错误：无法打开文件" << filenames[i];
            return false;
        }
    }

    // 创建文本流
    QTextStream inWords(&fWords), inTrans(&fTrans), inPos(&fPos),
        inPhrases(&fPhrases), inPhraseTrans(&fPhraseTrans);

    QString word, trans, pos, phraseLine, phraseTransLine;
    int lineNum = 0;
    int loadedCount = 0;

    // 逐行读取（五个文件行号必须对齐）
    while (!inWords.atEnd()) {
        // 读取一行数据
        word = inWords.readLine().trimmed();
        lineNum++;

        // 同步读取其他四个文件
        trans = inTrans.readLine().trimmed();
        pos = inPos.readLine().trimmed();
        phraseLine = inPhrases.readLine().trimmed();
        phraseTransLine = inPhraseTrans.readLine().trimmed();

        // 检查是否有文件提前结束（行数不匹配）
        // 这里可以加上phraseLine.isNull() || phraseTransLine.isNull()，作为条件
        if (trans.isNull() || pos.isNull()) {
            // qDebug() << "警告：文件行数不匹配（第" << lineNum << "行）";
            break;
        }

        // 跳过空行（单词和翻译都为空时）
        if (word.isEmpty() && trans.isEmpty()) {
            continue;
        }

        // ========== 解析词组 ==========
        // 词组文件中每行可能有多个词组，用逗号分隔

        // 使用 splitString 函数解析逗号分隔的词组
        QStringList phrasesVec = splitString(phraseLine, ',');
        QStringList phraseTransVec = splitString(phraseTransLine, ',');

        // 确保词组和翻译数量匹配
        if (phrasesVec.size() != phraseTransVec.size()) {
            // qDebug() << "警告：词组与翻译数量不匹配（第" << lineNum << "行）";
            // 取较小数量，确保一一对应
            int minSize = qMin(phrasesVec.size(), phraseTransVec.size());
            if (phrasesVec.size() > minSize) {
                phrasesVec = phrasesVec.mid(0, minSize);
            }
            if (phraseTransVec.size() > minSize) {
                phraseTransVec = phraseTransVec.mid(0, minSize);
            }
        }

        // ========== 创建词条对象 ==========
        DictionaryEntry entry;
        entry.word = word;                            // 原始单词（保持大小写）
        entry.word_cn = trans;                        // 中文翻译
        entry.partOfSpeech = pos;                     // 词性
        entry.phrases = phrasesVec.toVector();        // 词组数组
        entry.phrases_cn = phraseTransVec.toVector(); // 词组翻译数组
        entry.index = lineNum;

        // 存储到主索引
        // 预处理：单词统一为小写（用于索引）
        QString wordLower = word.toLower();
        wordToEntry[wordLower] = entry;
        //const DictionaryEntry* entryPtr = &wordToEntry[wordLower];

        // 关键：获取存储的entry的指针，并添加到索引映射
        // 注意：wordToEntry[wordLower]返回的是引用，我们获取它的地址
        indexToEntry[lineNum] = entry;
        // ========== 构建快速索引（单词/翻译/词性） ==========

        // 1. 单词 -> 词条（主索引）
        wordToEntry[wordLower] = entry;

        // 2. 所有单词集合（用于遍历）
        allWords.insert(wordLower);

        // 3. 首字母索引（优化模糊搜索）
        if (!wordLower.isEmpty()) {
            firstLetterIndex[wordLower[0]].append(wordLower);
        }

        // 4. 中文翻译 -> 单词（反向索引）
        if (!trans.isEmpty()) {
            // 注意：这里假设翻译是一对一的，如果有多个单词有相同翻译，它们会被添加到同一个列表中
            translationToWords[trans].append(wordLower);
        }

        // 5. 词性 -> 单词（词性索引）
        if (!pos.isEmpty()) {
            // 词性可能有多个，用分号分隔
            QStringList posList = splitString(pos, ';');
            for (const QString& singlePos : std::as_const(posList)) {
                QString cleanPos = singlePos.trimmed().toLower();
                if (!cleanPos.isEmpty()) {
                    posToWords[cleanPos].append(wordLower);
                }
            }
        }

        // 6 索引 -> 单词（供搜索或者创建所有词汇页面）
        // 已经在上面创建完成！相对位置：-33行，绝对位置：130行

        // 词组信息已经存储在 entry.phrases 和 entry.phrases_cn 中
        // 在搜索时动态遍历所有词条来查找词组
        // 这对于少量词组的自创语言词典是完全可以接受的

        loadedCount++;
    }

    // ========== 加载完成后的统计和验证 ==========

    // 关闭所有文件
    for (auto f : std::as_const(files)) {
        f->close();
    }

    // 输出加载统计
    // qDebug() << "\n=== 词典加载完成 ===";
    // qDebug() << "总词条数:" << loadedCount;
    // qDebug() << "单词索引大小:" << wordToEntry.size();
    // qDebug() << "中文翻译索引大小:" << translationToWords.size();
    // qDebug() << "词性索引大小:" << posToWords.size();
    // qDebug() << "首字母索引大小:" << firstLetterIndex.size();

    // 检查是否有重复单词（大小写不敏感）
    if (wordToEntry.size() != loadedCount) {
        // qDebug() << "警告：存在大小写不敏感的重复单词！";
    }

    // 验证数据完整性
    if (loadedCount > 0) {
        // qDebug() << "词典加载成功！";

        // 显示示例数据（前3个词条）
        // qDebug() << "\n示例数据（前3个词条）:";
        int count = 0;
        for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd() && count < 3; ++it) {
            const DictionaryEntry& entry = it.value();
            // qDebug() << "单词:" << entry.word;
            // qDebug() << "翻译:" << entry.word_cn;
            // qDebug() << "词性:" << entry.partOfSpeech;

            if (!entry.phrases.isEmpty()) {
                // qDebug() << "词组:";
                for (int i = 0; i < entry.phrases.size(); ++i) {
                    // qDebug() << "  " << entry.phrases[i] << " -> " << entry.phrases_cn[i];
                }
            }
            // qDebug() << "---";
            count++;
        }

        // 显示词组总数统计
        int totalPhrases = 0;
        for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd(); ++it) {
            totalPhrases += it.value().phrases.size();
        }
        // qDebug() << "总词组数:" << totalPhrases;
        // qDebug() << "平均每个词条词组数:" << (totalPhrases * 1.0 / loadedCount);
    }
    // 在loadDictionaryFiles函数末尾，return之前添加：
    // qDebug() << "\n=== 完整词条列表 ===";
    int count = 0;
    for (auto it = wordToEntry.constBegin(); it != wordToEntry.constEnd(); ++it) {
        const DictionaryEntry& entry = it.value();
        if (entry.word.contains("mesi", Qt::CaseInsensitive)) {
            // qDebug() << "找到包含'mesi'的词条:";
            // qDebug() << "  单词:" << entry.word;
            // qDebug() << "  翻译:" << entry.word_cn;
            // qDebug() << "  词组:" << entry.phrases;
        }
        if (count++ < 10) {  // 只显示前10个
            // qDebug() << count << ". 单词:" << entry.word << " 翻译:" << entry.word_cn;
        }
    }
    return loadedCount > 0;
}

// 辅助函数 splitString 的实现
QStringList ConlangDictionary::splitString(const QString& str, QChar delimiter) const {
    if (str.isEmpty()) {
        return QStringList();
    }

    // 使用Qt内置的split函数，跳过空的部分
    return str.split(delimiter, Qt::SkipEmptyParts);
}

// clearAllData 函数实现
void ConlangDictionary::clearAllData() {
    wordToEntry.clear();
    translationToWords.clear();
    posToWords.clear();
    allWords.clear();
    firstLetterIndex.clear();

    // 注意：我们不清理词组索引，因为我们没有构建词组索引
}

const DictionaryEntry* ConlangDictionary::lookupByNumIndex(int index) const {
    auto it = indexToEntry.find(index);
    return (it != indexToEntry.end()) ? &(it.value()) : nullptr;
}

// ========== 中文处理相关 ==========

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

// ========== 基础搜索功能 ==========
// 代替原来的中文关键词搜索
QVector<SearchResult> ConlangDictionary::searchChineseTranslation(
    const QString& query) const {

    QVector<SearchResult> results;
    QString queryTrimmed = query.trimmed();

    if (queryTrimmed.isEmpty()) return results;

    QSet<const DictionaryEntry*> addedEntries;

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

    // 方法2：部分匹配（简单实现，不提取关键词）
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



// ========== 新增搜索功能 ==========

// 单词精确查找 - O(1)
const DictionaryEntry* ConlangDictionary::lookupByWord(const QString& word) const {
    QString wordLower = word.toLower();
    auto it = wordToEntry.find(wordLower);
    return (it != wordToEntry.end()) ? &(it.value()) : nullptr;
}

QList<int> ConlangDictionary::getAllIndices() const {
    return indexToEntry.keys();
}

// 中文翻译精确查找 - O(1)
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

// 词性搜索 - O(1) 查找 + O(k) 获取结果
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

// 模糊搜索 - 使用首字母索引优化
QVector<std::pair<QString, double>> ConlangDictionary::fuzzyWordSearch(
    const QString& userInput, int maxSuggestions, double threshold) const {

    QVector<std::pair<QString, double>> candidates;

    if (userInput.isEmpty()) return candidates;

    QString inputLower = userInput.toLower();

    // 使用首字母索引缩小搜索范围
    QVector<QString> searchSpace;
    if (!inputLower.isEmpty() && firstLetterIndex.contains(inputLower[0])) {
        searchSpace = firstLetterIndex[inputLower[0]];  // 只搜索相同首字母的单词
    } else {
        searchSpace = allWords.values().toVector();     // 回退到全部单词
    }

    // 计算相似度
    std::string inputStd = inputLower.toStdString();

    for (const QString& word : std::as_const(searchSpace)) {
        std::string wordStd = word.toStdString();
        double score = rapidfuzz::fuzz::WRatio(inputStd, wordStd);

        if (score >= threshold) {
            candidates.append(qMakePair(word, score));
        }
    }

    // 排序并限制数量
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    if (candidates.size() > maxSuggestions) {
        candidates.resize(maxSuggestions);
    }

    return candidates;
}

// ========== 智能搜索 ==========

void ConlangDictionary::handleMixedQuery(const QString& query,
                                         QVector<SearchResult>& results,
                                         QSet<const DictionaryEntry*>& addedEntries) const {
    QStringList parts = splitString(query, ' ');

    if (parts.size() < 2) return;

    QHash<const DictionaryEntry*, double> entryScores;

    // 修复：使用 std::as_const 避免警告
    for (const QString& part : std::as_const(parts)) {
        if (part.isEmpty()) continue;

        QVector<SearchResult> partResults = smartSearch(part, 20);
        // 修复：使用 std::as_const 避免警告
        for (const SearchResult& result : std::as_const(partResults)) {
            entryScores[result.entry] += result.score;
        }
    }

    // 修复：使用 constBegin/constEnd 避免警告
    for (auto it = entryScores.constBegin(); it != entryScores.constEnd(); ++it) {
        const DictionaryEntry* entry = it.key();
        if (!addedEntries.contains(entry)) {
            addedEntries.insert(entry);
            double avgScore = it.value() / parts.size();
            results.append(SearchResult(entry, "mixed_query", avgScore, "混合查询匹配"));
        }
    }
}

// 检查是否为整数
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
        // qDebug() << "查询为空，返回空结果";
        return allResults;
    }

    if (isInteger(query)) {
        bool ok;
        int index = query.toInt(&ok);
        if (ok && index > 0) {   // 索引从1开始
            // // qDebug() << "已经进入if";
            const DictionaryEntry* entry = lookupByNumIndex(index);
            if (entry) {
                // // qDebug() << "   ✓ 通过索引找到词条: " << entry->word;
                addedEntries.insert(entry);
                allResults.append(SearchResult(
                    entry,
                    "index_match",
                    100.0,
                    QString("索引 %1 匹配").arg(index)
                    ));
                // 索引匹配通常非常精准，可以直接返回，避免继续搜索
                // 直接返回结果（仅索引匹配）
                // if (allResults.empty()) {
                //     // qDebug() << "返回了一个空的结果";
                // }
                return allResults;
            } else {
                // // qDebug() << "   ✗ 索引" << index << "无对应词条";
                return allResults;
            }
        }
    }
    // // qDebug() << "==================词语搜索==================";
    bool isChinese = isMainlyChinese(trimmedQuery);
    // // qDebug() << "修剪后查询: \"" << trimmedQuery << "\", 是否中文: " << isChinese;

    // ========== 第一阶段：单词精确匹配 ==========
    // qDebug() << "\n1. 尝试单词精确匹配...";

    // 无论是否中文，都尝试单词精确匹配
    const DictionaryEntry* wordEntry = lookupByWord(trimmedQuery);
    if (wordEntry) {
        // qDebug() << "   ✓ 找到精确匹配单词: " << wordEntry->word << " -> " << wordEntry->word_cn;
        addedEntries.insert(wordEntry);
        allResults.append(SearchResult(
            wordEntry,
            "word_exact",
            100.0,
            "单词完全匹配"
            ));
    } else {
        // qDebug() << "   ✗ 未找到精确匹配单词";
    }

    // ========== 第二阶段：查找包含该单词的词组 ==========
    // qDebug() << "\n2. 查找包含该单词的词组...";
    auto phraseResults = findPhrasesContainingWord(trimmedQuery);

    for (const SearchResult& result : std::as_const(phraseResults)) {
        if (!addedEntries.contains(result.entry)) {
            addedEntries.insert(result.entry);
            allResults.append(result);
            // qDebug() << "   添加词组结果: " << result.entry->word
            //         << " [" << result.matchedby << "]";
        }
    }

    // ========== 第三阶段：中文翻译搜索 ==========
    if (isChinese || allResults.isEmpty()) {
        // qDebug() << "\n3. 尝试中文翻译搜索...";
        auto chineseResults = searchChineseDynamic(trimmedQuery);
        // qDebug() << "   找到 " << chineseResults.size() << " 个中文翻译结果";

        for (const SearchResult& result : std::as_const(chineseResults)) {
            if (!addedEntries.contains(result.entry)) {
                addedEntries.insert(result.entry);
                allResults.append(result);
            }
        }
    }

    // ========== 第四阶段：完整词组匹配 ==========
    if (trimmedQuery.contains(' ')) {
        // qDebug() << "\n4. 尝试完整词组匹配...";
        auto exactPhraseResults = findExactPhrase(trimmedQuery);
        // qDebug() << "   找到 " << exactPhraseResults.size() << " 个完整词组匹配";

        for (const SearchResult& result : std::as_const(exactPhraseResults)) {
            if (!addedEntries.contains(result.entry)) {
                addedEntries.insert(result.entry);
                allResults.append(result);
            }
        }

        // 中文词组翻译匹配
        if (isChinese && exactPhraseResults.isEmpty()) {
            // qDebug() << "   尝试中文词组翻译匹配...";
            auto phraseTransResults = findPhraseTranslation(trimmedQuery);
            // qDebug() << "   找到 " << phraseTransResults.size() << " 个中文词组翻译匹配";

            for (const SearchResult& result : std::as_const(phraseTransResults)) {
                if (!addedEntries.contains(result.entry)) {
                    addedEntries.insert(result.entry);
                    allResults.append(result);
                }
            }
        }
    }

    // ========== 第五阶段：模糊搜索 ==========
    if (allResults.isEmpty()) {
        // qDebug() << "\n5. 尝试模糊搜索...";

        // 单词模糊匹配
        if (!isChinese) {
            auto fuzzyResults = fuzzyWordSearch(trimmedQuery, maxResults, 50.0);
            // qDebug() << "   找到 " << fuzzyResults.size() << " 个模糊匹配";

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

                        // 对于模糊匹配的单词，也查找包含它的词组
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

    // ========== 第六阶段：排序和去重 ==========
    // qDebug() << "\n6. 排序和去重...";
    // qDebug() << "   原始结果数: " << allResults.size();

    // 去重
    QVector<SearchResult> finalResults;
    QSet<const DictionaryEntry*> uniqueEntries;

    for (const SearchResult& result : std::as_const(allResults)) {
        if (!uniqueEntries.contains(result.entry)) {
            uniqueEntries.insert(result.entry);
            finalResults.append(result);
        }
    }

    // qDebug() << "   去重后结果数: " << finalResults.size();

    // 按分数排序
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

    // 限制结果数量
    if (finalResults.size() > maxResults) {
        finalResults.resize(maxResults);
    }

    // qDebug() << "=== smartSearch: 搜索完成，返回 " << finalResults.size() << " 个结果 ===";

    return finalResults;
}

// ========== 统计和测试 ==========

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
        // 修复：使用 std::as_const 避免警告
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
        qDebug() << "进入了if";
        return true;
    }
    qDebug() << "过了";
    return false;
}
