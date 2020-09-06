#include "lightpadpage.h"
#include "mainwindow.h"
#include <QHBoxLayout>
#include <QDebug>

LightpadPage::LightpadPage(QWidget* parent, bool treeViewHidden) :
    QWidget(parent) {

        auto* layoutHor = new QHBoxLayout();

        treeView = new QTreeView();
        textArea = new TextArea();

        layoutHor->addWidget(treeView);
        layoutHor->addWidget(textArea);

        if (treeViewHidden)
            treeView->hide();

        layoutHor->setContentsMargins(0, 0, 0, 0);
        layoutHor->setStretch(0, 0);
        layoutHor->setStretch(1, 1);

        setLayout(layoutHor);


        model = new QFileSystemModel(this);
        model->setRootPath(QDir::home().path());
        treeView->setModel(model);

        treeView->setColumnHidden(1, true);
        treeView->setColumnHidden(2, true);
        treeView->setColumnHidden(3, true);

}

QTreeView *LightpadPage::getTreeView()
{
    return treeView;
}

TextArea *LightpadPage::getTextArea()
{
    return textArea;
}

void LightpadPage::setTreeViewVisible(bool flag)
{
    treeView->setVisible(flag);
}

void LightpadPage::setModelRootIndex(QString path)
{
    treeView->setRootIndex(model->index(path));

}

void LightpadPage::setMainWindow(MainWindow *window)
{
 mainWindow = window;

 if (textArea) {
     textArea->setMainWindow(mainWindow);
     textArea->setFontSize(mainWindow->getFontSize());
     textArea->setTabWidth(mainWindow->getTabWidth());

 }
};
