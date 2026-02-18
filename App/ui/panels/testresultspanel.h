#ifndef TESTRESULTSPANEL_H
#define TESTRESULTSPANEL_H

#include "../../settings/theme.h"
#include <QComboBox>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

struct TestCaseResult {
  QString name;
  bool passed = false;
  QString timeSec;
};

class TestResultsPanel : public QWidget {
  Q_OBJECT

public:
  explicit TestResultsPanel(QWidget *parent = nullptr);
  ~TestResultsPanel();

  void setResults(const QList<TestCaseResult> &results, int passed, int failed,
                  int total, const QString &durationSec, int exitCode,
                  bool noTestsFound);
  void applyTheme(const Theme &theme);
  void clear();

signals:
  void rerunFailedRequested(const QStringList &failedTests);

private slots:
  void onFilterChanged(int index);
  void onRerunFailedClicked();
  void onCopySummaryClicked();

private:
  void setupUi();
  void rebuildTree();
  QString summaryText() const;

  QWidget *m_header;
  QLabel *m_titleLabel;
  QLabel *m_statusLabel;
  QComboBox *m_filterCombo;
  QPushButton *m_rerunFailedButton;
  QPushButton *m_copySummaryButton;
  QTreeWidget *m_tree;

  QList<TestCaseResult> m_results;
  int m_passedCount;
  int m_failedCount;
  int m_totalCount;
  QString m_durationSec;
  int m_exitCode;
  bool m_noTestsFound;
  int m_filterMode;
  Theme m_theme;
};

#endif
