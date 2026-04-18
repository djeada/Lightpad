#include "terminaltabwidget.h"
#include "../../python/pythonprojectenvironment.h"
#include "../../run_templates/runtemplatemanager.h"
#include "../../settings/theme.h"
#include "shellprofile.h"
#include "terminal.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

TerminalTabWidget::TerminalTabWidget(QWidget *parent)
    : QWidget(parent), m_splitter(nullptr), m_tabWidget(nullptr),
      m_newTerminalButton(nullptr), m_clearButton(nullptr),
      m_killButton(nullptr), m_closeButton(nullptr),
      m_shellProfileMenu(nullptr), m_terminalCounter(0) {
  setObjectName("TerminalTabWidget");

  m_currentWorkingDirectory = QDir::currentPath();

  setupUI();

  addNewTerminal();
}

TerminalTabWidget::~TerminalTabWidget() { closeAllTerminals(); }

void TerminalTabWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  setupToolbar();

  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setObjectName("terminalSplitter");
  m_splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_tabWidget = new QTabWidget(this);
  m_tabWidget->setObjectName("terminalTabs");
  m_tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_tabWidget->setTabsClosable(true);
  m_tabWidget->setMovable(true);
  m_tabWidget->setDocumentMode(true);

  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this,
          &TerminalTabWidget::onTabCloseRequested);
  connect(m_tabWidget, &QTabWidget::currentChanged, this,
          &TerminalTabWidget::onCurrentTabChanged);

  m_splitter->addWidget(m_tabWidget);
  mainLayout->addWidget(m_splitter, 1);
}

void TerminalTabWidget::setupToolbar() {
  QWidget *toolbar = new QWidget(this);
  toolbar->setObjectName("terminalToolbar");
  toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(4, 2, 4, 2);
  toolbarLayout->setSpacing(2);

  m_newTerminalButton = new QToolButton(toolbar);
  m_newTerminalButton->setObjectName("newTerminalButton");
  m_newTerminalButton->setText("+");
  m_newTerminalButton->setToolTip(tr("New Terminal (Ctrl+Shift+`)"));
  m_newTerminalButton->setIcon(
      qApp->style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  m_newTerminalButton->setAutoRaise(true);
  m_newTerminalButton->setPopupMode(QToolButton::MenuButtonPopup);

  setupShellProfileMenu();
  m_newTerminalButton->setMenu(m_shellProfileMenu);

  connect(m_newTerminalButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onNewTerminalClicked);

  m_clearButton = new QToolButton(toolbar);
  m_clearButton->setObjectName("clearTerminalButton");
  m_clearButton->setToolTip(tr("Clear Terminal (Ctrl+L)"));
  m_clearButton->setIcon(
      qApp->style()->standardIcon(QStyle::SP_DialogResetButton));
  m_clearButton->setAutoRaise(true);
  connect(m_clearButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onClearTerminalClicked);

  m_killButton = new QToolButton(toolbar);
  m_killButton->setObjectName("killTerminalButton");
  m_killButton->setText(tr("Stop"));
  m_killButton->setToolTip(
      tr("Stop running program or interrupt the terminal (Ctrl+C)"));
  m_killButton->setIcon(qApp->style()->standardIcon(QStyle::SP_BrowserStop));
  m_killButton->setAutoRaise(true);
  m_killButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_killButton->setEnabled(false);
  connect(m_killButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onKillProcessClicked);

  m_closeButton = new QToolButton(toolbar);
  m_closeButton->setObjectName("closeTerminalPanelButton");
  m_closeButton->setToolTip(tr("Close Terminal Panel"));
  m_closeButton->setText(QStringLiteral("\u00D7"));
  m_closeButton->setAutoRaise(true);
  m_closeButton->setCursor(Qt::ArrowCursor);
  m_closeButton->setFixedSize(QSize(18, 18));
  m_closeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(m_closeButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onCloseButtonClicked);

  toolbarLayout->addWidget(m_newTerminalButton);
  toolbarLayout->addWidget(m_clearButton);
  toolbarLayout->addWidget(m_killButton);
  toolbarLayout->addStretch();
  toolbarLayout->addWidget(m_closeButton);

  qobject_cast<QVBoxLayout *>(layout())->addWidget(toolbar, 0);
}

void TerminalTabWidget::setupShellProfileMenu() {
  m_shellProfileMenu = new QMenu(this);

  const QVector<ShellProfile> &profiles =
      ShellProfileManager::instance().availableProfiles();
  for (const ShellProfile &profile : profiles) {
    QAction *action = m_shellProfileMenu->addAction(profile.name);
    connect(action, &QAction::triggered, this,
            [this, name = profile.name]() { onShellProfileSelected(name); });
  }
}

Terminal *TerminalTabWidget::addNewTerminal(const QString &workingDirectory) {
  Terminal *terminal = new Terminal(this);

  QString workDir =
      workingDirectory.isEmpty() ? m_currentWorkingDirectory : workingDirectory;
  if (!workDir.isEmpty()) {
    terminal->setWorkingDirectory(workDir);
  }

  connect(terminal, &Terminal::processStarted, this,
          &TerminalTabWidget::processStarted);
  connect(terminal, &Terminal::processFinished, this,
          &TerminalTabWidget::processFinished);
  connect(terminal, &Terminal::processError, this,
          &TerminalTabWidget::errorOccurred);
  connect(terminal, &Terminal::linkClicked, this,
          &TerminalTabWidget::onTerminalLinkClicked);
  connect(terminal, &Terminal::processStarted, this,
          &TerminalTabWidget::updateRunningState);
  connect(terminal, &Terminal::processFinished, this,
          [this](int) { updateRunningState(); });
  connect(terminal, &Terminal::processError, this,
          [this](const QString &) { updateRunningState(); });
  connect(terminal, &Terminal::shellStarted, this,
          &TerminalTabWidget::updateRunningState);
  connect(terminal, &Terminal::shellFinished, this,
          [this](int) { updateRunningState(); });

  QString tabName = generateTerminalName();
  int index = m_tabWidget->addTab(terminal, tabName);
  m_tabWidget->setCurrentIndex(index);

  return terminal;
}

Terminal *
TerminalTabWidget::addNewTerminalWithProfile(const QString &profileName,
                                             const QString &workingDirectory) {
  Terminal *terminal = new Terminal(this);

  terminal->setShellProfileByName(profileName);

  QString workDir =
      workingDirectory.isEmpty() ? m_currentWorkingDirectory : workingDirectory;
  if (!workDir.isEmpty()) {
    terminal->setWorkingDirectory(workDir);
  }

  connect(terminal, &Terminal::processStarted, this,
          &TerminalTabWidget::processStarted);
  connect(terminal, &Terminal::processFinished, this,
          &TerminalTabWidget::processFinished);
  connect(terminal, &Terminal::processError, this,
          &TerminalTabWidget::errorOccurred);
  connect(terminal, &Terminal::linkClicked, this,
          &TerminalTabWidget::onTerminalLinkClicked);
  connect(terminal, &Terminal::processStarted, this,
          &TerminalTabWidget::updateRunningState);
  connect(terminal, &Terminal::processFinished, this,
          [this](int) { updateRunningState(); });
  connect(terminal, &Terminal::processError, this,
          [this](const QString &) { updateRunningState(); });
  connect(terminal, &Terminal::shellStarted, this,
          &TerminalTabWidget::updateRunningState);
  connect(terminal, &Terminal::shellFinished, this,
          [this](int) { updateRunningState(); });

  QString tabName = generateTerminalName(profileName);
  int index = m_tabWidget->addTab(terminal, tabName);
  m_tabWidget->setCurrentIndex(index);

  return terminal;
}

Terminal *TerminalTabWidget::currentTerminal() {
  return qobject_cast<Terminal *>(m_tabWidget->currentWidget());
}

Terminal *TerminalTabWidget::terminalAt(int index) {
  return qobject_cast<Terminal *>(m_tabWidget->widget(index));
}

int TerminalTabWidget::terminalCount() const { return m_tabWidget->count(); }

void TerminalTabWidget::closeTerminal(int index) {
  if (index < 0 || index >= m_tabWidget->count()) {
    return;
  }

  Terminal *terminal = terminalAt(index);
  if (terminal) {
    terminal->stopProcess();
    terminal->stopShell();
    m_tabWidget->removeTab(index);
    terminal->deleteLater();
  }

  if (m_tabWidget->count() == 0) {
    addNewTerminal();
  }
}

void TerminalTabWidget::closeAllTerminals() {
  while (m_tabWidget->count() > 0) {
    Terminal *terminal = terminalAt(0);
    if (terminal) {
      terminal->stopProcess();
      terminal->stopShell();
      m_tabWidget->removeTab(0);
      terminal->deleteLater();
    }
  }
}

void TerminalTabWidget::clearCurrentTerminal() {
  Terminal *terminal = currentTerminal();
  if (terminal) {
    terminal->clear();
  }
}

bool TerminalTabWidget::runFile(const QString &filePath,
                                const QString &languageId) {
  Terminal *terminal = currentTerminal();
  if (!terminal) {
    terminal = addNewTerminal();
  }

  bool result = terminal->runFile(filePath, languageId);

  if (result) {
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "py" || ext == "pyw" || ext == "pyi") {
      int index = m_tabWidget->indexOf(terminal);
      if (index >= 0) {
        RunTemplateManager &manager = RunTemplateManager::instance();
        FileTemplateAssignment assignment =
            manager.getAssignmentForFile(filePath);
        PythonEnvironmentPreference preference;
        preference.mode = assignment.pythonMode;
        preference.customInterpreter = assignment.pythonInterpreter;
        preference.venvPath = assignment.pythonVenvPath;
        preference.requirementsFile = assignment.pythonRequirementsFile;

        PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
            preference, manager.workspaceFolder(), filePath,
            QFileInfo(filePath).absolutePath());

        QString tabName;
        if (info.found && info.isVirtualEnvironment()) {
          tabName =
              QString("Python (%1)").arg(QFileInfo(info.venvPath).fileName());
        } else if (info.found) {
          tabName = QString("Python (%1)")
                        .arg(QFileInfo(info.interpreter).fileName());
        } else {
          tabName = tr("Python");
        }
        m_tabWidget->setTabText(index, tabName);
      }
    }
  }

  return result;
}

void TerminalTabWidget::executeCommand(const QString &command,
                                       const QStringList &args,
                                       const QString &workingDirectory,
                                       const QMap<QString, QString> &env) {
  Terminal *terminal = currentTerminal();
  if (!terminal) {
    terminal = addNewTerminal();
  }

  terminal->executeCommand(command, args, workingDirectory, env);
}

void TerminalTabWidget::stopCurrentProcess() {
  Terminal *terminal = currentTerminal();
  if (terminal) {
    terminal->interruptActiveProcess();
  }
}

void TerminalTabWidget::setWorkingDirectory(const QString &directory) {
  m_currentWorkingDirectory = directory;

  Terminal *terminal = currentTerminal();
  if (terminal) {
    terminal->setWorkingDirectory(directory);
  }
}

void TerminalTabWidget::applyTheme(const Theme &theme) {

  QString bgColor = theme.backgroundColor.isValid()
                        ? theme.backgroundColor.name()
                        : "#0e1116";
  QString textColor = theme.foregroundColor.isValid()
                          ? theme.foregroundColor.name()
                          : "#e6edf3";
  QString borderColor = theme.lineNumberAreaColor.isValid()
                            ? theme.lineNumberAreaColor.name()
                            : "#30363d";
  QString pressedColor =
      theme.errorColor.isValid() ? theme.errorColor.name() : "#e81123";

  QString tabWidgetStyle = QString("QTabWidget::pane {"
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
                                   "}")
                               .arg(borderColor, bgColor, textColor, bgColor);

  m_tabWidget->setStyleSheet(tabWidgetStyle);

  const QString closeButtonStyle =
      Terminal::closeButtonStyle(textColor, pressedColor);
  m_closeButton->setStyleSheet(closeButtonStyle);

  for (int i = 0; i < m_tabWidget->count(); ++i) {
    Terminal *terminal = terminalAt(i);
    if (terminal) {
      terminal->applyTheme(bgColor, textColor);
    }
  }
}

void TerminalTabWidget::sendTextToTerminal(const QString &text,
                                           bool appendNewline) {
  Terminal *terminal = currentTerminal();
  if (terminal) {
    terminal->sendText(text, appendNewline);
  }
}

void TerminalTabWidget::splitHorizontal() {}

bool TerminalTabWidget::isSplit() const { return false; }

void TerminalTabWidget::unsplit() {}

QStringList TerminalTabWidget::availableShellProfiles() const {
  QStringList names;
  for (const ShellProfile &profile :
       ShellProfileManager::instance().availableProfiles()) {
    names.append(profile.name);
  }
  return names;
}

void TerminalTabWidget::onNewTerminalClicked() { addNewTerminal(); }

void TerminalTabWidget::onClearTerminalClicked() { clearCurrentTerminal(); }

void TerminalTabWidget::onCloseButtonClicked() { emit closeRequested(); }

void TerminalTabWidget::onTabCloseRequested(int index) { closeTerminal(index); }

void TerminalTabWidget::onCurrentTabChanged(int index) {
  Q_UNUSED(index);
  updateRunningState();
}

void TerminalTabWidget::onKillProcessClicked() {
  Terminal *terminal = currentTerminal();
  if (terminal) {
    terminal->interruptActiveProcess();
    updateRunningState();
  }
}

void TerminalTabWidget::updateRunningState() {
  Terminal *terminal = currentTerminal();
  bool canStopProcess = terminal && terminal->canInterruptActiveProcess();
  m_killButton->setEnabled(canStopProcess);

  for (int i = 0; i < m_tabWidget->count(); ++i) {
    Terminal *t = terminalAt(i);
    if (t && t->hasActiveRunProcess()) {
      m_tabWidget->setTabIcon(
          i, qApp->style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
      m_tabWidget->setTabIcon(i, QIcon());
    }
  }
}

void TerminalTabWidget::onShellProfileSelected(const QString &profileName) {
  addNewTerminalWithProfile(profileName);
}

void TerminalTabWidget::onTerminalLinkClicked(const QString &link) {
  emit linkClicked(link);
}

QString TerminalTabWidget::generateTerminalName() {
  ++m_terminalCounter;
  return tr("Terminal %1").arg(m_terminalCounter);
}

QString TerminalTabWidget::generateTerminalName(const QString &profileName) {
  ++m_terminalCounter;
  return tr("%1 %2").arg(profileName).arg(m_terminalCounter);
}
