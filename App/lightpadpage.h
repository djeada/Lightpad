#ifndef LIGHTPADPAGE_H
#define LIGHTPADPAGE_H

#include "textarea.h"

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>

class LightpadPage;

class LightpadTreeView: public QTreeView {

    Q_OBJECT

    public:
        LightpadTreeView(LightpadPage* parent = nullptr);
        ~LightpadTreeView() {};
        void renameFile(QString oldFilePath, QString newFilePath);

    protected:
        void mouseReleaseEvent(QMouseEvent *e) override;

    private:
        LightpadPage* parentPage;
        void duplicateFile(QString filePath);
        void removeFile(QString filePath);
};

class LightpadPage: public QWidget {

    Q_OBJECT

    public:
        LightpadPage(QWidget* parent = nullptr, bool treeViewHidden = true);
        ~LightpadPage() {};
        QTreeView* getTreeView();
        TextArea* getTextArea();
        void setTreeViewVisible(bool flag);
        void setModelRootIndex(QString path);
        void setMainWindow(MainWindow* window);
        void setFilePath(QString path);
        void closeTabPage(QString path);
        void updateModel();
        QString getFilePath();
        QString getFilePath(const QModelIndex &index);
        bool scriptAssigned();

    private:
        MainWindow* mainWindow;
        QTreeView* treeView;
        TextArea* textArea;
        QFileSystemModel* model;
        QString filePath;
        QString scriptPath;

};

#endif // LIGHTPADPAGE_H
