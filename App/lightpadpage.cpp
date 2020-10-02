#include "lightpadpage.h"
#include "mainwindow.h"
#include <QHBoxLayout>
#include <QDebug>
#include <QMenu>
#include <QLineEdit>

class LineEdit : public QLineEdit {

    using QLineEdit::QLineEdit;

    public:
        LineEdit (QRect rect, QString filePath, QWidget* parent) :
            QLineEdit(parent),
            oldFilePath(filePath) {
            show();
            setGeometry(QRect(rect.x(), rect.y() + rect.height() + 1, 1.1*rect.width(), 1.1*rect.height()));
            setFocus(Qt::MouseFocusReason);
        }

    protected:
        void focusOutEvent(QFocusEvent *event) override {
            Q_UNUSED(event);

            LightpadTreeView* treeView = qobject_cast<LightpadTreeView*>(parentWidget());

            if (treeView)
                treeView->renameFile(oldFilePath, QFileInfo(oldFilePath).absoluteDir().path() + QDir::separator() + text());

            close();
        }

        void keyPressEvent(QKeyEvent *event) override {

            if( (event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return)) {
                renameTreeViewEntry();
                close();
            }

            else if(event->key() == Qt::Key_Escape)
                close();

            else
                QLineEdit::keyPressEvent(event);
        }

    private:
        QString oldFilePath;

        void renameTreeViewEntry() {
            LightpadTreeView* treeView = qobject_cast<LightpadTreeView*>(parentWidget());

            if (treeView)
                treeView->renameFile(oldFilePath, QFileInfo(oldFilePath).absoluteDir().path() + QDir::separator() + text());
        }
};

QString addUniqueSuffix(const QString &fileName) {
    if (!QFile::exists(fileName))
        return fileName;

    QFileInfo fileInfo(fileName);
    QString ret;

    QString secondPart = fileInfo.completeSuffix();
    QString firstPart;
    if (!secondPart.isEmpty()) {
        secondPart = "." + secondPart;
        firstPart = fileName.left(fileName.size() - secondPart.size());
    } else
        firstPart = fileName;

    for (int ii = 1; ; ii++) {
        ret = QString("%1 (%2)%3").arg(firstPart).arg(ii).arg(secondPart);
        if (!QFile::exists(ret)) {
            return ret;
        }
    }
}

LightpadTreeView::LightpadTreeView(LightpadPage* parent):
    QTreeView(parent),
    parentPage(parent) {

}

void LightpadTreeView::mouseReleaseEvent(QMouseEvent *e) {

    if (e->button() == Qt::RightButton) {
        QModelIndex idx = indexAt(e->pos());
        if (idx.isValid()) {
            QMenu m;
            m.addAction("Duplicate");
            m.addAction("Rename");
            m.addAction("Remove");

            QAction *selected = m.exec(mapToGlobal(e->pos()));

            if (selected) {

                QString filePath = parentPage->getFilePath(idx);

                if (selected->text() == "Duplicate")
                    duplicateFile(filePath);

                else if (selected->text() == "Rename")
                    new LineEdit(visualRect(idx), filePath, this);

                else if (selected->text() == "Remove")
                    removeFile(filePath);
            }
        }
    }

    else
        QTreeView::mouseReleaseEvent(e);
}

void LightpadTreeView::renameFile(QString oldFilePath, QString newFilePath)
{
    if (QFileInfo(oldFilePath).isFile()) {
        QFile(oldFilePath).rename(newFilePath);
        parentPage->updateModel();
        parentPage->setModelRootIndex(QFileInfo(newFilePath).absoluteDir().path());
    }
}

void LightpadTreeView::duplicateFile(QString filePath)
{
    if (QFileInfo(filePath).isFile()) {
        QFile(filePath).copy(addUniqueSuffix(filePath));
        parentPage->updateModel();
        parentPage->setModelRootIndex(QFileInfo(filePath).absoluteDir().path());
    }
}


void LightpadTreeView::removeFile(QString filePath)
{
    if (QFileInfo(filePath).isFile()) {
        QFile::remove(filePath);
        parentPage->updateModel();
        parentPage->setModelRootIndex(QFileInfo(filePath).absoluteDir().path());
        parentPage->closeTabPage(filePath);
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

void LightpadPage::setMainWindow(MainWindow *window) {

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

    if (!path.isEmpty()) {
        setTreeViewVisible(true);
        setModelRootIndex(QFileInfo(filePath).absoluteDir().path());
    }
}

void LightpadPage::closeTabPage(QString path)
{
    if (mainWindow)
        mainWindow->closeTabPage(path);
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
