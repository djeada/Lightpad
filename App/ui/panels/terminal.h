#ifndef TERMINAL_H
#define TERMINAL_H

#include <QWidget>
#include <QProcess>
#include <QKeyEvent>

namespace Ui {
class Terminal;
}

/**
 * @brief Terminal widget providing full shell interaction using QProcess
 * 
 * Provides an embedded terminal that spawns a shell process and allows
 * interactive command execution with real-time output display.
 */
class Terminal : public QWidget {
    Q_OBJECT

public:
    explicit Terminal(QWidget* parent = nullptr);
    ~Terminal();

    /**
     * @brief Start the terminal shell process
     * @param workingDirectory Optional working directory for the shell
     * @return true if shell started successfully
     */
    bool startShell(const QString& workingDirectory = QString());

    /**
     * @brief Stop the terminal shell process
     */
    void stopShell();

    /**
     * @brief Check if shell process is running
     * @return true if shell is running
     */
    bool isRunning() const;

    /**
     * @brief Execute a command in the terminal
     * @param command Command to execute
     */
    void executeCommand(const QString& command);

    /**
     * @brief Set the working directory for the shell
     * @param directory Working directory path
     */
    void setWorkingDirectory(const QString& directory);

    /**
     * @brief Clear the terminal output
     */
    void clear();

signals:
    /**
     * @brief Emitted when the shell process starts
     */
    void shellStarted();

    /**
     * @brief Emitted when the shell process finishes
     * @param exitCode Exit code of the shell
     */
    void shellFinished(int exitCode);

    /**
     * @brief Emitted when an error occurs
     * @param message Error message
     */
    void errorOccurred(const QString& message);

    /**
     * @brief Emitted when output is available
     * @param output Output text
     */
    void outputReceived(const QString& output);

private slots:
    void on_closeButton_clicked();
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onInputSubmitted();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupTerminal();
    void appendOutput(const QString& text, bool isError = false);
    void appendPrompt();
    QString getShellCommand() const;
    QStringList getShellArguments() const;
    void scrollToBottom();
    void handleHistoryNavigation(bool up);

    Ui::Terminal* ui;
    QProcess* m_process;
    QString m_workingDirectory;
    QString m_currentInput;
    QStringList m_commandHistory;
    int m_historyIndex;
    bool m_processRunning;
};

#endif // TERMINAL_H
