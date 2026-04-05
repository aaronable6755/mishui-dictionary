#include "widget.h"
#include "ui_widget.h"
#include "ConlangDictionary.h"
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QStyleHints>
#include <QTimer>

QStringList readFileLineByLine(const QString& path) {
    QStringList lines;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件,词典页无法渲染" << path;
        return lines;
    }

    QTextStream in (&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }

    file.close();
    return lines;
};

// 初始化
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , m_dictLoaded(false)
{
    ui->setupUi(this);
    ui->stackedWidget->setCurrentIndex(1);
    // 1lluw9capfxmkzw1trxay_g pwd 0721

/////////////////////////////////////////////////////////////


    // 创建新的容器 widget
    /*widget_container_scrollArea = new QWidget(ui->scrollArea);
    ui->scrollArea->setWidget(widget_container_scrollArea);
    //ui->scrollArea->setWidgetResizable(true);*/ // 允许内容部件随视口调整
    ui->scrollArea->setWidgetResizable(false);
    // 获取当前应用程序的调色板
    QPalette palette = qApp->palette();

    palette.setColor(QPalette::Highlight, QColor(0, 204, 106)); // 绿色#26FF97

    // 设置高亮文本颜色（选中时文字颜色）
    palette.setColor(QPalette::HighlightedText, Qt::white);

    // 将新的调色板应用到整个应用程序
    qApp->setPalette(palette);

    // qDebug() << "ContentWidget size:" << ui->scrollAreaWidgetContents_2->size();
    // qDebug() << "Viewport size:" << ui->scrollArea->viewport()->size();
    // qDebug() << "ScrollBar policies: H=" << ui->scrollArea->horizontalScrollBarPolicy()
    //          << " V=" << ui->scrollArea->verticalScrollBarPolicy();
    // qDebug() << "ScrollArea size:" << ui->scrollArea->size();
    // qDebug() << "ScrollArea minimumSize:" << ui->scrollArea->minimumSize();
    // qDebug() << "ScrollArea maximumSize:" << ui->scrollArea->maximumSize();
    ui->scrollAreaWidgetContents_2->updateGeometry();
    ui->scrollArea->updateGeometry();
    ui->scrollArea->viewport()->update();
    ui->lineEdit_translate->setStyleSheet(
        "QLineEdit::selection {"
        "    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,"
        "                                stop: 0 #FF6B6B, stop: 0.5 #4ECDC4, stop: 1 #FF6B6B);"
        "    color: white;"  // 选中文本的颜色
        "}"
        );
    //qDebug() << "去D二u不过（";
    font = ui->label_result->font();
    font.setPointSize(19);

    ui->vocabScrollArea->setWidgetResizable(true);
    gridlayout = new QGridLayout(ui->vocabulary);
    ui->vocabulary->setLayout(gridlayout);
    ui->gridLayout_dict->expandingDirections();

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                this, &Widget::onColorSchemeChanged);

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme scheme){
                createVocabPage(4, "all");
            });
    Qt::ColorScheme currentScheme = QGuiApplication::styleHints()->colorScheme();
    onColorSchemeChanged(currentScheme);
    // // qDebug() << "词典加载中...";


    m_dictLoaded = m_dictionary.loadDictionaryFiles("./texts/values.txt",
                                                    "./texts/keys.txt",
                                                    "./texts/wordtype.txt",
                                                    "./texts/phrases.txt",
                                                    "./texts/cn_phrases.txt");

    if (m_dictLoaded){
        // qDebug() << "字典加载完成！下一步：自检";
        m_dictionary.runTests();

        ui->label_result->setText("正确词汇：<br>最佳翻译：<br>词性：<br>相关词组：<br>翻译：<br>例句：<br>例句翻译：<br><font color = \"#27FF97\">拼写没有问题!</font>");
        ui->label_2->setText("加载已完成！");
        ui->label_2->setStyleSheet("color: #27FF97");
    } else {
        // qDebug() << "词典初始化失败！";
        ui->label_result->setText("请确保texts与mesi_polo.exe位于同一目录下!");
        ui->label_result->setStyleSheet("color: #FF0D0D"); // 红色
        ui->label_2->setText("词典加载失败，功能不可用");
        ui->label_2->setStyleSheet("color: #FF0D0D"); // 红色

        ui->lineEdit_translate->setEnabled(false);
    }

    // 在构造函数末尾或创建完所有标签后
    // QTimer::singleShot(100, this, [this]() {
    //     qDebug() << "--- After show ---";
    //     qDebug() << "ScrollArea size:" << ui->scrollArea->size();
    //     qDebug() << "Viewport size:" << ui->scrollArea->viewport()->size();
    //     qDebug() << "ContentWidget size:" << ui->scrollAreaWidgetContents_2->size();
    // });
};

Widget::~Widget()
{
    delete ui;
}

void Widget::on_lineEdit_translate_returnPressed() {
    QString query = ui->lineEdit_translate->text();
    resultsDisplay(query);
}

void Widget::createVocabPage(int row_s, QString pos)
{

    QScrollArea *scrollArea = ui->vocabScrollArea;
    QWidget *oldContainer = scrollArea->takeWidget(); // 取出旧容器（不删除）
    delete oldContainer; // 删除旧容器及其所有子控件

    // 创建新的容器 widget
    QWidget *container = new QWidget;
    QGridLayout *gridLayout = new QGridLayout(container);
    container->setLayout(gridLayout);

    for (int i = 0; i <= 98; i++) {
        QString strI = QString::number(i);
        QVector<SearchResult> result = m_dictionary.smartSearch(strI, 1);
        if (!result.empty()) {
            const DictionaryEntry* entry = result[0].entry;
            QString word = entry->word;
            QString word_cn = entry->word_cn;
            QString partOfSpeech = entry->partOfSpeech;
            QString text = word + " " + partOfSpeech + "\n" + word_cn;
            QLabel *label = new QLabel(text);
            if ((!(partOfSpeech == pos or pos != "all")) or partOfSpeech.contains(pos)) {
                QString styleSheet = QString(
                                         "border-radius: 5px;"
                                         "padding: 5px;"
                                         "background-color: %1;"
                                         "border: 1px solid %2;"
                                         ).arg(vocab_label_bg_suitable, vocab_label_border_suitable);
                label->setStyleSheet(styleSheet);
            } else if (!partOfSpeech.contains(pos)){
                QString styleSheet = QString("border: 0px solid #000000;"
                                             "border-radius: 5px;"
                                             "padding: 5px;"
                                             "background-color: %1;").arg(vocab_label_background_normal);
                label->setStyleSheet(styleSheet);
            }

            label->setAlignment(Qt::AlignCenter);


            // 计算行列（固定行数）
            int col = (i-1) % row_s;
            int row = int((i-1) / row_s);

            gridLayout->addWidget(label, row, col);
        }
    }
    // 将新容器设置给 scrollArea
    scrollArea->setWidget(container);
}

void Widget::on_btn2trans_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}


void Widget::on_btn2vocab_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
    if (!ifcreated) {
        createVocabPage(4, "all");
    }
}


void Widget::on_btn2settings_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
}


void Widget::onColorSchemeChanged(Qt::ColorScheme scheme) {
    updateColors(scheme);
    updateColorStrings(scheme);
}
void Widget::updateColors(Qt::ColorScheme scheme) {
    if (scheme == Qt::ColorScheme::Dark) { // 深色模式
        // ui->label_result->setStyleSheet("QLabel{background-color: #282828; border-radius: 9px; color: #f3f3f3}");
    } else if (scheme == Qt::ColorScheme::Light) { // 浅色模式
        // ui->label_result->setStyleSheet("QLabel{background-color: #F8F8F8; border-radius: 9px; color: #1A1A1A}");
    }
}

void Widget::updateColorStrings(Qt::ColorScheme scheme) {
    if (scheme == Qt::ColorScheme::Dark) {
        vocab_label_background_normal = "#2e2e2e";
        vocab_label_border_suitable = "#27FF97";
        vocab_label_bg_suitable = "#2a2a2a";
        stringColor = "#F4F4F4";
        background_color = "#1e1e1e";
    } else if (scheme == Qt::ColorScheme::Light) {
        vocab_label_background_normal = "#EBEBEB";
        vocab_label_bg_suitable = "#f3f3f3";
        vocab_label_border_suitable = "#27FF97";
        stringColor = "#1A1A1A";
        background_color = "";
    }
}

void Widget::on_btn_ko_clicked()
{
    createVocabPage(4, "ko");
}


void Widget::on_btn_ka_clicked()
{
    createVocabPage(4, "ka");
}


void Widget::on_btn_kiu_clicked()
{
    createVocabPage(4, "kiu");
}


void Widget::on_btn_koler_clicked()
{
    createVocabPage(4, "koler");
}


void Widget::on_btn_kei_clicked()
{
    createVocabPage(4, "kei");
}

void Widget::on_btn_ko_2_clicked()
{
    createVocabPage(4, "all");
}

void Widget::resultsDisplay(QString& query){
    if (!m_labels.isEmpty()) {
        qDeleteAll(m_labels);   // 删除所有标签
        m_labels.clear();
    }

    // 检查词典是否正确加载
    if (!m_dictLoaded){
        //qDebug() << "词典未加载,无法搜索";
        ui->label_result->setText("        词典未加载，请检查文件");
        return;
    }

    // 判断是否为按索引搜索
    if (m_dictionary.isInteger(query)) {
        query = query.trimmed();
    }
    //qDebug() << "part>>>1";
    if (m_dictionary.isSentence(query)){
        qDebug() << "可以";
        QString str = "翻译：" + m_dictionary.translateSentence(query);
        ui->label_result->setText(str);
        qDebug() << "正确显示了";
    }
    else {
        QVector<SearchResult> results = m_dictionary.smartSearch(query, 10);
        int resultsCount = results.count();
        QString trans = "";
        QString pos = "";
        QString phrase = "";
        QString phrase_Trans = "";
        QString exampleSentence = "";
        QString exampleSentenceTrans = "";
        QVector<QString> sentenceAndTrans;
        QString extra = "<font color = \"#27FF97\">拼写没有问题</font>";
        QString exactWord = "";
        QString strToDisplay;

        if (resultsCount == 0) { // 没有结果时：
            qDebug() << "if(resultsCount == 0) begins";
            if (m_dictionary.isMainlyChinese(query)){
                if (query != exactWord) {
                    extra = "<font color = \"#FF6A5A\">无结果,拼写可能有问题！</font>";
                }
            }
            strToDisplay = QString("正确词汇：<br>最佳翻译：<br>词性：<br>相关词组：<br>翻译：<br>例句：<br>例句翻译：<b"
                                   "r>%1").arg(extra);
            // qDebug() << strToDisplay;
            ui->label_result->setText(strToDisplay);

            return;
        } else {

            int labelHeight = 291;
            int spacing = 19;
            int totalHeight = resultsCount * (labelHeight + spacing) - spacing;

            ui->scrollAreaWidgetContents_2->resize(467, totalHeight);
            for(int i = 0; i < resultsCount; i++) {
                label = new QLabel;
                const SearchResult& sr = results[i];
                if (m_dictionary.isMainlyChinese(query)) { // 判断是否为中文，是：if 否：else
                    trans = sr.entry->word; // 设置翻译
                    exactWord = sr.entry->word_cn; // 设置具体词
                } else {
                    trans = sr.entry->word_cn; // 设置翻译
                    exactWord = sr.entry->word; // 设置具体词
                }
                if (!m_dictionary.isMainlyChinese(query)){
                    if (query != exactWord && !m_dictionary.isInteger(query)) {
                        extra = QString("<font color = \"#FF6A5A\">┏ (゜ω゜)=👉%1应该是%2</font>").arg(query, exactWord);
                    }
                }
                sentenceAndTrans = m_dictionary.createExSentence(sr.entry->word, sr.entry->word_cn, sr.entry->partOfSpeech);
                // 句子以及句子翻译

                exampleSentence = sentenceAndTrans[0];
                exampleSentenceTrans = sentenceAndTrans[1];

                pos = sr.entry->partOfSpeech;

                if (sr.hasPhrases()) {
                    phrase = sr.entry->phrases[i];
                    phrase_Trans = sr.entry->phrases_cn[i];
                }

                strToDisplay = QString("正确词汇：%1<br>"
                                       "最佳翻译：%2<br>"
                                       "词性：%3<br>"
                                       "相关词组：%4<br>"
                                       "翻译：%5<br>"
                                       "例句：%6<br>"
                                       "%7<br>"
                                       "%8").arg(exactWord,
                                        trans,
                                        pos,
                                        phrase,
                                        phrase_Trans,
                                        exampleSentence,
                                        exampleSentenceTrans,
                                        extra);
                //qDebug() << strToDisplay;
                if (i == 0) {
                    ui->label_result->setText(strToDisplay); // 设置结果
                }
                else
                {
                    label_result_i = new QLabel("", ui->scrollAreaWidgetContents_2);
                    label_result_i->move(0, i*310);
                    label_result_i->setFixedSize(455, 291);
                    label_result_i->setText(strToDisplay);
                    qDebug() << "daowole";
                    label_result_i->setFont(font);

                    label_result_i->setStyleSheet( "QLabel{"
                                                  "border: 1px solid #27FF97;"
                                                  "border-radius: 15px;"
                                                  "padding-left: 12px;"
                                                  "background: rgba(0, 0, 0, 15)"
                                                  "}");
                    label_result_i->show();
                    m_labels.append(label_result_i);
                }
            }
            return;
        }
    }
}

void Widget::on_btn_kiu_2_clicked()
{
    createVocabPage(4, "koos.");
}

