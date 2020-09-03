#include "findreplacepanel.h"
#include "ui_findreplacepanel.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QTextDocument>

//HighlightingRule::HighlightingRule(QRegularExpression pattern, QTextCharFormat format) :

 KeyWordsHighlighter::KeyWordsHighlighter(QString key, QTextDocument* parent):
    QSyntaxHighlighter(parent)
{
   format.setBackground(QColor("#646464"));
   pattern = QRegularExpression(key);

}


void KeyWordsHighlighter::highlightBlock(const QString &text) {

    QRegularExpressionMatchIterator matchIterator = pattern.globalMatch(text);

    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), format);
    }

    setCurrentBlockState(0);
}

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget *parent) :
    QWidget(parent),
    document(nullptr),
    ui(new Ui::FindReplacePanel),
    onlyFind(onlyFind)
{
    ui->setupUi(this);

    show();

    ui->options->setVisible(false);
    setReplaceVisibility(onlyFind);

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

void FindReplacePanel::on_more_clicked() {
    ui->options->setVisible(!ui->wholeWords->isVisible());
}

void FindReplacePanel::on_find_clicked()
{
    if (document) {
        KeyWordsHighlighter* highlighter = new KeyWordsHighlighter(ui->searchFind->text(), document);


    }
}

void FindReplacePanel::on_close_clicked()
{
    close();
}
