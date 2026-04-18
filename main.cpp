#include "widget.h"
#include <cctype>
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);
    Widget w;
    w.setWindowTitle("米水大词典");
    w.show();
    return a.exec();
};
