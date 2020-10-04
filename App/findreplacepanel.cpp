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
    updateCounterLabels();
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
        QTextCursor newCursor(textArea->document());

        if (textArea->getSearchWord() != searchWord)
           findInitial(newCursor, searchWord);

        else
            findNext(newCursor, searchWord);

        updateCounterLabels();
    }
}

void FindReplacePanel::on_replaceSingle_clicked()
{
    if (textArea) {

        textArea->setFocus();
        QString searchWord = ui->searchFind->text();
        QString replaceWord = ui->fieldReplace->text();
        QTextCursor newCursor(textArea->document());

        findInitial(newCursor, searchWord);
        replaceNext(newCursor, replaceWord);
        updateCounterLabels();
    }
}

void FindReplacePanel::on_close_clicked()
{
    if (textArea)
        textArea->updateSyntaxHighlightTags();

    close();
}

void FindReplacePanel::selectSearchWord(QTextCursor& cursor, int n, int offset)
{
    cursor.setPosition(positions[++position] - offset);

   if (!cursor.isNull()) {
       cursor.clearSelection();
       cursor.setPosition(positions[position] - offset + n, QTextCursor::KeepAnchor);
       prevFormat = cursor.charFormat();
       cursor.setCharFormat(colorFormat);
       textArea->setTextCursor(cursor);
   }
}

void FindReplacePanel::clearSelectionFormat(QTextCursor& cursor, int n)
{
    if (!positions.isEmpty() && position >= 0) {
        cursor.setPosition(positions[position]);

        if (!cursor.isNull()) {
            cursor.setPosition(positions[position] + n, QTextCursor::KeepAnchor);
            cursor.setCharFormat(prevFormat);
            textArea->setTextCursor(cursor);
        }
    }
}

void FindReplacePanel::replaceNext(QTextCursor &cursor, const QString &replaceWord)
{
    if (!cursor.selectedText().isEmpty() && !positions.isEmpty()) {
        cursor.removeSelectedText();
        cursor.insertText(replaceWord);
        textArea->setTextCursor(cursor);
    }
}

void FindReplacePanel::updateCounterLabels()
{
    if (positions.isEmpty()) {
        ui->currentIndex->hide();
        ui->totalFound->hide();
        ui->label->hide();
    }

    else {
        if (ui->currentIndex->isHidden()) {
            ui->currentIndex->show();
            ui->totalFound->show();
            ui->label->show();
        }

        ui->currentIndex->setText(QString::number(position + 1));
        ui->totalFound->setText(QString::number(positions.size()));
    }
}

void FindReplacePanel::findInitial(QTextCursor& cursor, const QString& searchWord)
{
    if (!positions.isEmpty()) {
        clearSelectionFormat(cursor, searchWord.size());
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
        selectSearchWord(cursor, searchWord.size());
    }
}

void FindReplacePanel::findNext(QTextCursor& cursor, const QString& searchWord, int offset)
{
    clearSelectionFormat(cursor, searchWord.size());

    if (!positions.isEmpty()) {
        if (position >= positions.size() - 1)
            position = -1;

        selectSearchWord(cursor, searchWord.size(), offset);
    }
}



void FindReplacePanel::on_replaceAll_clicked()
{

    if (textArea) {

        textArea->setFocus();
        QString searchWord = ui->searchFind->text();
        QString replaceWord = ui->fieldReplace->text();
        QTextCursor newCursor(textArea->document());

        position = -1;
        positions.clear();

        int j = 0;

        while ((j = textArea->toPlainText().indexOf(searchWord, j)) != -1) {
            positions.push_back(j);
            ++j;
        }

        int offset = 0;

        for (int i = 0; i < positions.size(); i++) {
            findNext(newCursor, replaceWord, offset);
            replaceNext(newCursor, replaceWord);
            offset += (replaceWord.size() - searchWord.size());
        }

        position = -1;
        positions.clear();
        updateCounterLabels();
    }

}
