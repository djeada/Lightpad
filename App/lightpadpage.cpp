#include "lightpadpage.h"
#include "mainwindow.h"
#include <QHBoxLayout>
#include <QDebug>
#include <QMenu>

LightpadTreeView::LightpadTreeView(LightpadPage* parent):
    QTreeView(parent),
    parentPage(parent) {

}

void LightpadTreeView::mouseReleaseEvent(QMouseEvent *e) {

    if (e->button() == Qt::RightButton) {
        QModelIndex idx = indexAt(e->pos());
        if (idx.isValid()) {
            QMenu m;
            m.addAction("Remove");

            QAction *selected = m.exec(mapToGlobal(e->pos()));

            if (selected) {
                if (selected->text() == "Remove")
                    removeFile(parentPage->getFilePath(idx));
            }
        }
    }

    else
        QTreeView::mouseReleaseEvent(e);
}

void LightpadTreeView::removeFile(QString filePath)
{
    if (QFileInfo(filePath).isFile()) {
        QFile::remove(filePath);
        parentPage->updateModel();
        parentPage->setModelRootIndex(QFileInfo(filePath).absoluteDir().path());
    }
}


LightpadPage::LightpadPage(QWidget* parent, bool treeViewHidden) :
    QWidget(parent),
    mainWindow(nullptr),
    filePath("") {

        auto* layoutHor = new QHBoxLayout(this);

        treeView = new LightpadTreeView(this);
        textArea = new TextArea(this);

        layoutHor->addWidget(treeView);
        layoutHor->addWidget(textArea);

        if (treeViewHidden)
            treeView->hide();

        layoutHor->setContentsMargins(0, 0, 0, 0);
        layoutHor->setStretch(0, 0);
        layoutHor->setStretch(1, 1);

        setLayout(layoutHor);
        updateModel();

        QObject::connect(treeView, &QAbstractItemView::clicked, this, [&] (const QModelIndex &index) {
            if (mainWindow) {
                QString path = model->filePath(index);
                mainWindow->openFileAndAddToNewTab(path);
                treeView->clearSelection();
                treeView->setCurrentIndex(index);
            }
        });
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
}

void LightpadPage::setFilePath(QString path)
{
    filePath = path;
}

void LightpadPage::updateModel()
{
    model = new QFileSystemModel(this);
    model->setRootPath(QDir::home().path());
    treeView->setModel(model);

    treeView->setColumnHidden(1, true);
    treeView->setColumnHidden(2, true);
    treeView->setColumnHidden(3, true);
}

QString LightpadPage::getFilePath()
{
    return filePath;
}

QString LightpadPage::getFilePath(const QModelIndex &index)
{
    return model->filePath(index);
};
