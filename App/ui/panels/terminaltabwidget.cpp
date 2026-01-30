#include "terminaltabwidget.h"
#include "terminal.h"
#include "../../settings/theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QToolButton>
#include <QPlainTextEdit>
#include <QStyle>
#include <QApplication>

TerminalTabWidget::TerminalTabWidget(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_newTerminalButton(nullptr)
    , m_clearButton(nullptr)
    , m_closeAllButton(nullptr)
    , m_closeButton(nullptr)
    , m_terminalCounter(0)
{
    setupUI();
    
    // Add initial terminal
    addNewTerminal();
}

TerminalTabWidget::~TerminalTabWidget()
{
    closeAllTerminals();
}

void TerminalTabWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar
    setupToolbar();

    // Tab widget for terminals
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, 
            this, &TerminalTabWidget::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &TerminalTabWidget::onCurrentTabChanged);

    mainLayout->addWidget(m_tabWidget);
}

void TerminalTabWidget::setupToolbar()
{
    QWidget* toolbar = new QWidget(this);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(4, 4, 4, 4);
    toolbarLayout->setSpacing(4);

    // New terminal button
    m_newTerminalButton = new QToolButton(toolbar);
    m_newTerminalButton->setText("+");
    m_newTerminalButton->setToolTip(tr("New Terminal (Ctrl+Shift+`)"));
    m_newTerminalButton->setIcon(qApp->style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    connect(m_newTerminalButton, &QToolButton::clicked, 
            this, &TerminalTabWidget::onNewTerminalClicked);

    // Clear terminal button
    m_clearButton = new QToolButton(toolbar);
    m_clearButton->setToolTip(tr("Clear Terminal"));
    m_clearButton->setIcon(qApp->style()->standardIcon(QStyle::SP_DialogResetButton));
    connect(m_clearButton, &QToolButton::clicked, 
            this, &TerminalTabWidget::onClearTerminalClicked);

    // Close all terminals button
    m_closeAllButton = new QToolButton(toolbar);
    m_closeAllButton->setToolTip(tr("Close All Terminals"));
    m_closeAllButton->setIcon(qApp->style()->standardIcon(QStyle::SP_DialogCloseButton));
    connect(m_closeAllButton, &QToolButton::clicked,
            this, &TerminalTabWidget::onCloseTerminalClicked);

    // Close terminal panel button
    m_closeButton = new QToolButton(toolbar);
    m_closeButton->setToolTip(tr("Close Terminal Panel"));
    m_closeButton->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    connect(m_closeButton, &QToolButton::clicked, 
            this, &TerminalTabWidget::onCloseButtonClicked);

    toolbarLayout->addWidget(m_newTerminalButton);
    toolbarLayout->addWidget(m_clearButton);
    toolbarLayout->addWidget(m_closeAllButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_closeButton);

    qobject_cast<QVBoxLayout*>(layout())->addWidget(toolbar);
}

Terminal* TerminalTabWidget::addNewTerminal(const QString& workingDirectory)
{
    Terminal* terminal = new Terminal(this);
    
    QString workDir = workingDirectory.isEmpty() ? m_currentWorkingDirectory : workingDirectory;
    if (!workDir.isEmpty()) {
        terminal->setWorkingDirectory(workDir);
    }

    // Connect terminal signals
    connect(terminal, &Terminal::processStarted, this, &TerminalTabWidget::processStarted);
    connect(terminal, &Terminal::processFinished, this, &TerminalTabWidget::processFinished);
    connect(terminal, &Terminal::processError, this, &TerminalTabWidget::errorOccurred);

    QString tabName = generateTerminalName();
    int index = m_tabWidget->addTab(terminal, tabName);
    m_tabWidget->setCurrentIndex(index);

    return terminal;
}

Terminal* TerminalTabWidget::currentTerminal()
{
    return qobject_cast<Terminal*>(m_tabWidget->currentWidget());
}

Terminal* TerminalTabWidget::terminalAt(int index)
{
    return qobject_cast<Terminal*>(m_tabWidget->widget(index));
}

int TerminalTabWidget::terminalCount() const
{
    return m_tabWidget->count();
}

void TerminalTabWidget::closeTerminal(int index)
{
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }

    Terminal* terminal = terminalAt(index);
    if (terminal) {
        terminal->stopProcess();
        terminal->stopShell();
        m_tabWidget->removeTab(index);
        terminal->deleteLater();
    }

    // If no terminals left, create a new one
    if (m_tabWidget->count() == 0) {
        addNewTerminal();
    }
}

void TerminalTabWidget::closeAllTerminals()
{
    while (m_tabWidget->count() > 0) {
        Terminal* terminal = terminalAt(0);
        if (terminal) {
            terminal->stopProcess();
            terminal->stopShell();
            m_tabWidget->removeTab(0);
            terminal->deleteLater();
        }
    }
}

void TerminalTabWidget::clearCurrentTerminal()
{
    Terminal* terminal = currentTerminal();
    if (terminal) {
        terminal->clear();
    }
}

bool TerminalTabWidget::runFile(const QString& filePath)
{
    Terminal* terminal = currentTerminal();
    if (!terminal) {
        terminal = addNewTerminal();
    }
    
    return terminal->runFile(filePath);
}

void TerminalTabWidget::stopCurrentProcess()
{
    Terminal* terminal = currentTerminal();
    if (terminal) {
        terminal->stopProcess();
    }
}

void TerminalTabWidget::setWorkingDirectory(const QString& directory)
{
    m_currentWorkingDirectory = directory;
    
    Terminal* terminal = currentTerminal();
    if (terminal) {
        terminal->setWorkingDirectory(directory);
    }
}

void TerminalTabWidget::applyTheme(const Theme& theme)
{
    // Use theme colors for terminal
    QString bgColor = theme.backgroundColor.name();
    QString textColor = theme.foregroundColor.name();
    QString borderColor = theme.lineNumberAreaColor.name();
    
    // Set default colors if theme values are missing
    if (bgColor.isEmpty() || bgColor == "#000000") bgColor = "#0e1116";
    if (textColor.isEmpty() || textColor == "#000000") textColor = "#e6edf3";
    if (borderColor.isEmpty() || borderColor == "#000000") borderColor = "#30363d";

    QString tabWidgetStyle = QString(
        "QTabWidget::pane {"
        "  border: 1px solid %1;"
        "  background-color: %2;"
        "}"
        "QTabBar::tab {"
        "  background-color: %2;"
        "  color: %3;"
        "  padding: 4px 8px;"
        "  border: 1px solid %1;"
        "  border-bottom: none;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: %4;"
        "}"
    ).arg(borderColor, bgColor, textColor, bgColor);

    m_tabWidget->setStyleSheet(tabWidgetStyle);

    // Apply to all terminals
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        Terminal* terminal = terminalAt(i);
        if (terminal) {
            terminal->applyTheme(bgColor, textColor);
        }
    }
}

void TerminalTabWidget::onNewTerminalClicked()
{
    addNewTerminal();
}

void TerminalTabWidget::onCloseTerminalClicked()
{
    int currentIndex = m_tabWidget->currentIndex();
    if (currentIndex >= 0) {
        closeTerminal(currentIndex);
    }
}

void TerminalTabWidget::onClearTerminalClicked()
{
    clearCurrentTerminal();
}

void TerminalTabWidget::onCloseButtonClicked()
{
    emit closeRequested();
}

void TerminalTabWidget::onTabCloseRequested(int index)
{
    closeTerminal(index);
}

void TerminalTabWidget::onCurrentTabChanged(int index)
{
    Q_UNUSED(index);
    // Could update status bar or other UI elements here
}

void TerminalTabWidget::updateTabTitle(int index)
{
    Terminal* terminal = terminalAt(index);
    if (terminal) {
        // Could update with current directory or running command
        // For now, just keep the terminal number
    }
}

QString TerminalTabWidget::generateTerminalName()
{
    ++m_terminalCounter;
    return tr("Terminal %1").arg(m_terminalCounter);
}
