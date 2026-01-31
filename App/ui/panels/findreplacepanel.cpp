#include "findreplacepanel.h"
#include "../../core/textarea.h"
#include "ui_findreplacepanel.h"

#include <QDebug>
#include <QTextDocument>
#include <QVBoxLayout>

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget* parent)
    : QWidget(parent)
    , document(nullptr)
    , textArea(nullptr)
    , ui(new Ui::FindReplacePanel)
    , onlyFind(onlyFind)
    , position(-1)
    , searchHistoryIndex(-1)
{
    ui->setupUi(this);

    show();

    ui->options->setVisible(false);
    setReplaceVisibility(onlyFind);

    colorFormat.setBackground(Qt::red);
    colorFormat.setForeground(Qt::white);

    updateCounterLabels();
}

FindReplacePanel::~FindReplacePanel()
{
    delete ui;
}

void FindReplacePanel::setReplaceVisibility(bool flag)
{
    ui->widget->setVisible(flag);
    ui->replaceSingle->setVisible(flag);
    ui->replaceAll->setVisible(flag);
    // Hide preserve case when replace is not visible
    ui->preserveCase->setVisible(flag);
}

bool FindReplacePanel::isOnlyFind()
{
    return onlyFind;
}

void FindReplacePanel::setOnlyFind(bool flag)
{
    onlyFind = flag;
}

void FindReplacePanel::setDocument(QTextDocument* doc)
{
    document = doc;
}

void FindReplacePanel::setTextArea(TextArea* area)
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

QRegularExpression FindReplacePanel::buildSearchPattern(const QString& searchWord) const
{
    QString pattern = searchWord;
    
    // Escape regex special characters unless using regex mode
    if (!ui->useRegex->isChecked()) {
        pattern = QRegularExpression::escape(pattern);
    }
    
    // Add whole word boundaries if required
    if (ui->wholeWords->isChecked()) {
        pattern = "\\b" + pattern + "\\b";
    }
    
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    
    // Case sensitivity
    if (!ui->matchCase->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    
    return QRegularExpression(pattern, options);
}

QString FindReplacePanel::applyPreserveCase(const QString& replaceWord, const QString& matchedText) const
{
    if (!ui->preserveCase->isChecked() || matchedText.isEmpty()) {
        return replaceWord;
    }
    
    QString result = replaceWord;
    
    // Check if matched text is all uppercase
    bool allUpper = true;
    bool allLower = true;
    bool firstUpper = false;
    
    for (int i = 0; i < matchedText.length(); ++i) {
        QChar c = matchedText.at(i);
        if (c.isLetter()) {
            if (c.isUpper()) {
                allLower = false;
                if (i == 0) firstUpper = true;
            } else {
                allUpper = false;
            }
        }
    }
    
    if (allUpper && !allLower) {
        // Matched text is all uppercase, make replacement all uppercase
        return result.toUpper();
    } else if (allLower && !allUpper) {
        // Matched text is all lowercase, make replacement all lowercase
        return result.toLower();
    } else if (firstUpper && matchedText.length() > 1) {
        // Title case: first letter uppercase, rest lowercase
        result = result.toLower();
        if (!result.isEmpty()) {
            result[0] = result[0].toUpper();
        }
        return result;
    }
    
    return replaceWord;
}

void FindReplacePanel::addToSearchHistory(const QString& searchTerm)
{
    if (searchTerm.isEmpty()) {
        return;
    }
    
    // Remove if already exists (to move to front)
    searchHistory.removeAll(searchTerm);
    
    // Add to front
    searchHistory.prepend(searchTerm);
    
    // Limit size
    while (searchHistory.size() > MAX_SEARCH_HISTORY) {
        searchHistory.removeLast();
    }
    
    searchHistoryIndex = -1;
}

void FindReplacePanel::on_find_clicked()
{
    if (textArea) {
        textArea->setFocus();
        QString searchWord = ui->searchFind->text();
        
        if (searchWord.isEmpty()) {
            return;
        }
        
        addToSearchHistory(searchWord);
        QTextCursor newCursor(textArea->document());

        if (textArea->getSearchWord() != searchWord) {
            findInitial(newCursor, searchWord);
        } else {
            findNext(newCursor, searchWord);
        }

        updateCounterLabels();
    }
}

void FindReplacePanel::on_findPrevious_clicked()
{
    if (textArea) {
        textArea->setFocus();
        QString searchWord = ui->searchFind->text();
        
        if (searchWord.isEmpty()) {
            return;
        }
        
        addToSearchHistory(searchWord);
        QTextCursor newCursor(textArea->document());

        if (textArea->getSearchWord() != searchWord) {
            findInitial(newCursor, searchWord);
        } else {
            findPrevious(newCursor, searchWord);
        }

        updateCounterLabels();
    }
}

void FindReplacePanel::on_replaceSingle_clicked()
{
    if (textArea) {
        textArea->setFocus();
        QString searchWord = ui->searchFind->text();
        QString replaceWord = ui->fieldReplace->text();
        
        if (searchWord.isEmpty()) {
            return;
        }
        
        addToSearchHistory(searchWord);
        QTextCursor newCursor(textArea->document());

        if (textArea->getSearchWord() != searchWord) {
            findInitial(newCursor, searchWord);
        }
        
        replaceNext(newCursor, replaceWord);
        
        // Find next occurrence after replacement
        if (!positions.isEmpty()) {
            findNext(newCursor, searchWord);
        }
        
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

void FindReplacePanel::replaceNext(QTextCursor& cursor, const QString& replaceWord)
{
    if (!cursor.selectedText().isEmpty() && !positions.isEmpty() && position >= 0) {
        QString matchedText = cursor.selectedText();
        QString finalReplacement = applyPreserveCase(replaceWord, matchedText);
        
        cursor.removeSelectedText();
        cursor.insertText(finalReplacement);
        textArea->setTextCursor(cursor);
        
        // Remove current position and adjust remaining positions
        if (position < positions.size()) {
            int oldLength = matchedText.length();
            int newLength = finalReplacement.length();
            int lengthDiff = newLength - oldLength;
            
            positions.removeAt(position);
            
            // Adjust positions of subsequent matches
            for (int i = position; i < positions.size(); ++i) {
                positions[i] += lengthDiff;
            }
            
            // Adjust position index
            if (position > 0) {
                position--;
            } else if (positions.isEmpty()) {
                position = -1;
            } else {
                position = -1;  // Will be incremented on next find
            }
        }
    }
}

void FindReplacePanel::updateCounterLabels()
{
    if (positions.isEmpty()) {
        ui->currentIndex->hide();
        ui->totalFound->hide();
        ui->label->hide();
    } else {
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

    QRegularExpression pattern = buildSearchPattern(searchWord);
    QString text = textArea->toPlainText();
    
    // Handle search from start option
    int startPos = 0;
    if (!ui->searchStart->isChecked() && textArea) {
        startPos = textArea->textCursor().position();
    }
    
    QRegularExpressionMatchIterator matches = pattern.globalMatch(text);
    QVector<int> allPositions;
    QVector<int> matchLengths;
    
    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        allPositions.push_back(match.capturedStart());
        matchLengths.push_back(match.capturedLength());
    }
    
    if (ui->searchBackward->isChecked()) {
        // Reverse the order for backward search
        std::reverse(allPositions.begin(), allPositions.end());
    }
    
    positions = allPositions;

    if (!positions.isEmpty()) {
        position = -1;
        
        // For search from current position, find the nearest match
        if (!ui->searchStart->isChecked() && startPos > 0) {
            for (int i = 0; i < positions.size(); ++i) {
                if (ui->searchBackward->isChecked()) {
                    if (positions[i] < startPos) {
                        position = i - 1;
                        break;
                    }
                } else {
                    if (positions[i] >= startPos) {
                        position = i - 1;
                        break;
                    }
                }
            }
        }
        
        // Get match length for highlighting
        int matchLength = searchWord.size();
        if (!allPositions.isEmpty() && !matchLengths.isEmpty()) {
            matchLength = matchLengths.first();
        }
        
        selectSearchWord(cursor, matchLength);
    }
}

void FindReplacePanel::findNext(QTextCursor& cursor, const QString& searchWord, int offset)
{
    QRegularExpression pattern = buildSearchPattern(searchWord);
    
    // Get match length for the pattern
    QString text = textArea->toPlainText();
    int matchLength = searchWord.size();
    
    if (!positions.isEmpty() && position >= 0 && position < positions.size()) {
        QRegularExpressionMatch match = pattern.match(text, positions[position]);
        if (match.hasMatch()) {
            matchLength = match.capturedLength();
        }
    }
    
    clearSelectionFormat(cursor, matchLength);

    if (!positions.isEmpty()) {
        if (position >= positions.size() - 1)
            position = -1;

        // Get new match length
        if (position + 1 < positions.size()) {
            QRegularExpressionMatch nextMatch = pattern.match(text, positions[position + 1]);
            if (nextMatch.hasMatch()) {
                matchLength = nextMatch.capturedLength();
            }
        }

        selectSearchWord(cursor, matchLength, offset);
    }
}

void FindReplacePanel::findPrevious(QTextCursor& cursor, const QString& searchWord)
{
    QRegularExpression pattern = buildSearchPattern(searchWord);
    
    // Get match length for the pattern
    QString text = textArea->toPlainText();
    int matchLength = searchWord.size();
    
    if (!positions.isEmpty() && position >= 0 && position < positions.size()) {
        QRegularExpressionMatch match = pattern.match(text, positions[position]);
        if (match.hasMatch()) {
            matchLength = match.capturedLength();
        }
    }
    
    clearSelectionFormat(cursor, matchLength);

    if (!positions.isEmpty()) {
        // Move to previous position
        position--;
        if (position < 0) {
            position = positions.size() - 1;
        }

        // Get match length for new position
        if (position >= 0 && position < positions.size()) {
            QRegularExpressionMatch prevMatch = pattern.match(text, positions[position]);
            if (prevMatch.hasMatch()) {
                matchLength = prevMatch.capturedLength();
            }
        }

        cursor.setPosition(positions[position]);

        if (!cursor.isNull()) {
            cursor.clearSelection();
            cursor.setPosition(positions[position] + matchLength, QTextCursor::KeepAnchor);
            prevFormat = cursor.charFormat();
            cursor.setCharFormat(colorFormat);
            textArea->setTextCursor(cursor);
        }
    }
}

void FindReplacePanel::on_replaceAll_clicked()
{
    if (textArea) {
        textArea->setFocus();
        QString searchWord = ui->searchFind->text();
        QString replaceWord = ui->fieldReplace->text();
        
        if (searchWord.isEmpty()) {
            return;
        }
        
        addToSearchHistory(searchWord);
        
        QRegularExpression pattern = buildSearchPattern(searchWord);
        QString text = textArea->toPlainText();
        
        // Collect all matches first
        QVector<QPair<int, int>> matchRanges;  // start position and length
        QRegularExpressionMatchIterator matches = pattern.globalMatch(text);
        
        while (matches.hasNext()) {
            QRegularExpressionMatch match = matches.next();
            matchRanges.push_back(qMakePair(match.capturedStart(), match.capturedLength()));
        }
        
        if (matchRanges.isEmpty()) {
            position = -1;
            positions.clear();
            updateCounterLabels();
            return;
        }
        
        // Replace from end to start to preserve positions
        QTextCursor cursor(textArea->document());
        cursor.beginEditBlock();
        
        for (int i = matchRanges.size() - 1; i >= 0; --i) {
            int start = matchRanges[i].first;
            int length = matchRanges[i].second;
            
            cursor.setPosition(start);
            cursor.setPosition(start + length, QTextCursor::KeepAnchor);
            
            QString matchedText = cursor.selectedText();
            QString finalReplacement = applyPreserveCase(replaceWord, matchedText);
            
            cursor.removeSelectedText();
            cursor.insertText(finalReplacement);
        }
        
        cursor.endEditBlock();
        textArea->setTextCursor(cursor);
        
        // Clear search state
        position = -1;
        positions.clear();
        textArea->updateSyntaxHighlightTags();
        updateCounterLabels();
    }
}
