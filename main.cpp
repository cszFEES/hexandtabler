#include <QApplication>
#include "hexandtabler.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    hexandtabler w;
    w.show();
    return a.exec();
}