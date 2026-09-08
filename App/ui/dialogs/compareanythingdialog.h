#ifndef COMPAREANYTHINGDIALOG_H
#define COMPAREANYTHINGDIALOG_H

#include "../../git/gitcomparison.h"
#include "styleddialog.h"

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class CompareAnythingDialog : public StyledDialog {
  Q_OBJECT

public:
  CompareAnythingDialog(GitIntegration *git, const Theme &theme,
                        QWidget *parent = nullptr);

  void setEndpoints(const GitCompareEndpoint &base,
                    const GitCompareEndpoint &compare);

  void compareWithWorkingTree(const QString &ref);

  void selectFile(const QString &filePath);

  const GitComparisonResult &result() const { return m_result; }

signals:
  void fileOpenRequested(const QString &filePath);

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void runComparison();
  void onSwapClicked();
  void onPresetChosen(int index);
  void onRecentChosen(int index);
  void onFileSelectionChanged();

private:
  void buildUi();
  void buildEndpointBar(QVBoxLayout *layout);
  void buildResultArea(QVBoxLayout *layout);
  void populateEndpointCombo(QComboBox *combo);
  void populatePresets();
  void populateRecents();
  void rememberComparison(const GitCompareEndpoint &base,
                          const GitCompareEndpoint &compare);
  GitCompareEndpoint endpointFrom(QComboBox *combo) const;
  void selectEndpoint(QComboBox *combo, const GitCompareEndpoint &endpoint);
  void showDiffFor(const QString &filePath);
  void renderDiffLines(const QString &diffText);
  void updateSummary();

  GitIntegration *m_git;
  GitComparisonResult m_result;

  QComboBox *m_presetCombo;
  QComboBox *m_baseCombo;
  QComboBox *m_compareCombo;
  QComboBox *m_recentCombo;
  QPushButton *m_swapButton;
  QPushButton *m_compareButton;

  QLabel *m_summaryLabel;
  QLabel *m_mergeBaseLabel;
  QTreeWidget *m_fileTree;
  QListWidget *m_diffView;
  QTreeWidget *m_onlyBaseTree;
  QTreeWidget *m_onlyCompareTree;
  QLabel *m_onlyBaseLabel;
  QLabel *m_onlyCompareLabel;

  bool m_populating;
};

#endif
