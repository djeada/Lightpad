#include "problemspanel.h"
#include "../uistylehelper.h"
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QHeaderView>
#include <QPushButton>

ProblemsPanel::ProblemsPanel(QWidget *parent)
    : QWidget(parent), m_tree(nullptr), m_statusLabel(nullptr),
      m_emptyStateLabel(nullptr), m_filterCombo(nullptr),
      m_autoRefreshCheckBox(nullptr), m_errorCount(0), m_warningCount(0),
      m_infoCount(0), m_hintCount(0), m_currentFilter(0),
      m_autoRefreshEnabled(true) {
  setupUI();
}

ProblemsPanel::~ProblemsPanel() {}

void ProblemsPanel::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  m_header = new QWidget(this);
  m_header->setStyleSheet(
      "background: #171c24; border-bottom: 1px solid #2a3241;");
  QHBoxLayout *headerLayout = new QHBoxLayout(m_header);
  headerLayout->setContentsMargins(8, 4, 8, 4);

  m_titleLabel = new QLabel(tr("Problems"), m_header);
  m_titleLabel->setStyleSheet("font-weight: bold; color: #e6edf3;");
  headerLayout->addWidget(m_titleLabel);

  m_filterCombo = new QComboBox(m_header);
  m_filterCombo->addItem(tr("All"));
  m_filterCombo->addItem(tr("Errors"));
  m_filterCombo->addItem(tr("Warnings"));
  m_filterCombo->addItem(tr("Info"));
  m_filterCombo->setStyleSheet(
      "QComboBox { background: #1f2632; color: #e6edf3; border: 1px solid "
      "#2a3241; padding: 2px 8px; }"
      "QComboBox::drop-down { border: none; }");
  connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ProblemsPanel::onFilterChanged);
  headerLayout->addWidget(m_filterCombo);

  headerLayout->addStretch();

  m_autoRefreshCheckBox = new QCheckBox(tr("Auto-refresh on save"), m_header);
  m_autoRefreshCheckBox->setChecked(m_autoRefreshEnabled);
  m_autoRefreshCheckBox->setStyleSheet(
      "QCheckBox { color: #9aa4b2; }"
      "QCheckBox::indicator { width: 14px; height: 14px; }"
      "QCheckBox::indicator:unchecked { border: 1px solid #2a3241; background: "
      "#1f2632; }"
      "QCheckBox::indicator:checked { border: 1px solid #3794ff; background: "
      "#3794ff; }");
  connect(m_autoRefreshCheckBox, &QCheckBox::toggled, this,
          &ProblemsPanel::onAutoRefreshToggled);
  headerLayout->addWidget(m_autoRefreshCheckBox);

  const QString buttonStyle =
      "QPushButton { background: #1f2632; color: #e6edf3; border: 1px solid "
      "#2a3241; padding: 2px 8px; }"
      "QPushButton:hover { background: #2a3241; }";

  auto *clearFileButton = new QPushButton(tr("Clear File"), m_header);
  clearFileButton->setStyleSheet(buttonStyle);
  connect(clearFileButton, &QPushButton::clicked, this,
          &ProblemsPanel::clearCurrentFile);
  headerLayout->addWidget(clearFileButton);

  auto *clearAllButton = new QPushButton(tr("Clear All"), m_header);
  clearAllButton->setStyleSheet(buttonStyle);
  connect(clearAllButton, &QPushButton::clicked, this,
          &ProblemsPanel::clearAll);
  headerLayout->addWidget(clearAllButton);

  auto *copyButton = new QPushButton(tr("Copy"), m_header);
  copyButton->setStyleSheet(buttonStyle);
  connect(copyButton, &QPushButton::clicked, this,
          &ProblemsPanel::copySelectedMessage);
  headerLayout->addWidget(copyButton);

  m_statusLabel = new QLabel(m_header);
  m_statusLabel->setObjectName("problemsStatusLabel");
  m_statusLabel->setTextFormat(Qt::RichText);
  m_statusLabel->setStyleSheet("color: #9aa4b2;");
  headerLayout->addWidget(m_statusLabel);

  mainLayout->addWidget(m_header);

  auto *treeContainer = new QWidget(this);
  auto *treeLayout = new QVBoxLayout(treeContainer);
  treeLayout->setContentsMargins(0, 0, 0, 0);
  treeLayout->setSpacing(0);

  m_tree = new QTreeWidget(treeContainer);
  m_tree->setObjectName("problemsTree");
  m_tree->setHeaderLabels({tr("Problem"), tr("Location")});
  m_tree->setRootIsDecorated(true);
  m_tree->setAlternatingRowColors(true);
  m_tree->setStyleSheet("QTreeWidget {"
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
                        "}");
  m_tree->header()->setStretchLastSection(true);
  m_tree->setColumnWidth(0, 500);

  connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
          &ProblemsPanel::onItemDoubleClicked);

  m_emptyStateLabel = new QLabel(treeContainer);
  m_emptyStateLabel->setObjectName("problemsEmptyState");
  m_emptyStateLabel->setAlignment(Qt::AlignCenter);
  m_emptyStateLabel->setText(
      tr("No problems detected.\n"
         "Diagnostics will appear here when issues are found in your code."));
  m_emptyStateLabel->setWordWrap(true);
  m_emptyStateLabel->setStyleSheet(
      "QLabel { color: #6e7681; font-size: 13px; padding: 32px; }");

  treeLayout->addWidget(m_tree);
  treeLayout->addWidget(m_emptyStateLabel);

  mainLayout->addWidget(treeContainer);

  updateCounts();
}

void ProblemsPanel::setDiagnostics(const QString &uri,
                                   const QList<LspDiagnostic> &diagnostics) {
  if (diagnostics.isEmpty()) {
    m_diagnostics.remove(uri);
  } else {
    m_diagnostics[uri] = diagnostics;
  }
  rebuildTree();
}

void ProblemsPanel::clearAll() {
  m_diagnostics.clear();
  rebuildTree();
}

void ProblemsPanel::clearFile(const QString &uri) {
  m_diagnostics.remove(uri);
  rebuildTree();
}

int ProblemsPanel::totalCount() const {
  return m_errorCount + m_warningCount + m_infoCount + m_hintCount;
}

int ProblemsPanel::errorCount() const { return m_errorCount; }

int ProblemsPanel::warningCount() const { return m_warningCount; }

int ProblemsPanel::infoCount() const { return m_infoCount; }

int ProblemsPanel::problemCountForFile(const QString &filePath) const {
  const QList<LspDiagnostic> *diagnostics = findDiagnosticsForFile(filePath);
  return diagnostics ? diagnostics->size() : 0;
}

int ProblemsPanel::errorCountForFile(const QString &filePath) const {
  const QList<LspDiagnostic> *diagnostics = findDiagnosticsForFile(filePath);
  if (!diagnostics) {
    return 0;
  }

  int count = 0;
  for (const auto &diag : *diagnostics) {
    if (diag.severity == LspDiagnosticSeverity::Error) {
      count++;
    }
  }
  return count;
}

int ProblemsPanel::warningCountForFile(const QString &filePath) const {
  const QList<LspDiagnostic> *diagnostics = findDiagnosticsForFile(filePath);
  if (!diagnostics) {
    return 0;
  }

  int count = 0;
  for (const auto &diag : *diagnostics) {
    if (diag.severity == LspDiagnosticSeverity::Warning) {
      count++;
    }
  }
  return count;
}

const QList<LspDiagnostic> *
ProblemsPanel::findDiagnosticsForFile(const QString &filePath) const {

  QString normalizedPath = filePath;
  if (normalizedPath.startsWith("file://")) {
    normalizedPath = normalizedPath.mid(7);
  }

  for (auto it = m_diagnostics.constBegin(); it != m_diagnostics.constEnd();
       ++it) {
    QString uri = it.key();
    QString uriPath = uri;
    if (uriPath.startsWith("file://")) {
      uriPath = uriPath.mid(7);
    }

    if (uriPath == normalizedPath || uri == filePath) {
      return &it.value();
    }
  }

  return nullptr;
}

void ProblemsPanel::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);

  if (!item->parent())
    return;

  QString filePath = item->data(0, Qt::UserRole).toString();
  int line = item->data(0, Qt::UserRole + 1).toInt();
  int col = item->data(0, Qt::UserRole + 2).toInt();

  emit problemClicked(filePath, line, col);
}

void ProblemsPanel::onFilterChanged(int index) {
  m_currentFilter = index;
  rebuildTree();
}

void ProblemsPanel::updateCounts() {
  m_errorCount = 0;
  m_warningCount = 0;
  m_infoCount = 0;
  m_hintCount = 0;

  for (const auto &diagList : m_diagnostics) {
    for (const auto &diag : diagList) {
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

  int total = m_errorCount + m_warningCount + m_infoCount + m_hintCount;

  if (total == 0) {
    m_statusLabel->setText(tr("No problems"));
  } else {
    QString errColor = m_theme.errorColor.isValid()
                           ? m_theme.errorColor.name()
                           : QStringLiteral("#f85149");
    QString warnColor = m_theme.warningColor.isValid()
                            ? m_theme.warningColor.name()
                            : QStringLiteral("#d29922");
    QString infoColor = m_theme.accentColor.isValid()
                            ? m_theme.accentColor.name()
                            : QStringLiteral("#58a6ff");
    QString status =
        QString("<span style='color:%1;'>&#x26D4; %2</span>"
                "&nbsp;&nbsp;"
                "<span style='color:%3;'>&#x26A0; %4</span>"
                "&nbsp;&nbsp;"
                "<span style='color:%5;'>&#x2139; %6</span>")
            .arg(errColor)
            .arg(m_errorCount)
            .arg(warnColor)
            .arg(m_warningCount)
            .arg(infoColor)
            .arg(m_infoCount + m_hintCount);
    m_statusLabel->setText(status);
  }

  bool hasProblems = (total > 0);
  if (m_emptyStateLabel) {
    m_emptyStateLabel->setVisible(!hasProblems);
  }
  if (m_tree) {
    m_tree->setVisible(hasProblems);
  }

  emit countsChanged(m_errorCount, m_warningCount, m_infoCount + m_hintCount);
}

void ProblemsPanel::rebuildTree() {
  m_tree->clear();
  updateCounts();

  for (auto it = m_diagnostics.constBegin(); it != m_diagnostics.constEnd();
       ++it) {
    QString uri = it.key();
    const QList<LspDiagnostic> &diagList = it.value();

    QString filePath = uri;
    if (filePath.startsWith("file://")) {
      filePath = filePath.mid(7);
    }
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();

    int fileErrors = 0;
    int fileWarnings = 0;
    int fileInfos = 0;
    int visibleCount = 0;
    for (const auto &diag : diagList) {
      switch (diag.severity) {
      case LspDiagnosticSeverity::Error:
        fileErrors++;
        break;
      case LspDiagnosticSeverity::Warning:
        fileWarnings++;
        break;
      case LspDiagnosticSeverity::Information:
      case LspDiagnosticSeverity::Hint:
        fileInfos++;
        break;
      }

      bool show = (m_currentFilter == 0) ||
                  (m_currentFilter == 1 &&
                   diag.severity == LspDiagnosticSeverity::Error) ||
                  (m_currentFilter == 2 &&
                   diag.severity == LspDiagnosticSeverity::Warning) ||
                  (m_currentFilter == 3 &&
                   (diag.severity == LspDiagnosticSeverity::Information ||
                    diag.severity == LspDiagnosticSeverity::Hint));
      if (show)
        visibleCount++;
    }

    emit fileCountsChanged(filePath, fileErrors, fileWarnings, fileInfos);

    if (visibleCount == 0)
      continue;

    QTreeWidgetItem *fileItem = new QTreeWidgetItem(m_tree);
    fileItem->setText(0, QString("%1 (%2)").arg(fileName).arg(visibleCount));
    fileItem->setToolTip(0, filePath);
    fileItem->setExpanded(true);

    for (const auto &diag : diagList) {
      bool show = (m_currentFilter == 0) ||
                  (m_currentFilter == 1 &&
                   diag.severity == LspDiagnosticSeverity::Error) ||
                  (m_currentFilter == 2 &&
                   diag.severity == LspDiagnosticSeverity::Warning) ||
                  (m_currentFilter == 3 &&
                   (diag.severity == LspDiagnosticSeverity::Information ||
                    diag.severity == LspDiagnosticSeverity::Hint));
      if (!show)
        continue;

      QTreeWidgetItem *diagItem = new QTreeWidgetItem(fileItem);

      QString icon = severityIcon(diag.severity);
      QString message = QString("%1 %2").arg(icon).arg(diag.message);
      diagItem->setText(0, message);

      QString location = QString("[%1:%2]")
                             .arg(diag.range.start.line + 1)
                             .arg(diag.range.start.character + 1);
      diagItem->setText(1, location);

      diagItem->setData(0, Qt::UserRole, filePath);
      diagItem->setData(0, Qt::UserRole + 1, diag.range.start.line);
      diagItem->setData(0, Qt::UserRole + 2, diag.range.start.character);

      QColor color;
      switch (diag.severity) {
      case LspDiagnosticSeverity::Error:
        color = m_theme.errorColor;
        break;
      case LspDiagnosticSeverity::Warning:
        color = m_theme.warningColor;
        break;
      case LspDiagnosticSeverity::Information:
        color = m_theme.accentColor;
        break;
      case LspDiagnosticSeverity::Hint:
        color = m_theme.singleLineCommentFormat;
        break;
      }
      diagItem->setForeground(0, color);
    }
  }
}

QString ProblemsPanel::severityIcon(LspDiagnosticSeverity severity) const {
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

QString ProblemsPanel::severityText(LspDiagnosticSeverity severity) const {
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

bool ProblemsPanel::isAutoRefreshEnabled() const {
  return m_autoRefreshEnabled;
}

void ProblemsPanel::setAutoRefreshEnabled(bool enabled) {
  m_autoRefreshEnabled = enabled;
  if (m_autoRefreshCheckBox) {
    m_autoRefreshCheckBox->setChecked(enabled);
  }
}

void ProblemsPanel::onFileSaved(const QString &filePath) {
  if (m_autoRefreshEnabled) {
    emit refreshRequested(filePath);
  }
}

void ProblemsPanel::onAutoRefreshToggled(bool checked) {
  m_autoRefreshEnabled = checked;
}

void ProblemsPanel::applyTheme(const Theme &theme) {
  m_theme = theme;

  if (m_header) {
    m_header->setStyleSheet(UIStyleHelper::panelHeaderStyle(theme));
  }

  if (m_titleLabel) {
    m_titleLabel->setStyleSheet(UIStyleHelper::titleLabelStyle(theme));
  }

  if (m_filterCombo) {
    m_filterCombo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }

  if (m_autoRefreshCheckBox) {
    m_autoRefreshCheckBox->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  }

  if (m_statusLabel) {
    m_statusLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  }

  if (m_tree) {
    m_tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }
}

void ProblemsPanel::clearCurrentFile() {
  if (!m_currentFilePath.isEmpty()) {
    QString uri = m_currentFilePath;
    if (!uri.startsWith("file://")) {
      uri = "file://" + uri;
    }
    clearFile(uri);
  }
}

void ProblemsPanel::copySelectedMessage() {
  if (!m_tree) {
    return;
  }
  QTreeWidgetItem *item = m_tree->currentItem();
  if (!item) {
    return;
  }
  QString text = item->text(0);
  QApplication::clipboard()->setText(text);
}

void ProblemsPanel::setCurrentFilePath(const QString &filePath) {
  m_currentFilePath = filePath;
}
