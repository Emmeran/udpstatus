#include <QtGui/QApplication>
#include "display.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    mDisplay w;
    w.show();

    return a.exec();
}
