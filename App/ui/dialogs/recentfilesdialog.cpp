#include "recentfilesdialog.h"
#include "../../core/recentfilesmanager.h"
#include <QFileInfo>
#include <algorithm>

namespace {
    // Em dash character for visual separator
    const QString kEmDash = QStringLiteral("\u2014");
}

RecentFilesDialog::RecentFilesDialog(RecentFilesManager* manager, QWidget* parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_manager(manager)
    , m_searchBox(nullptr)
    , m_resultsList(nullptr)
    , m_layout(nullptr)
{
    setupUI();
}

RecentFilesDialog::~RecentFilesDialog()
{
}

void RecentFilesDialog::setupUI()
{
    setMinimumWidth(500);
    setMaximumHeight(400);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(4);

    // Search box
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search recent files..."));
    m_searchBox->setStyleSheet(
        "QLineEdit {"
        "  padding: 8px;"
        "  font-size: 14px;"
        "  border: 1px solid #2a3241;"
        "  border-radius: 4px;"
        "  background: #1f2632;"
        "  color: #e6edf3;"
        "}"
    );
    m_layout->addWidget(m_searchBox);

    // Results list
    m_resultsList = new QListWidget(this);
    m_resultsList->setStyleSheet(
        "QListWidget {"
        "  border: none;"
        "  background: #0e1116;"
        "  color: #e6edf3;"
        "}"
        "QListWidget::item {"
        "  padding: 8px;"
        "  border-bottom: 1px solid #2a3241;"
        "}"
        "QListWidget::item:selected {"
        "  background: #1b2a43;"
        "}"
        "QListWidget::item:hover {"
        "  background: #222a36;"
        "}"
    );
    m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_layout->addWidget(m_resultsList);

    // Connections
    connect(m_searchBox, &QLineEdit::textChanged, this, &RecentFilesDialog::onSearchTextChanged);
    connect(m_resultsList, &QListWidget::itemActivated, this, &RecentFilesDialog::onItemActivated);
    connect(m_resultsList, &QListWidget::itemClicked, this, &RecentFilesDialog::onItemClicked);

    // Install event filter for keyboard navigation
    m_searchBox->installEventFilter(this);

    setStyleSheet("RecentFilesDialog { background: #171c24; border: 1px solid #2a3241; border-radius: 8px; }");
}

void RecentFilesDialog::showDialog()
{
    refresh();
    m_searchBox->clear();
    updateResults(QString());

    // Position at top center of parent
    if (parentWidget()) {
        QPoint parentCenter = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        int x = parentCenter.x() - width() / 2;
        int y = parentWidget()->mapToGlobal(QPoint(0, 0)).y() + 50;
        move(x, y);
    }

    show();
    m_searchBox->setFocus();
    
    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }
}

void RecentFilesDialog::refresh()
{
    if (m_manager) {
        m_recentFiles = m_manager->recentFiles();
    }
}

void RecentFilesDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        hide();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_resultsList->currentRow() >= 0) {
            selectFile(m_resultsList->currentRow());
        }
        break;
    case Qt::Key_Up:
        selectPrevious();
        break;
    case Qt::Key_Down:
        selectNext();
        break;
    default:
        QDialog::keyPressEvent(event);
    }
}

bool RecentFilesDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_searchBox && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Up:
            selectPrevious();
            return true;
        case Qt::Key_Down:
            selectNext();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_resultsList->currentRow() >= 0) {
                selectFile(m_resultsList->currentRow());
            }
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void RecentFilesDialog::onSearchTextChanged(const QString& text)
{
    updateResults(text);
}

void RecentFilesDialog::onItemActivated(QListWidgetItem* item)
{
    int row = m_resultsList->row(item);
    if (row >= 0) {
        selectFile(row);
    }
}

void RecentFilesDialog::onItemClicked(QListWidgetItem* item)
{
    onItemActivated(item);
}

void RecentFilesDialog::updateResults(const QString& query)
{
    m_resultsList->clear();
    m_filteredIndices.clear();

    QList<QPair<int, int>> scored;  // score, index

    for (int i = 0; i < m_recentFiles.size(); ++i) {
        QFileInfo fileInfo(m_recentFiles[i]);
        QString fileName = fileInfo.fileName();
        
        int score = 0;
        if (query.isEmpty()) {
            // When empty, order by recency (index)
            score = 1000 - i;
        } else {
            // Match against file name and full path
            int nameScore = fuzzyMatch(query.toLower(), fileName.toLower());
            int pathScore = fuzzyMatch(query.toLower(), m_recentFiles[i].toLower()) / 2;
            score = qMax(nameScore, pathScore);
        }

        if (score > 0) {
            scored.append(qMakePair(score, i));
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const QPair<int, int>& a, const QPair<int, int>& b) {
        return a.first > b.first;
    });

    // Limit results
    int maxResults = 15;
    for (int i = 0; i < qMin(scored.size(), maxResults); ++i) {
        int idx = scored[i].second;
        m_filteredIndices.append(idx);

        QFileInfo fileInfo(m_recentFiles[idx]);
        QListWidgetItem* item = new QListWidgetItem();
        
        QString displayText = fileInfo.fileName();
        QString directory = fileInfo.absolutePath();
        displayText += QString("  %1 %2").arg(kEmDash).arg(directory);
        
        item->setText(displayText);
        item->setData(Qt::UserRole, m_recentFiles[idx]);
        item->setToolTip(m_recentFiles[idx]);
        m_resultsList->addItem(item);
    }

    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }

    // Adjust height
    int itemHeight = 35;
    int newHeight = qMin(m_resultsList->count() * itemHeight + 60, 400);
    setFixedHeight(qMax(newHeight, 100));
}

int RecentFilesDialog::fuzzyMatch(const QString& pattern, const QString& text)
{
    if (pattern.isEmpty())
        return 1000;

    // Exact match gets highest score
    if (text.contains(pattern))
        return 2000 + (1000 - text.indexOf(pattern));

    // Fuzzy matching - all characters must appear in order
    int patternIdx = 0;
    int score = 0;
    int lastMatchIdx = -1;

    for (int i = 0; i < text.length() && patternIdx < pattern.length(); ++i) {
        if (text[i] == pattern[patternIdx]) {
            // Bonus for consecutive matches
            if (lastMatchIdx == i - 1) {
                score += 15;
            }
            // Bonus for word boundary matches
            if (i == 0 || text[i - 1] == '/' || text[i - 1] == '\\' || text[i - 1] == '_' || text[i - 1] == '.') {
                score += 10;
            }
            score += 10;
            lastMatchIdx = i;
            patternIdx++;
        }
    }

    // All pattern characters must be matched
    if (patternIdx != pattern.length())
        return 0;

    return score;
}

void RecentFilesDialog::selectFile(int row)
{
    if (row < 0 || row >= m_filteredIndices.size())
        return;

    int fileIdx = m_filteredIndices[row];
    if (fileIdx < 0 || fileIdx >= m_recentFiles.size())
        return;

    hide();

    emit fileSelected(m_recentFiles[fileIdx]);
}

void RecentFilesDialog::selectNext()
{
    int current = m_resultsList->currentRow();
    if (current < m_resultsList->count() - 1) {
        m_resultsList->setCurrentRow(current + 1);
    }
}

void RecentFilesDialog::selectPrevious()
{
    int current = m_resultsList->currentRow();
    if (current > 0) {
        m_resultsList->setCurrentRow(current - 1);
    }
}
