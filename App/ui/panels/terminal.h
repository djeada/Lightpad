#ifndef TERMINAL_H
#define TERMINAL_H

#include <QMap>
#include <QProcess>
#include <QStringList>
#include <QWidget>

namespace Ui {
class Terminal;
}

class QEvent;

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
     * @brief Execute a command with the given arguments
     * @param command The command to execute
     * @param args Arguments for the command
     * @param workingDirectory Working directory for execution
     * @param env Environment variables (optional)
     */
    void executeCommand(const QString& command,
                        const QStringList& args,
                        const QString& workingDirectory,
                        const QMap<QString, QString>& env = QMap<QString, QString>());

    /**
     * @brief Execute the run template for a file
     * @param filePath Path to the file to run
     * @return true if a command was started
     */
    bool runFile(const QString& filePath);

    /**
     * @brief Stop the currently running process
     */
    void stopProcess();

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

    /**
     * @brief Apply theme colors to the terminal
     * @param backgroundColor Background color
     * @param textColor Text color
     * @param errorColor Error text color
     */
    void applyTheme(const QString& backgroundColor, 
                    const QString& textColor,
                    const QString& errorColor = QString());

signals:
    void processStarted();
    void processFinished(int exitCode);
    void processError(const QString& errorMessage);

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

    void onRunProcessReadyReadStdout();
    void onRunProcessReadyReadStderr();
    void onRunProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onRunProcessError(QProcess::ProcessError error);

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
    void cleanupRunProcess(bool restartShell);
    void updateStyleSheet();

    Ui::Terminal* ui;
    QProcess* m_process;
    QProcess* m_runProcess;
    QString m_workingDirectory;
    QString m_currentInput;
    QStringList m_commandHistory;
    int m_historyIndex;
    bool m_processRunning;
    bool m_restartShellAfterRun;
    
    // Theme colors
    QString m_backgroundColor;
    QString m_textColor;
    QString m_errorColor;
};

#endif // TERMINAL_H
