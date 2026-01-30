#include "terminal.h"
#include "ui_terminal.h"
#include "../../run_templates/runtemplatemanager.h"

#include <QStyle>
#include <QScrollBar>
#include <QProcessEnvironment>

Terminal::Terminal(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Terminal)
    , m_process(nullptr)
{
    ui->setupUi(this);
    ui->closeButton->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    setupTextEdit();
}

Terminal::~Terminal()
{
    stopProcess();
    delete ui;
}

void Terminal::setupTextEdit()
{
    ui->textEdit->setPlainText("");
    ui->textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    
    // Set monospace font for terminal output
    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);
    font.setPointSize(10);
    ui->textEdit->setFont(font);
}

void Terminal::clear()
{
    ui->textEdit->clear();
}

void Terminal::appendOutput(const QString& text, bool isError)
{
    if (text.isEmpty()) {
        return;
    }
    
    QTextCharFormat format;
    if (isError) {
        format.setForeground(Qt::red);
    }
    
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, format);
    
    // Auto-scroll to bottom
    QScrollBar* scrollBar = ui->textEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void Terminal::executeCommand(const QString& command,
                               const QStringList& args,
                               const QString& workingDirectory,
                               const QMap<QString, QString>& env)
{
    // Stop any existing process
    stopProcess();
    
    // Create new process
    m_process = new QProcess(this);
    
    connect(m_process, &QProcess::readyReadStandardOutput, 
            this, &Terminal::onProcessReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, 
            this, &Terminal::onProcessReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Terminal::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &Terminal::onProcessError);
    
    // Set working directory
    m_process->setWorkingDirectory(workingDirectory);
    
    // Set environment
    QProcessEnvironment processEnv = QProcessEnvironment::systemEnvironment();
    for (auto it = env.begin(); it != env.end(); ++it) {
        processEnv.insert(it.key(), it.value());
    }
    m_process->setProcessEnvironment(processEnv);
    
    // Clear output and show command
    clear();
    appendOutput(QString("$ %1 %2\n").arg(command, args.join(" ")));
    appendOutput(QString("Working directory: %1\n\n").arg(workingDirectory));
    
    // Start the process
    m_process->start(command, args);
    
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
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(3000)) {
                m_process->kill();
            }
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
}

bool Terminal::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void Terminal::onProcessReadyReadStdout()
{
    if (m_process) {
        QString output = QString::fromUtf8(m_process->readAllStandardOutput());
        appendOutput(output);
    }
}

void Terminal::onProcessReadyReadStderr()
{
    if (m_process) {
        QString output = QString::fromUtf8(m_process->readAllStandardError());
        appendOutput(output, true);
    }
}

void Terminal::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    
    appendOutput(QString("\n\nProcess finished with exit code %1\n").arg(exitCode));
    emit processFinished(exitCode);
}

void Terminal::onProcessError(QProcess::ProcessError error)
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
}

void Terminal::on_closeButton_clicked()
{
    stopProcess();
    emit destroyed();
    close();
}
