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
class Theme;

/**
 * @brief Widget that manages multiple terminal instances with tabs
 *
 * Provides a tabbed interface for managing multiple terminal sessions,
 * with toolbar actions for creating, closing, and managing terminals.
 * Supports split view for side-by-side terminals.
 */
class TerminalTabWidget : public QWidget {
  Q_OBJECT

public:
  explicit TerminalTabWidget(QWidget *parent = nullptr);
  ~TerminalTabWidget();

  /**
   * @brief Create and add a new terminal tab
   * @param workingDirectory Optional working directory for the new terminal
   * @return Pointer to the newly created Terminal
   */
  Terminal *addNewTerminal(const QString &workingDirectory = QString());

  /**
   * @brief Create and add a new terminal with specific shell profile
   * @param profileName Name of the shell profile to use
   * @param workingDirectory Optional working directory
   * @return Pointer to the newly created Terminal
   */
  Terminal *
  addNewTerminalWithProfile(const QString &profileName,
                            const QString &workingDirectory = QString());

  /**
   * @brief Get the currently active terminal
   * @return Pointer to current Terminal, or nullptr if none
   */
  Terminal *currentTerminal();

  /**
   * @brief Get terminal at specified index
   * @param index Tab index
   * @return Pointer to Terminal at index, or nullptr if invalid
   */
  Terminal *terminalAt(int index);

  /**
   * @brief Get number of terminal tabs
   * @return Count of terminals
   */
  int terminalCount() const;

  /**
   * @brief Close the terminal at specified index
   * @param index Tab index to close
   */
  void closeTerminal(int index);

  /**
   * @brief Close all terminal tabs
   */
  void closeAllTerminals();

  /**
   * @brief Clear the current terminal's output
   */
  void clearCurrentTerminal();

  /**
   * @brief Run a file using the run template system
   * @param filePath Path to the file to run
   * @return true if a command was started
   */
  bool runFile(const QString &filePath);

  /**
   * @brief Stop any running process in the current terminal
   */
  void stopCurrentProcess();

  /**
   * @brief Set the working directory for the current terminal
   * @param directory Working directory path
   */
  void setWorkingDirectory(const QString &directory);

  /**
   * @brief Apply theme to terminal widget
   * @param theme Theme to apply
   */
  void applyTheme(const Theme &theme);

  /**
   * @brief Send text to the current terminal
   * @param text Text to send
   * @param appendNewline Whether to append newline
   */
  void sendTextToTerminal(const QString &text, bool appendNewline = false);

  /**
   * @brief Split the terminal view horizontally
   */
  void splitHorizontal();

  /**
   * @brief Check if the view is split
   * @return true if split view is active
   */
  bool isSplit() const;

  /**
   * @brief Unsplit the terminal view (keep current terminal)
   */
  void unsplit();

  /**
   * @brief Get available shell profile names
   * @return List of profile names
   */
  QStringList availableShellProfiles() const;

signals:
  /**
   * @brief Emitted when the close button is clicked
   */
  void closeRequested();

  /**
   * @brief Emitted when a process starts
   */
  void processStarted();

  /**
   * @brief Emitted when a process finishes
   * @param exitCode Exit code of the process
   */
  void processFinished(int exitCode);

  /**
   * @brief Emitted when an error occurs
   * @param errorMessage Error message
   */
  void errorOccurred(const QString &errorMessage);

  /**
   * @brief Emitted when a link is clicked in a terminal
   * @param link The clicked link
   */
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

#endif // TERMINALTABWIDGET_H
