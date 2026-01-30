#include "terminal.h"
#include "ui_terminal.h"
#include <QStyle>
#include <QDir>
#include <QScrollBar>
#include <QTextBlock>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Terminal::Terminal(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Terminal)
    , m_process(nullptr)
    , m_historyIndex(-1)
    , m_processRunning(false)
{
    ui->setupUi(this);
    ui->closeButton->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    setupTerminal();
}

Terminal::~Terminal()
{
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
    
    // Set terminal styling
    ui->textEdit->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  selection-background-color: #264f78;"
        "  border: none;"
        "}"
    );
    
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
    
    m_process->setWorkingDirectory(m_workingDirectory);
    
    // Set environment for interactive shell
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TERM", "dumb");  // Simple terminal type for compatibility
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
    
    // Write command to process
    QString cmdWithNewline = command + "\n";
    m_process->write(cmdWithNewline.toLocal8Bit());
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
    appendPrompt();
}

void Terminal::on_closeButton_clicked()
{
    stopShell();
    emit destroyed();
    close();
}

void Terminal::onReadyReadStandardOutput()
{
    if (!m_process) return;
    
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromLocal8Bit(data);
    
    appendOutput(output);
    emit outputReceived(output);
}

void Terminal::onReadyReadStandardError()
{
    if (!m_process) return;
    
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
                // Ctrl+C - send interrupt signal
                if (isRunning()) {
#ifdef Q_OS_WIN
                    // Windows: Send Ctrl+C event
                    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
#else
                    // Unix: Send SIGINT
                    m_process->write("\x03");  // Ctrl+C character
#endif
                }
                appendOutput("^C\n");
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
        // Set error text format
        QTextCharFormat errorFormat;
        errorFormat.setForeground(QColor("#f44336"));  // Red for errors
        cursor.insertText(text, errorFormat);
    } else {
        // Reset to default format
        QTextCharFormat defaultFormat;
        defaultFormat.setForeground(QColor("#d4d4d4"));  // Light gray
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
        // Navigate up in history
        if (m_historyIndex > 0) {
            m_historyIndex--;
        }
    } else {
        // Navigate down in history
        if (m_historyIndex < m_commandHistory.size() - 1) {
            m_historyIndex++;
        } else {
            // Past end of history, clear input
            m_historyIndex = m_commandHistory.size();
            QTextCursor cursor = ui->textEdit->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            return;
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
