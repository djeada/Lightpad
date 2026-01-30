#include "terminal.h"
#include "ui_terminal.h"
#include "../../run_templates/runtemplatemanager.h"

#include <QColor>
#include <QDir>
#include <QFont>
#include <QKeyEvent>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QStyle>
#include <QTextCharFormat>
#include <QTextCursor>

Terminal::Terminal(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Terminal)
    , m_process(nullptr)
    , m_runProcess(nullptr)
    , m_historyIndex(0)
    , m_processRunning(false)
    , m_restartShellAfterRun(false)
    , m_backgroundColor("#0e1116")
    , m_textColor("#e6edf3")
    , m_errorColor("#f44336")
{
    ui->setupUi(this);
    ui->closeButton->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    setupTerminal();
}

Terminal::~Terminal()
{
    cleanupRunProcess(false);
    stopShell();
    delete ui;
}

void Terminal::setupTerminal()
{
    // Setup the text edit for terminal display
    ui->textEdit->setReadOnly(false);
    ui->textEdit->setTextInteractionFlags(Qt::TextEditorInteraction);
    ui->textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

    // Set monospace font for terminal
    QFont monoFont("Monospace");
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(10);
    ui->textEdit->setFont(monoFont);

    // Install event filter for key handling
    ui->textEdit->installEventFilter(this);

    // Apply terminal styling
    updateStyleSheet();

    // Display initial prompt
    appendOutput("Terminal ready. Starting shell...\n");

    // Start shell automatically
    startShell();
}

bool Terminal::startShell(const QString& workingDirectory)
{
    if (m_process && m_processRunning) {
        return true;  // Already running
    }

    if (!m_process) {
        m_process = new QProcess(this);

        connect(m_process, &QProcess::readyReadStandardOutput,
                this, &Terminal::onReadyReadStandardOutput);
        connect(m_process, &QProcess::readyReadStandardError,
                this, &Terminal::onReadyReadStandardError);
        connect(m_process, &QProcess::errorOccurred,
                this, &Terminal::onProcessError);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Terminal::onProcessFinished);
    }

    // Set working directory
    if (!workingDirectory.isEmpty()) {
        m_workingDirectory = workingDirectory;
    }

    if (m_workingDirectory.isEmpty()) {
        m_workingDirectory = QDir::homePath();
    }

    // Validate working directory
    if (!m_workingDirectory.isEmpty() && !QDir(m_workingDirectory).exists()) {
        appendOutput(QString("Warning: Directory '%1' does not exist, using home directory.\n")
                    .arg(m_workingDirectory), true);
        m_workingDirectory = QDir::homePath();
    }

    m_process->setWorkingDirectory(m_workingDirectory);

    // Set environment for shell compatibility
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TERM", "xterm");  // More compatible terminal type
    m_process->setProcessEnvironment(env);

    // Get shell command based on platform
    QString shell = getShellCommand();
    QStringList args = getShellArguments();

    m_process->setProgram(shell);
    m_process->setArguments(args);
    m_process->start();

    if (!m_process->waitForStarted(5000)) {
        appendOutput("Error: Failed to start shell process.\n", true);
        emit errorOccurred("Failed to start shell");
        return false;
    }

    m_processRunning = true;
    ui->textEdit->setReadOnly(false);
    appendOutput(QString("Shell started: %1\n").arg(shell));
    emit shellStarted();

    return true;
}

void Terminal::stopShell()
{
    if (!m_process) {
        return;
    }

    m_processRunning = false;

    // Try graceful termination
    m_process->terminate();
    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    delete m_process;
    m_process = nullptr;
}

bool Terminal::isRunning() const
{
    return m_processRunning && m_process &&
           m_process->state() == QProcess::Running;
}

void Terminal::executeCommand(const QString& command)
{
    if (!isRunning()) {
        appendOutput("Error: Shell not running. Restarting...\n", true);
        if (!startShell()) {
            return;
        }
    }

    // Add to history (avoid duplicates)
    if (!command.trimmed().isEmpty()) {
        if (m_commandHistory.isEmpty() || m_commandHistory.last() != command) {
            m_commandHistory.append(command);
        }
        m_historyIndex = m_commandHistory.size();
    }

    // Write command to process (use UTF-8 for better Unicode support)
    QString cmdWithNewline = command + "\n";
    m_process->write(cmdWithNewline.toUtf8());
}

void Terminal::setWorkingDirectory(const QString& directory)
{
    m_workingDirectory = directory;
    if (isRunning()) {
        // Change directory in running shell
        executeCommand(QString("cd \"%1\"").arg(directory));
    }
}

void Terminal::clear()
{
    ui->textEdit->clear();
    if (isRunning() && !(m_runProcess && m_runProcess->state() != QProcess::NotRunning)) {
        appendPrompt();
    }
}

void Terminal::executeCommand(const QString& command,
                              const QStringList& args,
                              const QString& workingDirectory,
                              const QMap<QString, QString>& env)
{
    // Stop any existing run process
    cleanupRunProcess(false);

    bool wasShellRunning = isRunning();
    if (wasShellRunning) {
        stopShell();
    }
    m_restartShellAfterRun = wasShellRunning;

    if (!workingDirectory.isEmpty()) {
        m_workingDirectory = workingDirectory;
    }

    // Create new process for run template execution
    m_runProcess = new QProcess(this);

    connect(m_runProcess, &QProcess::readyReadStandardOutput,
            this, &Terminal::onRunProcessReadyReadStdout);
    connect(m_runProcess, &QProcess::readyReadStandardError,
            this, &Terminal::onRunProcessReadyReadStderr);
    connect(m_runProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Terminal::onRunProcessFinished);
    connect(m_runProcess, &QProcess::errorOccurred,
            this, &Terminal::onRunProcessError);

    // Set working directory
    m_runProcess->setWorkingDirectory(workingDirectory);

    // Set environment
    QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
    for (auto it = env.begin(); it != env.end(); ++it) {
        processEnv.insert(it.key(), it.value());
    }
    m_runProcess->setProcessEnvironment(processEnv);

    ui->textEdit->setReadOnly(true);
    clear();
    appendOutput(QString("$ %1 %2\n").arg(command, args.join(" ")));
    if (!workingDirectory.isEmpty()) {
        appendOutput(QString("Working directory: %1\n\n").arg(workingDirectory));
    }

    // Start the process
    m_runProcess->start(command, args);

    emit processStarted();
}

bool Terminal::runFile(const QString& filePath)
{
    RunTemplateManager& manager = RunTemplateManager::instance();

    // Ensure templates are loaded
    if (manager.getAllTemplates().isEmpty()) {
        manager.loadTemplates();
    }

    QPair<QString, QStringList> command = manager.buildCommand(filePath);

    if (command.first.isEmpty()) {
        clear();
        appendOutput("Error: No run template found for this file type.\n", true);
        appendOutput("Use Edit > Run Configurations to assign a template.\n");
        return false;
    }

    QString workingDir = manager.getWorkingDirectory(filePath);
    QMap<QString, QString> env = manager.getEnvironment(filePath);

    executeCommand(command.first, command.second, workingDir, env);
    return true;
}

void Terminal::stopProcess()
{
    cleanupRunProcess(true);
}

void Terminal::cleanupRunProcess(bool restartShell)
{
    if (m_runProcess) {
        // Disconnect all signals first to prevent callbacks after deletion
        disconnect(m_runProcess, nullptr, this, nullptr);

        if (m_runProcess->state() != QProcess::NotRunning) {
            m_runProcess->terminate();
            if (!m_runProcess->waitForFinished(3000)) {
                m_runProcess->kill();
                m_runProcess->waitForFinished(1000);
            }
        }
        delete m_runProcess;
        m_runProcess = nullptr;
    }

    ui->textEdit->setReadOnly(false);

    if (restartShell && m_restartShellAfterRun && !isRunning()) {
        startShell(m_workingDirectory);
    }

    if (restartShell) {
        m_restartShellAfterRun = false;
    }
}

void Terminal::onRunProcessReadyReadStdout()
{
    if (m_runProcess) {
        QString output = QString::fromUtf8(m_runProcess->readAllStandardOutput());
        appendOutput(output);
    }
}

void Terminal::onRunProcessReadyReadStderr()
{
    if (m_runProcess) {
        QString output = QString::fromUtf8(m_runProcess->readAllStandardError());
        appendOutput(output, true);
    }
}

void Terminal::onRunProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);

    appendOutput(QString("\n\nProcess finished with exit code %1\n").arg(exitCode));
    emit processFinished(exitCode);
    cleanupRunProcess(true);
}

void Terminal::onRunProcessError(QProcess::ProcessError error)
{
    QString errorMessage;
    switch (error) {
        case QProcess::FailedToStart:
            errorMessage = "Failed to start. The program may not be installed or not in PATH.";
            break;
        case QProcess::Crashed:
            errorMessage = "The process crashed.";
            break;
        case QProcess::Timedout:
            errorMessage = "The process timed out.";
            break;
        case QProcess::WriteError:
            errorMessage = "Write error occurred.";
            break;
        case QProcess::ReadError:
            errorMessage = "Read error occurred.";
            break;
        default:
            errorMessage = "An unknown error occurred.";
            break;
    }

    appendOutput(QString("\nError: %1\n").arg(errorMessage), true);
    emit processError(errorMessage);

    if (error == QProcess::FailedToStart) {
        cleanupRunProcess(true);
    }
}

void Terminal::on_closeButton_clicked()
{
    stopProcess();
    stopShell();
    emit destroyed();
    close();
}

void Terminal::onReadyReadStandardOutput()
{
    if (!m_process) {
        return;
    }

    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromLocal8Bit(data);

    appendOutput(output);
    emit outputReceived(output);
}

void Terminal::onReadyReadStandardError()
{
    if (!m_process) {
        return;
    }

    QByteArray data = m_process->readAllStandardError();
    QString output = QString::fromLocal8Bit(data);

    appendOutput(output, true);
    emit outputReceived(output);
}

void Terminal::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Failed to start shell process";
        break;
    case QProcess::Crashed:
        errorMsg = "Shell process crashed";
        break;
    case QProcess::Timedout:
        errorMsg = "Shell process timed out";
        break;
    case QProcess::WriteError:
        errorMsg = "Error writing to shell process";
        break;
    case QProcess::ReadError:
        errorMsg = "Error reading from shell process";
        break;
    default:
        errorMsg = "Unknown shell error";
        break;
    }

    appendOutput(QString("Error: %1\n").arg(errorMsg), true);
    m_processRunning = false;
    emit errorOccurred(errorMsg);
}

void Terminal::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);

    m_processRunning = false;
    appendOutput(QString("\nShell exited with code: %1\n").arg(exitCode));
    emit shellFinished(exitCode);
}

void Terminal::onInputSubmitted()
{
    if (!m_currentInput.isEmpty()) {
        executeCommand(m_currentInput);
        m_currentInput.clear();
    }
}

bool Terminal::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->textEdit && event->type() == QEvent::KeyPress) {
        if (m_runProcess && m_runProcess->state() != QProcess::NotRunning) {
            return QWidget::eventFilter(obj, event);
        }

        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            {
                // Get the last line as command input
                QTextCursor cursor = ui->textEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                cursor.movePosition(QTextCursor::StartOfBlock);
                cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                QString line = cursor.selectedText();

                // Move cursor to end and add newline
                ui->textEdit->moveCursor(QTextCursor::End);
                ui->textEdit->insertPlainText("\n");

                // Execute the command
                if (!line.trimmed().isEmpty()) {
                    executeCommand(line);
                }
                return true;
            }

        case Qt::Key_Up:
            handleHistoryNavigation(true);
            return true;

        case Qt::Key_Down:
            handleHistoryNavigation(false);
            return true;

        case Qt::Key_C:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                // Ctrl+C with selection - copy to clipboard
                if (ui->textEdit->textCursor().hasSelection()) {
                    ui->textEdit->copy();
                    return true;
                }
                // Ctrl+C without selection - send interrupt signal
                if (isRunning()) {
                    m_process->write("\x03");  // Ctrl+C character
                }
                appendOutput("^C\n");
                return true;
            }
            break;

        case Qt::Key_V:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                // Ctrl+V - paste from clipboard at cursor position
                ui->textEdit->paste();
                return true;
            }
            break;

        case Qt::Key_X:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                // Ctrl+X - cut selected text
                if (ui->textEdit->textCursor().hasSelection()) {
                    ui->textEdit->cut();
                    return true;
                }
            }
            break;

        case Qt::Key_A:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                // Ctrl+A - select all
                ui->textEdit->selectAll();
                return true;
            }
            break;

        case Qt::Key_D:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                // Ctrl+D - send EOF
                if (isRunning()) {
                    m_process->write("\x04");  // Ctrl+D character
                }
                return true;
            }
            break;

        case Qt::Key_L:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                // Ctrl+L - clear screen
                clear();
                return true;
            }
            break;

        default:
            break;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void Terminal::appendOutput(const QString& text, bool isError)
{
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    if (isError) {
        // Set error text format using theme color
        QTextCharFormat errorFormat;
        errorFormat.setForeground(QColor(m_errorColor));
        cursor.insertText(text, errorFormat);
    } else {
        // Reset to default format using theme color
        QTextCharFormat defaultFormat;
        defaultFormat.setForeground(QColor(m_textColor));
        cursor.insertText(text, defaultFormat);
    }

    ui->textEdit->setTextCursor(cursor);
    scrollToBottom();
}

void Terminal::appendPrompt()
{
    QString prompt = QString("%1 $ ").arg(m_workingDirectory);
    appendOutput(prompt);
}

QString Terminal::getShellCommand() const
{
#ifdef Q_OS_WIN
    // Windows: use cmd.exe or PowerShell
    QString comspec = qEnvironmentVariable("COMSPEC", "cmd.exe");
    return comspec;
#else
    // Unix/Linux/macOS: try user's shell or fall back to /bin/sh
    QString shell = qEnvironmentVariable("SHELL", "/bin/sh");
    return shell;
#endif
}

QStringList Terminal::getShellArguments() const
{
    QStringList args;

#ifdef Q_OS_WIN
    // Keep cmd running for interactive use
    // No special arguments needed
#else
    // Interactive shell mode
    args << "-i";
#endif

    return args;
}

void Terminal::scrollToBottom()
{
    QScrollBar* vScrollBar = ui->textEdit->verticalScrollBar();
    vScrollBar->setValue(vScrollBar->maximum());
}

void Terminal::handleHistoryNavigation(bool up)
{
    if (m_commandHistory.isEmpty()) {
        return;
    }

    if (up) {
        // Navigate up in history (towards older commands)
        if (m_historyIndex > 0) {
            m_historyIndex--;
        } else if (m_historyIndex == m_commandHistory.size()) {
            // First up press from "past end" position - go to last command
            m_historyIndex = m_commandHistory.size() - 1;
        }
    } else {
        // Navigate down in history (towards newer commands)
        if (m_historyIndex < m_commandHistory.size()) {
            m_historyIndex++;
            if (m_historyIndex >= m_commandHistory.size()) {
                // Past end of history, clear input
                QTextCursor cursor = ui->textEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                return;
            }
        }
    }

    // Replace current line with history entry
    if (m_historyIndex >= 0 && m_historyIndex < m_commandHistory.size()) {
        QTextCursor cursor = ui->textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        cursor.insertText(m_commandHistory[m_historyIndex]);
        ui->textEdit->setTextCursor(cursor);
    }
}

void Terminal::applyTheme(const QString& backgroundColor,
                          const QString& textColor,
                          const QString& errorColor)
{
    if (!backgroundColor.isEmpty()) {
        m_backgroundColor = backgroundColor;
    }
    if (!textColor.isEmpty()) {
        m_textColor = textColor;
    }
    if (!errorColor.isEmpty()) {
        m_errorColor = errorColor;
    }
    
    updateStyleSheet();
}

void Terminal::updateStyleSheet()
{
    QString styleSheet = QString(
        "QPlainTextEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  selection-background-color: #1b2a43;"
        "  border: none;"
        "}"
    ).arg(m_backgroundColor, m_textColor);
    
    ui->textEdit->setStyleSheet(styleSheet);
}
