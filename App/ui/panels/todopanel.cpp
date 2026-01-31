#include "todopanel.h"
#include <QHeaderView>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QRegularExpression>

TodoPanel::TodoPanel(QWidget* parent)
    : QWidget(parent)
    , m_tree(nullptr)
    , m_statusLabel(nullptr)
    , m_filterCombo(nullptr)
    , m_searchEdit(nullptr)
    , m_todoCount(0)
    , m_fixmeCount(0)
    , m_noteCount(0)
    , m_currentFilter(0)
{
    setupUI();
}

TodoPanel::~TodoPanel()
{
}

void TodoPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget* header = new QWidget(this);
    header->setStyleSheet("background: #171c24; border-bottom: 1px solid #2a3241;");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    QLabel* titleLabel = new QLabel(tr("Todo"), header);
    titleLabel->setStyleSheet("font-weight: bold; color: #e6edf3;");
    headerLayout->addWidget(titleLabel);

    m_filterCombo = new QComboBox(header);
    m_filterCombo->addItem(tr("All"));
    m_filterCombo->addItem(tr("TODO"));
    m_filterCombo->addItem(tr("FIXME"));
    m_filterCombo->addItem(tr("NOTE"));
    m_filterCombo->setStyleSheet(
        "QComboBox { background: #1f2632; color: #e6edf3; border: 1px solid #2a3241; padding: 2px 8px; }"
        "QComboBox::drop-down { border: none; }"
    );
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TodoPanel::onFilterChanged);
    headerLayout->addWidget(m_filterCombo);

    m_searchEdit = new QLineEdit(header);
    m_searchEdit->setPlaceholderText(tr("Search todos..."));
    m_searchEdit->setStyleSheet(
        "QLineEdit { background: #1f2632; color: #e6edf3; border: 1px solid #2a3241; padding: 2px 8px; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &TodoPanel::onSearchChanged);
    headerLayout->addWidget(m_searchEdit);

    headerLayout->addStretch();

    m_statusLabel = new QLabel(header);
    m_statusLabel->setStyleSheet("color: #9aa4b2;");
    headerLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(header);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Todo"), tr("Location")});
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setStyleSheet(
        "QTreeWidget {"
        "  background: #0e1116;"
        "  color: #e6edf3;"
        "  border: none;"
        "}"
        "QTreeWidget::item {"
        "  padding: 4px;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #1b2a43;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #222a36;"
        "}"
        "QHeaderView::section {"
        "  background: #171c24;"
        "  color: #9aa4b2;"
        "  padding: 4px;"
        "  border: none;"
        "  border-right: 1px solid #2a3241;"
        "}"
    );
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(0, 500);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &TodoPanel::onItemDoubleClicked);

    mainLayout->addWidget(m_tree);

    updateCounts();
}

void TodoPanel::setTodos(const QString& filePath, const QString& content)
{
    QList<TodoEntry> entries;
    QRegularExpression regex(R"(\b(TODO|FIXME|NOTE)\b\s*:?\s*(.*))");
    const QStringList lines = content.split('\n');

    for (int i = 0; i < lines.size(); ++i) {
        QRegularExpressionMatch match = regex.match(lines[i]);
        if (!match.hasMatch())
            continue;

        TodoEntry entry;
        entry.filePath = filePath;
        entry.tag = match.captured(1);
        entry.message = match.captured(2).trimmed();
        entry.line = i + 1;
        entries.append(entry);
    }

    if (entries.isEmpty()) {
        m_entries.remove(filePath);
    } else {
        m_entries[filePath] = entries;
    }

    rebuildTree();
}

void TodoPanel::clearAll()
{
    m_entries.clear();
    rebuildTree();
}

int TodoPanel::totalCount() const
{
    return m_todoCount + m_fixmeCount + m_noteCount;
}

int TodoPanel::todoCount() const
{
    return m_todoCount;
}

int TodoPanel::fixmeCount() const
{
    return m_fixmeCount;
}

int TodoPanel::noteCount() const
{
    return m_noteCount;
}

void TodoPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (!item->parent())
        return;

    QString filePath = item->data(0, Qt::UserRole).toString();
    int line = item->data(0, Qt::UserRole + 1).toInt();
    emit todoClicked(filePath, line);
}

void TodoPanel::onFilterChanged(int index)
{
    m_currentFilter = index;
    rebuildTree();
}

void TodoPanel::onSearchChanged(const QString& text)
{
    m_searchText = text.trimmed();
    rebuildTree();
}

void TodoPanel::updateCounts()
{
    m_todoCount = 0;
    m_fixmeCount = 0;
    m_noteCount = 0;

    for (const auto& list : m_entries) {
        for (const auto& entry : list) {
            if (entry.tag == "TODO")
                m_todoCount++;
            else if (entry.tag == "FIXME")
                m_fixmeCount++;
            else if (entry.tag == "NOTE")
                m_noteCount++;
        }
    }

    QString status = QString("Todo: %1  Fixme: %2  Note: %3")
                         .arg(m_todoCount)
                         .arg(m_fixmeCount)
                         .arg(m_noteCount);
    m_statusLabel->setText(status);
}

void TodoPanel::rebuildTree()
{
    m_tree->clear();
    updateCounts();

    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QString filePath = it.key();
        const QList<TodoEntry>& items = it.value();
        int visibleCount = 0;

        for (const auto& entry : items) {
            if (passesFilter(entry) && passesSearch(entry))
                visibleCount++;
        }

        if (visibleCount == 0)
            continue;

        QTreeWidgetItem* fileItem = new QTreeWidgetItem(m_tree);
        fileItem->setText(0, QString("%1 (%2)").arg(displayNameForPath(filePath)).arg(visibleCount));
        fileItem->setToolTip(0, filePath);
        fileItem->setExpanded(true);

        for (const auto& entry : items) {
            if (!passesFilter(entry) || !passesSearch(entry))
                continue;

            QTreeWidgetItem* todoItem = new QTreeWidgetItem(fileItem);
            QString text = QString("%1 %2").arg(tagIcon(entry.tag)).arg(entry.message);
            todoItem->setText(0, text.trimmed());
            todoItem->setText(1, QString("[%1]").arg(entry.line));

            todoItem->setData(0, Qt::UserRole, entry.filePath);
            todoItem->setData(0, Qt::UserRole + 1, entry.line - 1);

            QColor color("#9aa4b2");
            if (entry.tag == "TODO") {
                color = QColor("#58a6ff");
            } else if (entry.tag == "FIXME") {
                color = QColor("#f14c4c");
            } else if (entry.tag == "NOTE") {
                color = QColor("#cca700");
            }
            todoItem->setForeground(0, color);
        }
    }
}

bool TodoPanel::passesFilter(const TodoEntry& entry) const
{
    if (m_currentFilter == 0)
        return true;
    if (m_currentFilter == 1)
        return entry.tag == "TODO";
    if (m_currentFilter == 2)
        return entry.tag == "FIXME";
    if (m_currentFilter == 3)
        return entry.tag == "NOTE";
    return true;
}

bool TodoPanel::passesSearch(const TodoEntry& entry) const
{
    if (m_searchText.isEmpty())
        return true;
    return entry.message.contains(m_searchText, Qt::CaseInsensitive)
        || entry.tag.contains(m_searchText, Qt::CaseInsensitive)
        || entry.filePath.contains(m_searchText, Qt::CaseInsensitive);
}

QString TodoPanel::tagIcon(const QString& tag) const
{
    if (tag == "TODO")
        return "📝";
    if (tag == "FIXME")
        return "🛠️";
    if (tag == "NOTE")
        return "📌";
    return "•";
}

QString TodoPanel::displayNameForPath(const QString& filePath) const
{
    return QFileInfo(filePath).fileName();
}
