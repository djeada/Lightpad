#ifndef LIGHTPADPAGE_H
#define LIGHTPADPAGE_H

#include "textarea.h"

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>

class LightpadPage: public QWidget {

    Q_OBJECT

    public:
        LightpadPage(bool treeViewHidden = true, QWidget* parent = nullptr);
        virtual ~LightpadPage() {};
        QTreeView* getTreeView();
        TextArea* getTextArea();
        void setTreeViewVisible(bool flag);
        void setModelRootIndex(QString path);

    private:
       QTreeView* treeView;
       TextArea* textArea;
       QFileSystemModel* model;

};

#endif // LIGHTPADPAGE_H
