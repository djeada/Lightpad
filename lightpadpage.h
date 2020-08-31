#ifndef LIGHTPADPAGE_H
#define LIGHTPADPAGE_H

#include "textarea.h"

#include <QWidget>
#include <QListView>

class LightpadPage: public QWidget {

    Q_OBJECT

    public:
        LightpadPage(bool listViewHidden = true, QWidget* parent = nullptr);
        virtual ~LightpadPage() {};
        QListView* getListView();
        TextArea* getTextArea();

    private:
       QListView* listView;
       TextArea* textArea;

};

#endif // LIGHTPADPAGE_H
