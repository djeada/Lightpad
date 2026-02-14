#ifndef TERMINALTABWIDGET_H
#define TERMINALTABWIDGET_H

#include <QList>
#include <QWidget>

class Terminal;
class QTabWidget;
class QToolButton;
class QHBoxLayout;
class QSplitter;
class QMenu;
class QComboBox;
struct Theme;

class TerminalTabWidget : public QWidget {
  Q_OBJECT

public:
  explicit TerminalTabWidget(QWidget *parent = nullptr);
  ~TerminalTabWidget();

  Terminal *addNewTerminal(const QString &workingDirectory = QString());

  Terminal *
  addNewTerminalWithProfile(const QString &profileName,
                            const QString &workingDirectory = QString());

  Terminal *currentTerminal();

  Terminal *terminalAt(int index);

  int terminalCount() const;

  void closeTerminal(int index);

  void closeAllTerminals();

  void clearCurrentTerminal();

  bool runFile(const QString &filePath, const QString &languageId = QString());

  void stopCurrentProcess();

  void setWorkingDirectory(const QString &directory);

  void applyTheme(const Theme &theme);

  void sendTextToTerminal(const QString &text, bool appendNewline = false);

  void splitHorizontal();

  bool isSplit() const;

  void unsplit();

  QStringList availableShellProfiles() const;

signals:

  void closeRequested();

  void processStarted();

  void processFinished(int exitCode);

  void errorOccurred(const QString &errorMessage);

  void linkClicked(const QString &link);

private slots:
  void onNewTerminalClicked();
  void onClearTerminalClicked();
  void onCloseButtonClicked();
  void onTabCloseRequested(int index);
  void onCurrentTabChanged(int index);
  void onSplitTerminalClicked();
  void onShellProfileSelected(const QString &profileName);
  void onTerminalLinkClicked(const QString &link);

private:
  void setupUI();
  void setupToolbar();
  void setupShellProfileMenu();
  QString generateTerminalName();
  QString generateTerminalName(const QString &profileName);

  QSplitter *m_splitter;
  QTabWidget *m_tabWidget;
  QTabWidget *m_secondaryTabWidget;
  QToolButton *m_newTerminalButton;
  QToolButton *m_clearButton;
  QToolButton *m_splitButton;
  QToolButton *m_closeButton;
  QMenu *m_shellProfileMenu;
  int m_terminalCounter;
  QString m_currentWorkingDirectory;
  bool m_isSplit;
};

#endif
