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
class QListWidget;
class QSplitter;
class QFrame;
class QScrollArea;

class GitDiffDialog : public QDialog {
  Q_OBJECT

public:
  enum class DiffTarget { File, Commit };

  enum class DiffViewMode { Unified, Split, Word };

  GitDiffDialog(GitIntegration *git, const QString &targetId, DiffTarget target,
                bool staged, const Theme &theme, QWidget *parent = nullptr);

  void setDiffText(const QString &diffText);
  void setCommitInfo(const QString &author, const QString &date,
                     const QString &message);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onModeChanged(int index);
  void onWrapToggled(bool enabled);
  void onPrevChange();
  void onNextChange();
  void onCopy();
  void onFileSelected(int index);

private:
  void buildUi();
  void applyTheme(const Theme &theme);
  void updateDiffPresentation();
  void rebuildUnified();
  void rebuildSplit();
  void rebuildWord();
  void parseDiff(const QString &diffText);
  void parseFiles();
  void collectChangeBlocks();
  void scrollToChange(int index);
  void scrollToFile(int fileIndex);
  void updateChangeCounter();
  void updateSearchCounter();
  void updateMinimap();
  void performSearch(bool backwards);
  int countSearchMatches(const QString &query);
  QString resolveWordDiff();
  QString htmlEscape(const QString &text) const;
  QString buildWordDiffLine(const QString &line) const;
  QString styleToken(const QString &token, const QString &cssClass) const;
  QString buildLineNumberGutter(int oldLine, int newLine, QChar prefix) const;

  struct DiffLine {
    QChar prefix;
    QString content;
    int oldLineNum;
    int newLineNum;
  };

  struct FileSection {
    QString filename;
    int startLine;
    int addedCount;
    int deletedCount;
  };

  GitIntegration *m_git;
  QString m_targetId;
  DiffTarget m_target;
  bool m_staged;
  QString m_diffText;
  QString m_wordDiffText;
  QString m_summaryText;
  DiffViewMode m_viewMode;

  // Commit info
  QString m_commitAuthor;
  QString m_commitDate;
  QString m_commitMessage;

  // Main layout widgets
  QSplitter *m_mainSplitter;
  QWidget *m_fileListPanel;
  QListWidget *m_fileList;
  QLabel *m_fileListHeader;

  // Diff view area
  QTextEdit *m_diffView;
  QFrame *m_minimapFrame;
  QLabel *m_minimapLabel;

  // Header widgets
  QLabel *m_summaryLabel;
  QLabel *m_commitInfoLabel;
  QLabel *m_changeCounterLabel;
  QLabel *m_searchCounterLabel;
  QFrame *m_toolbarSeparator1;
  QFrame *m_toolbarSeparator2;
  QComboBox *m_modeSelector;
  QCheckBox *m_wrapToggle;
  QLineEdit *m_searchField;
  QPushButton *m_findPrevButton;
  QPushButton *m_findNextButton;
  QPushButton *m_prevButton;
  QPushButton *m_nextButton;
  QPushButton *m_copyButton;

  // Data
  QList<DiffLine> m_lines;
  QList<FileSection> m_files;
  QList<QPair<int, int>> m_changeBlocks;
  int m_currentChange;
  int m_addedCount;
  int m_deletedCount;
  int m_totalAdded;
  int m_totalDeleted;
  int m_currentSearchMatch;
  int m_totalSearchMatches;
  Theme m_theme;
};

#endif // GITDIFFDIALOG_H
