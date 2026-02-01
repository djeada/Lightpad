#include "findreplacepanel.h"
#include "../../core/textarea.h"
#include "ui_findreplacepanel.h"

#include <QDebug>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QTreeWidget>
#include <QHeaderView>
#include <QSpacerItem>
#include <QSizePolicy>

namespace {
void setModeLayoutVisible(Ui::FindReplacePanel* ui, bool visible)
{
    if (!ui) {
        return;
    }
    ui->modeLabel->setVisible(visible);
    ui->localMode->setVisible(visible);
    ui->globalMode->setVisible(visible);
    if (ui->modeSpacer) {
        if (visible) {
            ui->modeSpacer->changeSize(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        } else {
            ui->modeSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        }
    }
    if (ui->modeLayout) {
        ui->modeLayout->invalidate();
    }
}
} // namespace

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget* parent)
    : QWidget(parent)
    , document(nullptr)
    , textArea(nullptr)
    , mainWindow(nullptr)
    , ui(new Ui::FindReplacePanel)
    , onlyFind(onlyFind)
    , position(-1)
    , globalResultIndex(-1)
    , resultsTree(nullptr)
    , searchHistoryIndex(-1)
    , m_vimCommandMode(false)
{
    ui->setupUi(this);

    show();

    ui->options->setVisible(false);
    setReplaceVisibility(onlyFind);

    colorFormat.setBackground(Qt::red);
    colorFormat.setForeground(Qt::white);

    // Create results tree for search results (works for both local and global modes)
    resultsTree = new QTreeWidget(this);
    resultsTree->setHeaderLabels(QStringList() << "File" << "Line" << "Match");
    resultsTree->setColumnCount(3);
    resultsTree->header()->setStretchLastSection(true);
    resultsTree->setVisible(false);
    resultsTree->setMinimumHeight(150);
    
    // Insert results tree into layout
    if (layout()) {
        layout()->addWidget(resultsTree);
    }
    
    // Handle clicks on results - route to appropriate handler based on mode
    connect(resultsTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        if (isGlobalMode()) {
            onGlobalResultClicked(item, column);
        } else {
            onLocalResultClicked(item, column);
        }
    });
    
    updateModeUI();
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

bool FindReplacePanel::isOnlyFind() const
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

void FindReplacePanel::setMainWindow(MainWindow* window)
{
    mainWindow = window;
}

void FindReplacePanel::setProjectPath(const QString& path)
{
    projectPath = path;
}

void FindReplacePanel::setFocusOnSearchBox()
{
    ui->searchFind->setFocus();
}

void FindReplacePanel::setVimCommandMode(bool enabled)
{
    if (m_vimCommandMode == enabled) {
        return;
    }
    m_vimCommandMode = enabled;
    if (enabled) {
        setOnlyFind(true);
        setReplaceVisibility(false);
        setModeLayoutVisible(ui, false);
        ui->options->setVisible(false);
        ui->more->setVisible(false);
        ui->findPrevious->setVisible(false);
        ui->find->setVisible(false);
        ui->replaceSingle->setVisible(false);
        ui->replaceAll->setVisible(false);
        ui->currentIndex->setVisible(false);
        ui->totalFound->setVisible(false);
        ui->label->setVisible(false);
        ui->searchBackward->setChecked(false);
        ui->searchStart->setChecked(true);
    } else {
        setModeLayoutVisible(ui, true);
        ui->more->setVisible(true);
        ui->findPrevious->setVisible(true);
        ui->find->setVisible(true);
        ui->currentIndex->setVisible(true);
        ui->totalFound->setVisible(true);
        ui->label->setVisible(true);
        ui->findWhat->setText("Find what :");
    }
}

bool FindReplacePanel::isVimCommandMode() const
{
    return m_vimCommandMode;
}

void FindReplacePanel::setSearchPrefix(const QString& prefix)
{
    m_searchPrefix = prefix;
    ui->findWhat->setText(QString("Command (%1):").arg(prefix));
}

void FindReplacePanel::setSearchText(const QString& text)
{
    ui->searchFind->setText(text);
    ui->searchFind->setCursorPosition(text.length());
}

bool FindReplacePanel::isGlobalMode() const
{
    return ui->globalMode->isChecked();
}

void FindReplacePanel::on_localMode_toggled(bool checked)
{
    if (checked) {
        updateModeUI();
    }
}

void FindReplacePanel::on_globalMode_toggled(bool checked)
{
    if (checked) {
        updateModeUI();
    }
}

void FindReplacePanel::updateModeUI()
{
    bool isGlobal = ui->globalMode->isChecked();
    
    // Show/hide results tree based on mode and whether we have results
    if (resultsTree) {
        if (isGlobal) {
            resultsTree->setVisible(!globalResults.isEmpty());
        } else {
            resultsTree->setVisible(!positions.isEmpty());
        }
    }
    
    // In global mode, some options don't apply
    ui->searchStart->setEnabled(!isGlobal);
    ui->searchBackward->setEnabled(!isGlobal);
    
    // Clear previous search results when switching modes
    if (isGlobal) {
        positions.clear();
        position = -1;
    } else {
        globalResults.clear();
        globalResultIndex = -1;
    }
    
    // Clear results tree when switching modes
    if (resultsTree) {
        resultsTree->clear();
        resultsTree->setVisible(false);
    }
    
    updateCounterLabels();
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
    
    const int textLength = matchedText.length();
    for (int i = 0; i < textLength; ++i) {
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
    } else if (firstUpper && textLength > 1) {
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
    if (m_vimCommandMode) {
        return;
    }
    QString searchWord = ui->searchFind->text();
    
    if (searchWord.isEmpty()) {
        return;
    }
    
    addToSearchHistory(searchWord);
    
    // Check if global mode
    if (isGlobalMode()) {
        performGlobalSearch(searchWord);
        return;
    }
    
    // Local mode - search in current file
    if (textArea) {
        textArea->setFocus();
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
    if (m_vimCommandMode) {
        return;
    }
    QString searchWord = ui->searchFind->text();
    
    if (searchWord.isEmpty()) {
        return;
    }
    
    addToSearchHistory(searchWord);
    
    // Global mode - navigate to previous result
    if (isGlobalMode()) {
        if (!globalResults.isEmpty()) {
            globalResultIndex--;
            if (globalResultIndex < 0) {
                globalResultIndex = globalResults.size() - 1;
            }
            navigateToGlobalResult(globalResultIndex);
            updateCounterLabels();
        }
        return;
    }
    
    // Local mode
    if (textArea) {
        textArea->setFocus();
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
    if (m_vimCommandMode) {
        return;
    }
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
    if (m_vimCommandMode) {
        setVimCommandMode(false);
        if (textArea) {
            textArea->setFocus();
        }
    }
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
            
            // Adjust position index for next search operation
            if (position > 0) {
                position--;
            } else if (positions.isEmpty()) {
                position = -1;
            } else {
                position = -1;  // Reset to start; next find operation will set to 0
            }
        }
    }
}

void FindReplacePanel::updateCounterLabels()
{
    // Handle global mode
    if (isGlobalMode()) {
        if (globalResults.isEmpty()) {
            ui->currentIndex->hide();
            ui->totalFound->hide();
            ui->label->hide();
        } else {
            if (ui->currentIndex->isHidden()) {
                ui->currentIndex->show();
                ui->totalFound->show();
                ui->label->show();
            }

            ui->currentIndex->setText(QString::number(globalResultIndex + 1));
            ui->totalFound->setText(QString::number(globalResults.size()));
        }
        return;
    }
    
    // Handle local mode
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
            if (ui->searchBackward->isChecked()) {
                // For backward search, find the last match before cursor
                // positions are already reversed, so we want the first one that's before startPos
                for (int i = 0; i < positions.size(); ++i) {
                    if (positions[i] < startPos) {
                        // Found a match before cursor, start just before it
                        position = i - 1;
                        break;
                    }
                }
                // If no match found before cursor, position stays at -1 (start from first in reversed list)
            } else {
                // For forward search, find the first match at or after cursor
                for (int i = 0; i < positions.size(); ++i) {
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
    
    // Display results in the tree for local mode
    displayLocalResults(searchWord);
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
    if (m_vimCommandMode) {
        return;
    }
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

// Global search methods

QStringList FindReplacePanel::getProjectFiles() const
{
    QStringList files;
    
    if (projectPath.isEmpty()) {
        return files;
    }
    
    QDirIterator it(projectPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    
    // File extensions to search (common source code files)
    QStringList searchableExtensions = {
        "cpp", "hpp", "c", "h", "cc", "cxx", "hxx",  // C/C++
        "py", "pyw",                                   // Python
        "js", "jsx", "ts", "tsx",                     // JavaScript/TypeScript
        "java",                                        // Java
        "go",                                          // Go
        "rs",                                          // Rust
        "rb",                                          // Ruby
        "php",                                         // PHP
        "swift",                                       // Swift
        "kt", "kts",                                   // Kotlin
        "cs",                                          // C#
        "html", "htm", "css", "scss", "sass", "less", // Web
        "json", "xml", "yaml", "yml", "toml",         // Config
        "md", "txt", "rst",                            // Text/Doc
        "sql",                                         // SQL
        "sh", "bash", "zsh",                           // Shell
        "cmake", "make", "makefile"                    // Build
    };
    
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        QString ext = fileInfo.suffix().toLower();
        QString fileNameLower = fileInfo.baseName().toLower();
        
        // Check if extension or file name matches (e.g., "makefile" has no extension)
        if (searchableExtensions.contains(ext) || 
            searchableExtensions.contains(fileNameLower)) {
            files.append(filePath);
        }
    }
    
    return files;
}

void FindReplacePanel::performGlobalSearch(const QString& searchWord)
{
    globalResults.clear();
    globalResultIndex = -1;
    
    if (resultsTree) {
        resultsTree->clear();
    }
    
    QRegularExpression pattern = buildSearchPattern(searchWord);
    
    if (!pattern.isValid()) {
        updateCounterLabels();
        return;
    }
    
    QStringList files = getProjectFiles();
    
    for (const QString& filePath : files) {
        searchInFile(filePath, pattern);
    }
    
    displayGlobalResults();
    
    // Navigate to first result if any
    if (!globalResults.isEmpty()) {
        globalResultIndex = 0;
        navigateToGlobalResult(0);
    }
    
    updateCounterLabels();
}

void FindReplacePanel::searchInFile(const QString& filePath, const QRegularExpression& pattern)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();
    
    // Split into lines for line number tracking
    QStringList lines = content.split('\n');
    
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        const QString& line = lines[lineNum];
        
        QRegularExpressionMatchIterator matches = pattern.globalMatch(line);
        while (matches.hasNext()) {
            QRegularExpressionMatch match = matches.next();
            
            GlobalSearchResult result;
            result.filePath = filePath;
            result.lineNumber = lineNum + 1;  // 1-based line numbers
            result.columnNumber = match.capturedStart() + 1;  // 1-based columns
            result.matchLength = match.capturedLength();
            result.lineContent = line.trimmed();
            
            globalResults.append(result);
        }
    }
}

void FindReplacePanel::displayGlobalResults()
{
    if (!resultsTree) {
        return;
    }
    
    resultsTree->clear();
    
    // Group results by file
    QMap<QString, QVector<GlobalSearchResult>> fileGroups;
    for (const GlobalSearchResult& result : globalResults) {
        fileGroups[result.filePath].append(result);
    }
    
    for (auto it = fileGroups.begin(); it != fileGroups.end(); ++it) {
        const QString& filePath = it.key();
        const QVector<GlobalSearchResult>& results = it.value();
        
        // Create file node
        QFileInfo fileInfo(filePath);
        QString displayPath = filePath;
        if (!projectPath.isEmpty() && filePath.startsWith(projectPath)) {
            displayPath = filePath.mid(projectPath.length() + 1);
        }
        
        QTreeWidgetItem* fileItem = new QTreeWidgetItem(resultsTree);
        fileItem->setText(0, displayPath);
        fileItem->setText(1, QString::number(results.size()) + " matches");
        fileItem->setData(0, Qt::UserRole, filePath);
        fileItem->setData(1, Qt::UserRole, -1);  // -1 indicates it's a file node
        
        // Add result items
        for (int i = 0; i < results.size(); ++i) {
            const GlobalSearchResult& result = results[i];
            
            QTreeWidgetItem* resultItem = new QTreeWidgetItem(fileItem);
            resultItem->setText(0, "");
            resultItem->setText(1, QString::number(result.lineNumber));
            resultItem->setText(2, result.lineContent);
            resultItem->setData(0, Qt::UserRole, filePath);
            resultItem->setData(1, Qt::UserRole, result.lineNumber);
            resultItem->setData(2, Qt::UserRole, result.columnNumber);
        }
        
        fileItem->setExpanded(true);
    }
    
    resultsTree->setVisible(!globalResults.isEmpty());
}

void FindReplacePanel::navigateToGlobalResult(int index)
{
    if (index < 0 || index >= globalResults.size()) {
        return;
    }
    
    const GlobalSearchResult& result = globalResults[index];
    
    // Emit signal or directly open file (this would need MainWindow integration)
    // For now, we'll highlight the item in the tree
    if (resultsTree) {
        // Find and select the corresponding tree item
        int currentIndex = 0;
        for (int i = 0; i < resultsTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* fileItem = resultsTree->topLevelItem(i);
            for (int j = 0; j < fileItem->childCount(); ++j) {
                if (currentIndex == index) {
                    resultsTree->setCurrentItem(fileItem->child(j));
                    return;
                }
                currentIndex++;
            }
        }
    }
}

void FindReplacePanel::onGlobalResultClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item) {
        return;
    }
    
    QString filePath = item->data(0, Qt::UserRole).toString();
    int lineNumber = item->data(1, Qt::UserRole).toInt();
    
    // If it's a file node (lineNumber == -1), don't navigate
    if (lineNumber < 0) {
        return;
    }
    
    int columnNumber = item->data(2, Qt::UserRole).toInt();
    
    // Find the index in globalResults
    for (int i = 0; i < globalResults.size(); ++i) {
        const GlobalSearchResult& result = globalResults[i];
        if (result.filePath == filePath && 
            result.lineNumber == lineNumber && 
            result.columnNumber == columnNumber) {
            globalResultIndex = i;
            break;
        }
    }
    
    // Emit signal to navigate to the file and position
    emit navigateToFile(filePath, lineNumber, columnNumber);
    
    updateCounterLabels();
}

void FindReplacePanel::displayLocalResults(const QString& searchWord)
{
    if (!resultsTree || !textArea) {
        return;
    }
    
    resultsTree->clear();
    
    if (positions.isEmpty()) {
        resultsTree->setVisible(false);
        return;
    }
    
    QString text = textArea->toPlainText();
    QStringList lines = text.split('\n');
    QRegularExpression pattern = buildSearchPattern(searchWord);
    
    // Build line start positions for quick lookup
    QVector<int> lineStarts;
    int pos = 0;
    for (const QString& line : lines) {
        lineStarts.append(pos);
        pos += line.length() + 1; // +1 for newline
    }
    
    // Create result items for each match
    for (int i = 0; i < positions.size(); ++i) {
        int matchPos = positions[i];
        
        // Find line number for this position
        int lineNum = 0;
        for (int j = 0; j < lineStarts.size(); ++j) {
            if (j + 1 < lineStarts.size() && matchPos >= lineStarts[j + 1]) {
                continue;
            }
            lineNum = j;
            break;
        }
        
        int columnNum = matchPos - lineStarts[lineNum] + 1; // 1-based
        QString lineContent = (lineNum < lines.size()) ? lines[lineNum].trimmed() : QString();
        
        QTreeWidgetItem* resultItem = new QTreeWidgetItem(resultsTree);
        resultItem->setText(0, "Current File");
        resultItem->setText(1, QString::number(lineNum + 1)); // 1-based line number
        resultItem->setText(2, lineContent);
        resultItem->setData(0, Qt::UserRole, QString()); // Empty path for current file
        resultItem->setData(1, Qt::UserRole, lineNum + 1);
        resultItem->setData(2, Qt::UserRole, columnNum);
    }
    
    resultsTree->setVisible(true);
}

void FindReplacePanel::onLocalResultClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    if (!item || !textArea) {
        return;
    }
    
    int lineNumber = item->data(1, Qt::UserRole).toInt();
    int columnNumber = item->data(2, Qt::UserRole).toInt();
    
    if (lineNumber <= 0) {
        return;
    }
    
    // Navigate to position in current file
    QTextCursor cursor = textArea->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineNumber - 1);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, columnNumber - 1);
    
    // Select the match
    QString searchWord = ui->searchFind->text();
    QRegularExpression pattern = buildSearchPattern(searchWord);
    QString text = textArea->toPlainText();
    QRegularExpressionMatch match = pattern.match(text, cursor.position());
    if (match.hasMatch() && match.capturedStart() == cursor.position()) {
        cursor.setPosition(match.capturedStart());
        cursor.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
    }
    
    textArea->setTextCursor(cursor);
    textArea->setFocus();
    
    // Update position index
    for (int i = 0; i < positions.size(); ++i) {
        if (positions[i] == match.capturedStart()) {
            position = i;
            break;
        }
    }
    
    updateCounterLabels();
}
