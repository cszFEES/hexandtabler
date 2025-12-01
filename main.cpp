#include <QApplication>
#include "hexandtabler.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    hexandtabler hexEditor; 
    hexEditor.show();
    return app.exec();
}