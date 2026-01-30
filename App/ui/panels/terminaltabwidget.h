#ifndef TERMINALTABWIDGET_H
#define TERMINALTABWIDGET_H

#include <QWidget>
#include <QList>

class Terminal;
class QTabWidget;
class QToolButton;
class QHBoxLayout;
class Theme;

/**
 * @brief Widget that manages multiple terminal instances with tabs
 * 
 * Provides a tabbed interface for managing multiple terminal sessions,
 * with toolbar actions for creating, closing, and managing terminals.
 */
class TerminalTabWidget : public QWidget {
    Q_OBJECT

public:
    explicit TerminalTabWidget(QWidget* parent = nullptr);
    ~TerminalTabWidget();

    /**
     * @brief Create and add a new terminal tab
     * @param workingDirectory Optional working directory for the new terminal
     * @return Pointer to the newly created Terminal
     */
    Terminal* addNewTerminal(const QString& workingDirectory = QString());

    /**
     * @brief Get the currently active terminal
     * @return Pointer to current Terminal, or nullptr if none
     */
    Terminal* currentTerminal();

    /**
     * @brief Get terminal at specified index
     * @param index Tab index
     * @return Pointer to Terminal at index, or nullptr if invalid
     */
    Terminal* terminalAt(int index);

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
    bool runFile(const QString& filePath);

    /**
     * @brief Stop any running process in the current terminal
     */
    void stopCurrentProcess();

    /**
     * @brief Set the working directory for the current terminal
     * @param directory Working directory path
     */
    void setWorkingDirectory(const QString& directory);

    /**
     * @brief Apply theme to terminal widget
     * @param theme Theme to apply
     */
    void applyTheme(const Theme& theme);

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
    void errorOccurred(const QString& errorMessage);

private slots:
    void onNewTerminalClicked();
    void onCloseTerminalClicked();
    void onClearTerminalClicked();
    void onCloseButtonClicked();
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);

private:
    void setupUI();
    void setupToolbar();
    void updateTabTitle(int index);
    QString generateTerminalName();

    QTabWidget* m_tabWidget;
    QToolButton* m_newTerminalButton;
    QToolButton* m_clearButton;
    QToolButton* m_closeAllButton;
    QToolButton* m_closeButton;
    int m_terminalCounter;
    QString m_currentWorkingDirectory;
};

#endif // TERMINALTABWIDGET_H
