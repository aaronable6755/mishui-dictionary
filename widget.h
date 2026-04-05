#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <qlabel.h>
#include "ConlangDictionary.h"
#include <QGridLayout>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    void createVocabPage(int row_s, QString pos);

    void resultsDisplay(QString& query);
private slots:
    void on_lineEdit_translate_returnPressed();

    void on_btn2trans_clicked();

    void on_btn2vocab_clicked();

    void on_btn2settings_clicked();

    void onColorSchemeChanged(Qt::ColorScheme scheme);

    void updateColors(Qt::ColorScheme scheme);

    void updateColorStrings(Qt::ColorScheme scheme);

    void on_btn_ko_clicked();

    void on_btn_ka_clicked();

    void on_btn_kiu_clicked();

    void on_btn_koler_clicked();

    void on_btn_kei_clicked();

    void on_btn_ko_2_clicked();

    void on_btn_kiu_2_clicked();

private:
    Ui::Widget *ui;
    ConlangDictionary m_dictionary;  // 词典作为成员变量
    bool m_dictLoaded;               // 加载状态标志

    void performSearch();  // 可选的搜索函数

    QGridLayout *gridlayout;

    bool ifcreated = false;

    QString vocab_label_background_normal;
    QString vocab_label_border_normal;
    QString vocab_label_border_suitable;
    QString vocab_label_bg_suitable;
    QString stringColor;
    QString color_true;
    QString color_warn;
    QString background_color;

    QLabel* label;

    QWidget* widget_container_scrollArea;

    QLabel* label_result_i;
    bool translate_returned_clicked = false;

    QList<QLabel*> m_labels;
    QFont font;
};

#endif // WIDGET_H
