#include "lightpadpage.h"
#include <QHBoxLayout>

LightpadPage::LightpadPage(bool listViewHidden, QWidget* parent) :
    QWidget(parent) {

        auto* layoutHor = new QHBoxLayout();

        listView = new QListView();
        textArea = new TextArea();

        layoutHor->addWidget(listView);
        layoutHor->addWidget(textArea);

        if (listViewHidden)
            listView->hide();

        layoutHor->setContentsMargins(0, 0, 0, 0);
        layoutHor->setStretch(0, 1);
        setLayout(layoutHor);
}

TextArea *LightpadPage::getTextArea()
{
    return textArea;
};
