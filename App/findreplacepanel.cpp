#include "findreplacepanel.h"
#include "ui_findreplacepanel.h"
#include "textarea.h"

#include <QVBoxLayout>
#include <QDebug>
#include <QTextDocument>

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget *parent) :
    QWidget(parent),
    document(nullptr),
    textArea(nullptr),
    ui(new Ui::FindReplacePanel),
    onlyFind(onlyFind)
{
    ui->setupUi(this);

    show();

    ui->options->setVisible(false);
    setReplaceVisibility(onlyFind);

    colorFormat.setBackground(Qt::red);
    colorFormat.setForeground(Qt::white);

    ui->find->setFixedSize(ui->more->width(), ui->find->height());
}

FindReplacePanel::~FindReplacePanel() {
    delete ui;
}

void FindReplacePanel::setReplaceVisibility(bool flag)
{
    ui->widget->setVisible(flag);
    ui->replaceSingle->setVisible(flag);
    ui->replaceAll->setVisible(flag);
}

bool FindReplacePanel::isOnlyFind()
{
    return onlyFind;
}

void FindReplacePanel::setOnlyFind(bool flag)
{
    onlyFind = flag;
}

void FindReplacePanel::setDocument(QTextDocument *doc)
{
    document = doc;
}

void FindReplacePanel::setTextArea(TextArea *area)
{
    textArea = area;
}

void FindReplacePanel::setFocusOnSearchBox()
{
    ui->searchFind->setFocus();
}

void FindReplacePanel::on_more_clicked()
{
    ui->options->setVisible(!ui->wholeWords->isVisible());
}

void FindReplacePanel::on_find_clicked()
{
    if (textArea) {

        textArea->setFocus();

        QString searchWord = ui->searchFind->text();

        if (textArea->getSearchWord() != searchWord) {
            QTextCursor newCursor(textArea->document());

            if (!positions.isEmpty()) {
                clearSelectionFormat(newCursor, searchWord.size());
                positions.clear();
            }

            textArea->updateSyntaxHighlightTags(searchWord);


            int j = 0;

            while ((j = textArea->toPlainText().indexOf(searchWord, j)) != -1) {
                positions.push_back(j);
                ++j;
            }

            if (!positions.isEmpty()) {
                position = -1;

                selectSearchWord(newCursor, searchWord.size());
            }

            ui->currentIndex->setText(QString::number(position + 1));
            ui->totalFound->setText(QString::number(positions.size()));

        }


        else {

            QTextCursor newCursor(textArea->document());

            clearSelectionFormat(newCursor, searchWord.size());

           if (position >= positions.size() - 1)
                 position = -1;

            selectSearchWord(newCursor, searchWord.size());
            ui->currentIndex->setText(QString::number(position + 1));
        }
    }
}

void FindReplacePanel::on_close_clicked()
{
    textArea->updateSyntaxHighlightTags();
    close();
}

void FindReplacePanel::selectSearchWord(QTextCursor cursor, int n)
{
    cursor.setPosition(positions[++position]);

   if (!cursor.isNull()) {
       cursor.setPosition(positions[position] + n, QTextCursor::KeepAnchor);
       prevFormat = cursor.charFormat();
       cursor.setCharFormat(colorFormat);
       textArea->setTextCursor(cursor);
   }
}

void FindReplacePanel::clearSelectionFormat(QTextCursor cursor, int n)
{
    cursor.setPosition(positions[position]);

    if (!cursor.isNull()) {
        cursor.setPosition(positions[position] + n, QTextCursor::KeepAnchor);
        cursor.setCharFormat(prevFormat);
        textArea->setTextCursor(cursor);
    }
}
