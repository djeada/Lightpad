#include "terminal.h"
#include "ui_terminal.h"
#include "../../run_templates/runtemplatemanager.h"

#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QStyle>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QUrl>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

Terminal::Terminal(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Terminal)
    , m_process(nullptr)
    , m_runProcess(nullptr)
    , m_restartTimer(nullptr)
    , m_historyIndex(0)
    , m_processRunning(false)
    , m_restartShellAfterRun(false)
    , m_autoRestartEnabled(true)
    , m_restartAttempts(0)
    , m_backgroundColor("#0e1116")
    , m_textColor("#e6edf3")
    , m_errorColor("#f44336")
    , m_linkColor("#58a6ff")
    , m_scrollbackLines(kDefaultScrollbackLines)
    , m_linkDetectionEnabled(true)
    , m_urlRegex(R"((https?://|ftp://|file://)[^\s<>\"\'\]\)]+)")
    , m_filePathRegex(R"((?:^|[\s:])(/[^\s:]+|[A-Za-z]:\\[^\s:]+))")
    , m_inputStartPosition(0)
{
    ui->setupUi(this);
    ui->closeButton->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    
    // Get default shell profile
    m_shellProfile = ShellProfileManager::instance().defaultProfile();
    
    // Setup restart timer for auto-recovery
    m_restartTimer = new QTimer(this);
    m_restartTimer->setSingleShot(true);
    connect(m_restartTimer, &QTimer::timeout, this, [this]() {
        if (m_autoRestartEnabled && !m_processRunning) {
            appendOutput("Attempting to restart shell...\n");
            if (startShell()) {
                m_restartAttempts = 0;
            }
        }
    });
    
    setupTerminal();
}

Terminal::~Terminal()
{
    // Disable auto-restart during destruction
    m_autoRestartEnabled = false;
    if (m_restartTimer) {
        m_restartTimer->stop();
    }
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

    // Start shell automatically
    startShell();
}

bool Terminal::startShell(const QString& workingDirectory)
{
    // Stop any pending restart timer
    if (m_restartTimer && m_restartTimer->isActive()) {
        m_restartTimer->stop();
    }
    
    if (m_process && m_processRunning) {
        return true;  // Already running
    }

    // Clean up any existing process first
    if (m_process) {
        disconnect(m_process, nullptr, this, nullptr);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            m_process->waitForFinished(1000);
        }
        delete m_process;
        m_process = nullptr;
    }

    m_process = new QProcess(this);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &Terminal::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &Terminal::onReadyReadStandardError);
    connect(m_process, &QProcess::errorOccurred,
            this, &Terminal::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Terminal::onProcessFinished);

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
    env.insert("TERM", "dumb");  // Use dumb terminal to avoid control sequences
    m_process->setProcessEnvironment(env);

#ifndef Q_OS_WIN
    // Create new session to detach from controlling terminal
    // This prevents SIGTTIN/SIGTTOU when the parent runs in background
    m_process->setChildProcessModifier([]() {
        setsid();
    });
#endif

    // Get shell command based on platform
    QString shell = getShellCommand();
    QStringList args = getShellArguments();

    m_process->setProgram(shell);
    m_process->setArguments(args);
    m_process->start();

    if (!m_process->waitForStarted(5000)) {
        appendOutput("Error: Failed to start shell process.\n", true);
        emit errorOccurred("Failed to start shell");
        
        // Clean up failed process
        delete m_process;
        m_process = nullptr;
        
        return false;
    }

    m_processRunning = true;
    m_restartAttempts = 0;  // Reset restart counter on successful start
    ui->textEdit->setReadOnly(false);
    emit shellStarted();

    return true;
}

void Terminal::stopShell()
{
    // Stop any pending restart
    if (m_restartTimer) {
        m_restartTimer->stop();
    }
    
    if (!m_process) {
        return;
    }

    m_processRunning = false;

    // Disconnect signals to prevent callbacks during shutdown
    disconnect(m_process, nullptr, this, nullptr);

    // Try graceful termination
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(2000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
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
    m_inputStartPosition = 0;
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

void Terminal::cleanupProcess()
{
    if (!m_process) {
        return;
    }
    
    // Disconnect signals to prevent further callbacks
    disconnect(m_process, nullptr, this, nullptr);
    
    // Schedule the process for deletion to avoid deleting in signal handler
    m_process->deleteLater();
    m_process = nullptr;
}

void Terminal::scheduleAutoRestart()
{
    if (!m_autoRestartEnabled || !m_restartTimer) {
        return;
    }
    
    ++m_restartAttempts;
    
    if (m_restartAttempts > kMaxRestartAttempts) {
        appendOutput(QString("Auto-restart disabled after %1 failed attempts.\n")
                    .arg(kMaxRestartAttempts), true);
        appendOutput("Use the terminal controls to manually restart the shell.\n");
        m_restartAttempts = 0;
        return;
    }
    
    appendOutput(QString("Will attempt restart in %1 second(s) (attempt %2/%3)...\n")
                .arg(kRestartDelayMs / 1000)
                .arg(m_restartAttempts)
                .arg(kMaxRestartAttempts));
    
    m_restartTimer->start(kRestartDelayMs);
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
    if (exitStatus == QProcess::CrashExit) {
        appendOutput(QString("\n\nProcess crashed (exit code: %1)\n").arg(exitCode), true);
    } else {
        appendOutput(QString("\n\nProcess finished with exit code %1\n").arg(exitCode));
    }
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

    // Clean up on fatal errors
    if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
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
    output = filterShellStartupNoise(output);
    if (output.isEmpty()) {
        return;
    }

    appendOutput(output, true);
    emit outputReceived(output);
}

void Terminal::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    bool shouldRestart = false;
    
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Failed to start shell process";
        shouldRestart = true;
        break;
    case QProcess::Crashed:
        errorMsg = "Shell process crashed";
        shouldRestart = true;
        break;
    case QProcess::Timedout:
        errorMsg = "Shell process timed out";
        shouldRestart = true;
        break;
    case QProcess::WriteError:
        errorMsg = "Error writing to shell process";
        // Don't restart on write errors - may be temporary
        break;
    case QProcess::ReadError:
        errorMsg = "Error reading from shell process";
        // Don't restart on read errors - may be temporary
        break;
    default:
        errorMsg = "Unknown shell error";
        break;
    }

    appendOutput(QString("Error: %1\n").arg(errorMsg), true);
    m_processRunning = false;
    
    // Clean up the process properly
    cleanupProcess();
    
    emit errorOccurred(errorMsg);
    
    // Schedule auto-restart for recoverable errors
    if (shouldRestart && m_autoRestartEnabled) {
        scheduleAutoRestart();
    }
}

void Terminal::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_processRunning = false;
    
    if (exitStatus == QProcess::CrashExit) {
        appendOutput(QString("\nShell crashed (exit code: %1)\n").arg(exitCode), true);
        cleanupProcess();
        
        // Schedule auto-restart for crashes
        if (m_autoRestartEnabled) {
            scheduleAutoRestart();
        }
    } else {
        appendOutput(QString("\nShell exited with code: %1\n").arg(exitCode));
        cleanupProcess();
    }
    
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
                // Get only user input (text after m_inputStartPosition)
                QTextCursor cursor = ui->textEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                int endPos = cursor.position();
                
                // Extract text from input start position to end
                cursor.setPosition(m_inputStartPosition);
                cursor.setPosition(endPos, QTextCursor::KeepAnchor);
                QString userInput = cursor.selectedText();

                // Move cursor to end and add newline
                ui->textEdit->moveCursor(QTextCursor::End);
                ui->textEdit->insertPlainText("\n");

                // Execute the command (send user input only, not the prompt)
                if (!userInput.trimmed().isEmpty()) {
                    executeCommand(userInput);
                } else {
                    // Just send newline for empty input
                    if (m_process && m_process->state() == QProcess::Running) {
                        m_process->write("\n");
                    }
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
            if ((keyEvent->modifiers() & Qt::ControlModifier) && 
                (keyEvent->modifiers() & Qt::ShiftModifier)) {
                // Ctrl+Shift+C - copy to clipboard (terminal convention)
                if (ui->textEdit->textCursor().hasSelection()) {
                    ui->textEdit->copy();
                }
                return true;
            }
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                // Ctrl+C - send interrupt signal
                if (isRunning()) {
                    m_process->write("\x03");  // Ctrl+C character
                }
                appendOutput("^C\n");
                return true;
            }
            break;

        case Qt::Key_V:
            if ((keyEvent->modifiers() & Qt::ControlModifier) && 
                (keyEvent->modifiers() & Qt::ShiftModifier)) {
                // Ctrl+Shift+V - paste from clipboard (terminal convention)
                ui->textEdit->paste();
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
    // Strip ANSI escape sequences before displaying
    QString cleanText = stripAnsiEscapeCodes(text);
    if (cleanText.isEmpty()) {
        return;
    }

    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    if (isError) {
        // Set error text format using theme color
        QTextCharFormat errorFormat;
        errorFormat.setForeground(QColor(m_errorColor));
        cursor.insertText(cleanText, errorFormat);
    } else {
        // Reset to default format using theme color
        QTextCharFormat defaultFormat;
        defaultFormat.setForeground(QColor(m_textColor));
        cursor.insertText(cleanText, defaultFormat);
    }

    ui->textEdit->setTextCursor(cursor);
    scrollToBottom();
    
    // Enforce scrollback limit
    enforceScrollbackLimit();
    
    // Update input start position to current end of document
    // User input will begin after this position
    m_inputStartPosition = ui->textEdit->textCursor().position();
}

void Terminal::appendPrompt()
{
    QString prompt = QString("%1 $ ").arg(m_workingDirectory);
    appendOutput(prompt);
}

QString Terminal::getShellCommand() const
{
    // Use shell profile if set
    if (m_shellProfile.isValid()) {
        return m_shellProfile.command;
    }
    
    // Fallback to default detection
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
    // Use shell profile if set
    if (m_shellProfile.isValid()) {
        return m_shellProfile.arguments;
    }
    
    // Fallback to default arguments
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
        "  selection-color: %2;"
        "  border: none;"
        "}"
    ).arg(m_backgroundColor, m_textColor);
    
    ui->textEdit->setStyleSheet(styleSheet);
}

QString Terminal::filterShellStartupNoise(const QString& text) const
{
    if (text.isEmpty()) {
        return text;
    }

    const QStringList lines = text.split('\n');
    QStringList kept;
    kept.reserve(lines.size());
    bool hasNonEmpty = false;

    for (const QString& line : lines) {
        if (isShellStartupNoiseLine(line)) {
            continue;
        }
        kept.append(line);
        if (!line.isEmpty()) {
            hasNonEmpty = true;
        }
    }

    if (!hasNonEmpty) {
        return QString();
    }

    QString result = kept.join("\n");
    if (text.endsWith('\n') && !result.endsWith('\n')) {
        result.append('\n');
    }

    return result;
}

bool Terminal::isShellStartupNoiseLine(const QString& line) const
{
    if (line.startsWith("bash: cannot set terminal process group")) {
        return true;
    }
    if (line.startsWith("bash: no job control in this shell")) {
        return true;
    }
    return false;
}

void Terminal::setShellProfile(const ShellProfile& profile)
{
    if (!profile.isValid()) {
        return;
    }
    
    bool wasRunning = isRunning();
    QString oldName = m_shellProfile.name;
    
    m_shellProfile = profile;
    
    // Restart shell with new profile if it was running
    if (wasRunning) {
        stopShell();
        startShell(m_workingDirectory);
    }
    
    if (oldName != profile.name) {
        emit shellProfileChanged(profile.name);
    }
}

ShellProfile Terminal::shellProfile() const
{
    return m_shellProfile;
}

QStringList Terminal::availableShellProfiles() const
{
    QStringList names;
    for (const ShellProfile& profile : ShellProfileManager::instance().availableProfiles()) {
        names.append(profile.name);
    }
    return names;
}

bool Terminal::setShellProfileByName(const QString& profileName)
{
    ShellProfile profile = ShellProfileManager::instance().profileByName(profileName);
    if (profile.isValid()) {
        setShellProfile(profile);
        return true;
    }
    return false;
}

void Terminal::sendText(const QString& text, bool appendNewline)
{
    if (!isRunning()) {
        appendOutput("Error: Shell not running.\n", true);
        return;
    }
    
    QString textToSend = text;
    if (appendNewline) {
        textToSend += "\n";
    }
    
    m_process->write(textToSend.toUtf8());
}

void Terminal::setScrollbackLines(int lines)
{
    m_scrollbackLines = lines;
    enforceScrollbackLimit();
}

int Terminal::scrollbackLines() const
{
    return m_scrollbackLines;
}

void Terminal::setLinkDetectionEnabled(bool enabled)
{
    m_linkDetectionEnabled = enabled;
}

bool Terminal::isLinkDetectionEnabled() const
{
    return m_linkDetectionEnabled;
}

void Terminal::onLinkActivated(const QString& link)
{
    emit linkClicked(link);
    
    // Try to open the link
    if (link.startsWith("http://") || link.startsWith("https://") || 
        link.startsWith("ftp://") || link.startsWith("file://")) {
        QDesktopServices::openUrl(QUrl(link));
    } else if (QFile::exists(link)) {
        // It's a file path - emit signal so the application can handle it
        emit linkClicked(link);
    }
}

void Terminal::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && 
        (event->modifiers() & Qt::ControlModifier)) {
        QString link = getLinkAtPosition(event->pos());
        if (!link.isEmpty()) {
            onLinkActivated(link);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

QString Terminal::getLinkAtPosition(const QPoint& pos)
{
    if (!m_linkDetectionEnabled) {
        return QString();
    }
    
    QTextCursor cursor = ui->textEdit->cursorForPosition(pos);
    if (cursor.isNull()) {
        return QString();
    }
    
    // Get the text of the line
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    QString lineText = cursor.selectedText();
    
    // Get position within the line
    int posInLine = ui->textEdit->cursorForPosition(pos).positionInBlock();
    
    // Check for URLs
    QRegularExpressionMatchIterator urlMatches = m_urlRegex.globalMatch(lineText);
    while (urlMatches.hasNext()) {
        QRegularExpressionMatch match = urlMatches.next();
        if (posInLine >= match.capturedStart() && posInLine <= match.capturedEnd()) {
            return match.captured();
        }
    }
    
    // Check for file paths
    QRegularExpressionMatchIterator pathMatches = m_filePathRegex.globalMatch(lineText);
    while (pathMatches.hasNext()) {
        QRegularExpressionMatch match = pathMatches.next();
        QString path = match.captured(1);
        int start = match.capturedStart(1);
        int end = match.capturedEnd(1);
        if (posInLine >= start && posInLine <= end && QFile::exists(path)) {
            return path;
        }
    }
    
    return QString();
}

QString Terminal::processTextForLinks(const QString& text)
{
    if (!m_linkDetectionEnabled) {
        return text;
    }
    
    // For now, just return the text as-is
    // The link detection is handled via mouse clicks
    return text;
}

void Terminal::enforceScrollbackLimit()
{
    if (m_scrollbackLines <= 0) {
        return;  // Unlimited
    }
    
    QTextDocument* doc = ui->textEdit->document();
    int blockCount = doc->blockCount();
    
    if (blockCount > m_scrollbackLines) {
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);
        
        int linesToRemove = blockCount - m_scrollbackLines;
        for (int i = 0; i < linesToRemove; ++i) {
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
        }
        int removedLength = cursor.selectedText().length();
        cursor.removeSelectedText();
        
        // Adjust input start position since we removed text from the beginning
        m_inputStartPosition = qMax(0, m_inputStartPosition - removedLength);
    }
}

QString Terminal::stripAnsiEscapeCodes(const QString& text)
{
    // Remove ANSI escape sequences:
    // - CSI sequences: ESC [ ... (parameters) final byte
    // - OSC sequences: ESC ] ... BEL or ESC ] ... ESC \
    // - Simple escape sequences: ESC followed by single char
    static QRegularExpression ansiRegex(
        R"(\x1b\[[0-9;?]*[A-Za-z])"       // CSI sequences (e.g., colors, cursor)
        R"(|\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)?)"  // OSC sequences (e.g., window title)
        R"(|\x1b[()][AB012])"              // Character set selection
        R"(|\x1b[=>])"                     // Keypad modes
        R"(|\x1b[DME78HcNO])"              // Simple escape sequences
        R"(|\x07)"                         // Bell character
    );
    
    QString result = text;
    result.remove(ansiRegex);
    return result;
}
