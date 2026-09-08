#ifndef COMMANDMIRRORDIALOG_H
#define COMMANDMIRRORDIALOG_H

#include "../../git/gitcommandlog.h"
#include "styleddialog.h"

class GitIntegration;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

class CommandMirrorDialog : public StyledDialog {
  Q_OBJECT

public:
  CommandMirrorDialog(GitIntegration *git, const Theme &theme,
                      QWidget *parent = nullptr);

  int recordCount() const;
  GitCommandMirrorMode mode() const;

protected:
  void applyTheme(const Theme &theme) override;

private slots:
  void onCommandExecuted(const GitCommandRecord &record);
  void onSearchChanged(const QString &text);
  void onSelectionChanged();
  void onModeChanged(int index);
  void onCopy();
  void onClear();

private:
  void buildUi();
  void addRecord(const GitCommandRecord &record);
  const GitCommandRecord *currentRecord() const;

  GitIntegration *m_git;
  QList<GitCommandRecord> m_records;

  QComboBox *m_modeCombo;
  QLineEdit *m_searchEdit;
  QTreeWidget *m_commandTree;
  QLabel *m_explanationLabel;
  QLabel *m_riskLabel;
  QPlainTextEdit *m_outputView;
  QPushButton *m_copyButton;
  QPushButton *m_clearButton;
  QPushButton *m_closeButton;
};

#endif
