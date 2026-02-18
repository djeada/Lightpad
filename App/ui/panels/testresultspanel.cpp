#include "testresultspanel.h"
#include "../uistylehelper.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>

TestResultsPanel::TestResultsPanel(QWidget *parent)
    : QWidget(parent), m_header(nullptr), m_titleLabel(nullptr),
      m_statusLabel(nullptr), m_filterCombo(nullptr),
      m_rerunFailedButton(nullptr), m_copySummaryButton(nullptr),
      m_tree(nullptr), m_passedCount(0), m_failedCount(0), m_totalCount(0),
      m_exitCode(0), m_noTestsFound(false), m_filterMode(0) {
  setupUi();
}

TestResultsPanel::~TestResultsPanel() {}

void TestResultsPanel::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  m_header = new QWidget(this);
  QHBoxLayout *headerLayout = new QHBoxLayout(m_header);
  headerLayout->setContentsMargins(8, 4, 8, 4);

  m_titleLabel = new QLabel(tr("Test Results"), m_header);
  m_titleLabel->setStyleSheet("font-weight: bold; color: #e6edf3;");
  headerLayout->addWidget(m_titleLabel);

  m_filterCombo = new QComboBox(m_header);
  m_filterCombo->addItem(tr("All"));
  m_filterCombo->addItem(tr("Failed"));
  m_filterCombo->addItem(tr("Passed"));
  connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &TestResultsPanel::onFilterChanged);
  headerLayout->addWidget(m_filterCombo);

  m_rerunFailedButton = new QPushButton(tr("Rerun Failed"), m_header);
  connect(m_rerunFailedButton, &QPushButton::clicked, this,
          &TestResultsPanel::onRerunFailedClicked);
  headerLayout->addWidget(m_rerunFailedButton);

  m_copySummaryButton = new QPushButton(tr("Copy Summary"), m_header);
  connect(m_copySummaryButton, &QPushButton::clicked, this,
          &TestResultsPanel::onCopySummaryClicked);
  headerLayout->addWidget(m_copySummaryButton);

  headerLayout->addStretch();

  m_statusLabel = new QLabel(m_header);
  headerLayout->addWidget(m_statusLabel);

  mainLayout->addWidget(m_header);

  m_tree = new QTreeWidget(this);
  m_tree->setHeaderLabels({tr("Test"), tr("Status"), tr("Time")});
  m_tree->setRootIsDecorated(false);
  m_tree->setAlternatingRowColors(true);
  m_tree->setUniformRowHeights(true);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  mainLayout->addWidget(m_tree);
}

void TestResultsPanel::setResults(const QList<TestCaseResult> &results,
                                  int passed, int failed, int total,
                                  const QString &durationSec, int exitCode,
                                  bool noTestsFound) {
  m_results = results;
  m_passedCount = passed;
  m_failedCount = failed;
  m_totalCount = total;
  m_durationSec = durationSec;
  m_exitCode = exitCode;
  m_noTestsFound = noTestsFound;

  m_rerunFailedButton->setEnabled(m_failedCount > 0);
  m_statusLabel->setText(summaryText());
  rebuildTree();
}

void TestResultsPanel::clear() {
  m_results.clear();
  m_passedCount = 0;
  m_failedCount = 0;
  m_totalCount = 0;
  m_durationSec.clear();
  m_exitCode = 0;
  m_noTestsFound = false;
  m_statusLabel->clear();
  m_tree->clear();
  m_rerunFailedButton->setEnabled(false);
}

void TestResultsPanel::onFilterChanged(int index) {
  m_filterMode = index;
  rebuildTree();
}

void TestResultsPanel::onRerunFailedClicked() {
  QStringList failedTests;
  for (const TestCaseResult &result : m_results) {
    if (!result.passed) {
      failedTests.append(result.name);
    }
  }
  if (!failedTests.isEmpty()) {
    emit rerunFailedRequested(failedTests);
  }
}

void TestResultsPanel::onCopySummaryClicked() {
  QApplication::clipboard()->setText(summaryText());
}

void TestResultsPanel::rebuildTree() {
  m_tree->clear();

  if (m_noTestsFound) {
    QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
    item->setText(0, tr("No tests were found"));
    item->setText(1, tr("Info"));
    item->setText(2, "-");
    return;
  }

  for (const TestCaseResult &result : m_results) {
    if (m_filterMode == 1 && result.passed) {
      continue;
    }
    if (m_filterMode == 2 && !result.passed) {
      continue;
    }

    QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
    item->setText(0, result.name);
    item->setText(1, result.passed ? tr("Passed") : tr("Failed"));
    item->setText(2, result.timeSec.isEmpty() ? "-" : result.timeSec + " sec");
    item->setForeground(1, result.passed ? m_theme.successColor : m_theme.errorColor);
  }
}

QString TestResultsPanel::summaryText() const {
  if (m_noTestsFound) {
    return tr("No tests found");
  }

  QString summary = QString("Passed: %1/%2  Failed: %3")
                        .arg(m_passedCount)
                        .arg(m_totalCount)
                        .arg(m_failedCount);
  if (!m_durationSec.isEmpty()) {
    summary.append(QString("  Time: %1 sec").arg(m_durationSec));
  }
  if (m_exitCode != 0) {
    summary.append(QString("  Exit: %1").arg(m_exitCode));
  }
  return summary;
}

void TestResultsPanel::applyTheme(const Theme &theme) {
  m_theme = theme;

  if (m_header) {
    m_header->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;")
            .arg(theme.surfaceColor.name())
            .arg(theme.borderColor.name()));
  }
  if (m_titleLabel) {
    m_titleLabel->setStyleSheet(UIStyleHelper::titleLabelStyle(theme));
  }
  if (m_statusLabel) {
    m_statusLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  }
  if (m_filterCombo) {
    m_filterCombo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }
  if (m_rerunFailedButton) {
    m_rerunFailedButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  }
  if (m_copySummaryButton) {
    m_copySummaryButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  }
  if (m_tree) {
    m_tree->setStyleSheet(UIStyleHelper::treeWidgetStyle(theme));
  }

  m_statusLabel->setText(summaryText());
  rebuildTree();
}
