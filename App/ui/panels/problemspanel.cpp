#include "problemspanel.h"
#include <QHeaderView>
#include <QFileInfo>

ProblemsPanel::ProblemsPanel(QWidget* parent)
    : QWidget(parent)
    , m_tree(nullptr)
    , m_statusLabel(nullptr)
    , m_filterCombo(nullptr)
    , m_errorCount(0)
    , m_warningCount(0)
    , m_infoCount(0)
    , m_hintCount(0)
    , m_currentFilter(0)
{
    setupUI();
}

ProblemsPanel::~ProblemsPanel()
{
}

void ProblemsPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    QWidget* header = new QWidget(this);
    header->setStyleSheet("background: #171c24; border-bottom: 1px solid #2a3241;");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    QLabel* titleLabel = new QLabel(tr("Problems"), header);
    titleLabel->setStyleSheet("font-weight: bold; color: #e6edf3;");
    headerLayout->addWidget(titleLabel);

    m_filterCombo = new QComboBox(header);
    m_filterCombo->addItem(tr("All"));
    m_filterCombo->addItem(tr("Errors"));
    m_filterCombo->addItem(tr("Warnings"));
    m_filterCombo->addItem(tr("Info"));
    m_filterCombo->setStyleSheet(
        "QComboBox { background: #1f2632; color: #e6edf3; border: 1px solid #2a3241; padding: 2px 8px; }"
        "QComboBox::drop-down { border: none; }"
    );
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ProblemsPanel::onFilterChanged);
    headerLayout->addWidget(m_filterCombo);

    headerLayout->addStretch();

    m_statusLabel = new QLabel(header);
    m_statusLabel->setStyleSheet("color: #9aa4b2;");
    headerLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(header);

    // Tree widget
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Problem"), tr("Location")});
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

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ProblemsPanel::onItemDoubleClicked);

    mainLayout->addWidget(m_tree);

    updateCounts();
}

void ProblemsPanel::setDiagnostics(const QString& uri, const QList<LspDiagnostic>& diagnostics)
{
    if (diagnostics.isEmpty()) {
        m_diagnostics.remove(uri);
    } else {
        m_diagnostics[uri] = diagnostics;
    }
    rebuildTree();
}

void ProblemsPanel::clearAll()
{
    m_diagnostics.clear();
    rebuildTree();
}

void ProblemsPanel::clearFile(const QString& uri)
{
    m_diagnostics.remove(uri);
    rebuildTree();
}

int ProblemsPanel::totalCount() const
{
    return m_errorCount + m_warningCount + m_infoCount + m_hintCount;
}

int ProblemsPanel::errorCount() const
{
    return m_errorCount;
}

int ProblemsPanel::warningCount() const
{
    return m_warningCount;
}

int ProblemsPanel::infoCount() const
{
    return m_infoCount;
}

void ProblemsPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    // Check if this is a diagnostic item (has parent = file item)
    if (!item->parent())
        return;

    QString filePath = item->data(0, Qt::UserRole).toString();
    int line = item->data(0, Qt::UserRole + 1).toInt();
    int col = item->data(0, Qt::UserRole + 2).toInt();

    emit problemClicked(filePath, line, col);
}

void ProblemsPanel::onFilterChanged(int index)
{
    m_currentFilter = index;
    rebuildTree();
}

void ProblemsPanel::updateCounts()
{
    m_errorCount = 0;
    m_warningCount = 0;
    m_infoCount = 0;
    m_hintCount = 0;

    for (const auto& diagList : m_diagnostics) {
        for (const auto& diag : diagList) {
            switch (diag.severity) {
            case LspDiagnosticSeverity::Error:
                m_errorCount++;
                break;
            case LspDiagnosticSeverity::Warning:
                m_warningCount++;
                break;
            case LspDiagnosticSeverity::Information:
                m_infoCount++;
                break;
            case LspDiagnosticSeverity::Hint:
                m_hintCount++;
                break;
            }
        }
    }

    QString status = QString("Errors: %1  Warnings: %2  Info: %3")
                         .arg(m_errorCount)
                         .arg(m_warningCount)
                         .arg(m_infoCount + m_hintCount);
    m_statusLabel->setText(status);

    emit countsChanged(m_errorCount, m_warningCount, m_infoCount + m_hintCount);
}

void ProblemsPanel::rebuildTree()
{
    m_tree->clear();
    updateCounts();

    for (auto it = m_diagnostics.constBegin(); it != m_diagnostics.constEnd(); ++it) {
        QString uri = it.key();
        const QList<LspDiagnostic>& diagList = it.value();

        // Convert URI to file path
        QString filePath = uri;
        if (filePath.startsWith("file://")) {
            filePath = filePath.mid(7);
        }
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();

        // Count visible diagnostics for this file
        int visibleCount = 0;
        for (const auto& diag : diagList) {
            bool show = (m_currentFilter == 0) ||
                        (m_currentFilter == 1 && diag.severity == LspDiagnosticSeverity::Error) ||
                        (m_currentFilter == 2 && diag.severity == LspDiagnosticSeverity::Warning) ||
                        (m_currentFilter == 3 && (diag.severity == LspDiagnosticSeverity::Information || 
                                                   diag.severity == LspDiagnosticSeverity::Hint));
            if (show)
                visibleCount++;
        }

        if (visibleCount == 0)
            continue;

        // File item
        QTreeWidgetItem* fileItem = new QTreeWidgetItem(m_tree);
        fileItem->setText(0, QString("%1 (%2)").arg(fileName).arg(visibleCount));
        fileItem->setToolTip(0, filePath);
        fileItem->setExpanded(true);

        // Diagnostic items
        for (const auto& diag : diagList) {
            bool show = (m_currentFilter == 0) ||
                        (m_currentFilter == 1 && diag.severity == LspDiagnosticSeverity::Error) ||
                        (m_currentFilter == 2 && diag.severity == LspDiagnosticSeverity::Warning) ||
                        (m_currentFilter == 3 && (diag.severity == LspDiagnosticSeverity::Information || 
                                                   diag.severity == LspDiagnosticSeverity::Hint));
            if (!show)
                continue;

            QTreeWidgetItem* diagItem = new QTreeWidgetItem(fileItem);
            
            QString icon = severityIcon(diag.severity);
            QString message = QString("%1 %2").arg(icon).arg(diag.message);
            diagItem->setText(0, message);
            
            QString location = QString("[%1:%2]").arg(diag.range.start.line + 1).arg(diag.range.start.character + 1);
            diagItem->setText(1, location);

            // Store navigation data
            diagItem->setData(0, Qt::UserRole, filePath);
            diagItem->setData(0, Qt::UserRole + 1, diag.range.start.line);
            diagItem->setData(0, Qt::UserRole + 2, diag.range.start.character);

            // Color based on severity
            QColor color;
            switch (diag.severity) {
            case LspDiagnosticSeverity::Error:
                color = QColor("#f14c4c");
                break;
            case LspDiagnosticSeverity::Warning:
                color = QColor("#cca700");
                break;
            case LspDiagnosticSeverity::Information:
                color = QColor("#3794ff");
                break;
            case LspDiagnosticSeverity::Hint:
                color = QColor("#888888");
                break;
            }
            diagItem->setForeground(0, color);
        }
    }
}

QString ProblemsPanel::severityIcon(LspDiagnosticSeverity severity) const
{
    switch (severity) {
    case LspDiagnosticSeverity::Error:
        return "⛔";
    case LspDiagnosticSeverity::Warning:
        return "⚠️";
    case LspDiagnosticSeverity::Information:
        return "ℹ️";
    case LspDiagnosticSeverity::Hint:
        return "💡";
    }
    return "";
}

QString ProblemsPanel::severityText(LspDiagnosticSeverity severity) const
{
    switch (severity) {
    case LspDiagnosticSeverity::Error:
        return tr("Error");
    case LspDiagnosticSeverity::Warning:
        return tr("Warning");
    case LspDiagnosticSeverity::Information:
        return tr("Info");
    case LspDiagnosticSeverity::Hint:
        return tr("Hint");
    }
    return "";
}
