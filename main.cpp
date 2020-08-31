#include <QApplication>

#include "mainwindow.h"

int main(int argv, char **args)
{
    QApplication app(argv, args);
    MainWindow w;

    return app.exec();
}

