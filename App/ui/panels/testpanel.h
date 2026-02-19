#ifndef TESTPANEL_H
#define TESTPANEL_H

#include "../../settings/theme.h"
#include "../../test_templates/testconfiguration.h"
#include "../../test_templates/testdiscovery.h"
#include "../../test_templates/testrunmanager.h"
#include <QAction>
#include <QComboBox>
#include <QLabel>
#include <QSplitter>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

class TestPanel : public QWidget {
  Q_OBJECT

public:
  explicit TestPanel(QWidget *parent = nullptr);
  ~TestPanel();

  void applyTheme(const Theme &theme);
  void setWorkspaceFolder(const QString &folder);

  void setDiscoveryAdapter(ITestDiscoveryAdapter *adapter);

  int passedCount() const { return m_passedCount; }
  int failedCount() const { return m_failedCount; }
  int skippedCount() const { return m_skippedCount; }
  int erroredCount() const { return m_erroredCount; }

  void saveState() const;
  void restoreState();

signals:
  void locationClicked(const QString &filePath, int line, int column);
  void countsChanged(int passed, int failed, int skipped, int errored);

public slots:
  void runAll();
  void runFailed();
  void runCurrentFile(const QString &filePath);
  void runTestsForPath(const QString &path);
  bool runWithConfigurationId(const QString &configId,
                              const QString &filePath = QString());
  void stopTests();
  void discoverTests();

private slots:
  void onTestStarted(const TestResult &result);
  void onTestFinished(const TestResult &result);
  void onRunStarted();
  void onRunFinished(int passed, int failed, int skipped, int errored);
  void onItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onItemClicked(QTreeWidgetItem *item, int column);
  void onFilterChanged(int index);
  void onConfigChanged(int index);
  void onContextMenu(const QPoint &pos);
  void onDiscoveryFinished(const QList<DiscoveredTest> &tests);
  void onDiscoveryError(const QString &message);

private:
  void setupUI();
  void updateStatusLabel();
  void updateTreeItemIcon(QTreeWidgetItem *item, TestStatus status);
  QTreeWidgetItem *findOrCreateSuiteItem(const QString &suite);
  QTreeWidgetItem *findTestItem(const QString &id);
  void applyFilter();
  void refreshConfigurations();
  TestConfiguration currentConfiguration() const;
  void populateTreeFromDiscovery(const QList<DiscoveredTest> &tests);
  void connectDiscoveryAdapter();

  QToolBar *m_toolbar;
  QAction *m_runAllAction;
  QAction *m_runFailedAction;
  QAction *m_stopAction;
  QAction *m_clearAction;
  QAction *m_discoverAction;
  QComboBox *m_filterCombo;
  QComboBox *m_configCombo;

  QSplitter *m_splitter;
  QTreeWidget *m_tree;
  QTextEdit *m_detailPane;
  QLabel *m_statusLabel;

  TestRunManager *m_runManager;
  ITestDiscoveryAdapter *m_discoveryAdapter = nullptr;
  bool m_ownsDiscoveryAdapter = false;
  QString m_workspaceFolder;
  Theme m_theme;

  int m_passedCount = 0;
  int m_failedCount = 0;
  int m_skippedCount = 0;
  int m_erroredCount = 0;

  QMap<QString, QTreeWidgetItem *> m_suiteItems;
  QMap<QString, QTreeWidgetItem *> m_testItems;
  QMap<QString, TestResult> m_testResults;
};

#endif
