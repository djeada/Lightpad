#ifndef GITDIFFDIALOG_H
#define GITDIFFDIALOG_H

#include "../../settings/theme.h"
#include <QDialog>
#include <QPointer>

class GitIntegration;
class QTextEdit;
class QLabel;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLineEdit;
class GitDiffDialog : public QDialog {
  Q_OBJECT

public:
  enum class DiffTarget { File, Commit };

  enum class DiffViewMode { Unified, Split, Word };

  GitDiffDialog(GitIntegration *git, const QString &targetId, DiffTarget target,
                bool staged, const Theme &theme, QWidget *parent = nullptr);

  void setDiffText(const QString &diffText);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void onModeChanged(int index);
  void onWrapToggled(bool enabled);
  void onPrevChange();
  void onNextChange();
  void onCopy();

private:
  void buildUi();
  void applyTheme(const Theme &theme);
  void updateDiffPresentation();
  void rebuildUnified();
  void rebuildSplit();
  void rebuildWord();
  void parseDiff(const QString &diffText);
  void collectChangeBlocks();
  void scrollToChange(int index);
  void updateChangeCounter();
  void performSearch(bool backwards);
  QString resolveWordDiff();
  QString htmlEscape(const QString &text) const;
  QString buildWordDiffLine(const QString &line) const;
  QString styleToken(const QString &token, const QString &cssClass) const;

  struct DiffLine {
    QChar prefix;
    QString content;
  };

  GitIntegration *m_git;
  QString m_targetId;
  DiffTarget m_target;
  bool m_staged;
  QString m_diffText;
  QString m_wordDiffText;
  QString m_summaryText;
  DiffViewMode m_viewMode;

  QTextEdit *m_diffView;
  QLabel *m_summaryLabel;
  QLabel *m_changeCounterLabel;
  QComboBox *m_modeSelector;
  QCheckBox *m_wrapToggle;
  QLineEdit *m_searchField;
  QPushButton *m_findPrevButton;
  QPushButton *m_findNextButton;
  QPushButton *m_prevButton;
  QPushButton *m_nextButton;
  QPushButton *m_copyButton;

  QList<DiffLine> m_lines;
  QList<QPair<int, int>> m_changeBlocks;
  int m_currentChange;
  int m_addedCount;
  int m_deletedCount;
  int m_totalAdded;
  int m_totalDeleted;
  Theme m_theme;
};

#endif // GITDIFFDIALOG_H
