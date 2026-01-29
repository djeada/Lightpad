#include <QApplication>

#include "mainwindow.h"

int main(int argv, char** args)
{
    QApplication app(argv, args);
    app.setApplicationName("Lightpad");
    app.setOrganizationName("Lightpad");
    app.setWindowIcon(QIcon(":/resources/icons/app.png"));

    MainWindow w;

    return app.exec();
}
