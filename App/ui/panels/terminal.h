#ifndef TERMINAL_H
#define TERMINAL_H

#include <QWidget>
#include <QProcess>
#include <QMap>
#include <QString>

namespace Ui {
class Terminal;
}

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
     * @brief Check if a process is currently running
     */
    bool isRunning() const;

signals:
    void processStarted();
    void processFinished(int exitCode);
    void processError(const QString& errorMessage);

private slots:
    void on_closeButton_clicked();
    void onProcessReadyReadStdout();
    void onProcessReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void setupTextEdit();
    void appendOutput(const QString& text, bool isError = false);
    void clear();
    
    Ui::Terminal* ui;
    QProcess* m_process;
};

#endif // TERMINAL_H
