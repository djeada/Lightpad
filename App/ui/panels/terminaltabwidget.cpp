#include "terminaltabwidget.h"
#include "../../settings/theme.h"
#include "shellprofile.h"
#include "terminal.h"

#include <QApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

TerminalTabWidget::TerminalTabWidget(QWidget *parent)
    : QWidget(parent), m_splitter(nullptr), m_tabWidget(nullptr),
      m_secondaryTabWidget(nullptr), m_newTerminalButton(nullptr),
      m_clearButton(nullptr), m_splitButton(nullptr), m_closeButton(nullptr),
      m_shellProfileMenu(nullptr), m_terminalCounter(0), m_isSplit(false) {
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

  m_tabWidget = new QTabWidget(this);
  m_tabWidget->setTabsClosable(true);
  m_tabWidget->setMovable(true);
  m_tabWidget->setDocumentMode(true);

  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this,
          &TerminalTabWidget::onTabCloseRequested);
  connect(m_tabWidget, &QTabWidget::currentChanged, this,
          &TerminalTabWidget::onCurrentTabChanged);

  m_splitter->addWidget(m_tabWidget);
  mainLayout->addWidget(m_splitter);
}

void TerminalTabWidget::setupToolbar() {
  QWidget *toolbar = new QWidget(this);
  QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(4, 4, 4, 4);
  toolbarLayout->setSpacing(4);

  m_newTerminalButton = new QToolButton(toolbar);
  m_newTerminalButton->setText("+");
  m_newTerminalButton->setToolTip(tr("New Terminal (Ctrl+Shift+`)"));
  m_newTerminalButton->setIcon(
      qApp->style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  m_newTerminalButton->setPopupMode(QToolButton::MenuButtonPopup);

  setupShellProfileMenu();
  m_newTerminalButton->setMenu(m_shellProfileMenu);

  connect(m_newTerminalButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onNewTerminalClicked);

  m_clearButton = new QToolButton(toolbar);
  m_clearButton->setToolTip(tr("Clear Terminal"));
  m_clearButton->setIcon(
      qApp->style()->standardIcon(QStyle::SP_DialogResetButton));
  connect(m_clearButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onClearTerminalClicked);

  m_splitButton = new QToolButton(toolbar);
  m_splitButton->setToolTip(tr("Split Terminal"));
  m_splitButton->setIcon(
      qApp->style()->standardIcon(QStyle::SP_TitleBarNormalButton));
  connect(m_splitButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onSplitTerminalClicked);

  m_closeButton = new QToolButton(toolbar);
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
  toolbarLayout->addWidget(m_splitButton);
  toolbarLayout->addStretch();
  toolbarLayout->addWidget(m_closeButton);

  qobject_cast<QVBoxLayout *>(layout())->addWidget(toolbar);
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

int TerminalTabWidget::terminalCount() const {
  int count = m_tabWidget->count();
  if (m_secondaryTabWidget) {
    count += m_secondaryTabWidget->count();
  }
  return count;
}

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

  if (m_secondaryTabWidget) {
    while (m_secondaryTabWidget->count() > 0) {
      Terminal *terminal =
          qobject_cast<Terminal *>(m_secondaryTabWidget->widget(0));
      if (terminal) {
        terminal->stopProcess();
        terminal->stopShell();
        m_secondaryTabWidget->removeTab(0);
        terminal->deleteLater();
      }
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

  return terminal->runFile(filePath, languageId);
}

void TerminalTabWidget::stopCurrentProcess() {
  Terminal *terminal = currentTerminal();
  if (terminal) {
    terminal->stopProcess();
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
  if (m_secondaryTabWidget) {
    m_secondaryTabWidget->setStyleSheet(tabWidgetStyle);
  }

  const QString closeButtonStyle =
      Terminal::closeButtonStyle(textColor, pressedColor);
  m_closeButton->setStyleSheet(closeButtonStyle);

  for (int i = 0; i < m_tabWidget->count(); ++i) {
    Terminal *terminal = terminalAt(i);
    if (terminal) {
      terminal->applyTheme(bgColor, textColor);
    }
  }

  if (m_secondaryTabWidget) {
    for (int i = 0; i < m_secondaryTabWidget->count(); ++i) {
      Terminal *terminal =
          qobject_cast<Terminal *>(m_secondaryTabWidget->widget(i));
      if (terminal) {
        terminal->applyTheme(bgColor, textColor);
      }
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

void TerminalTabWidget::splitHorizontal() {
  if (m_isSplit) {
    return;
  }

  m_secondaryTabWidget = new QTabWidget(this);
  m_secondaryTabWidget->setTabsClosable(true);
  m_secondaryTabWidget->setMovable(true);
  m_secondaryTabWidget->setDocumentMode(true);

  connect(m_secondaryTabWidget, &QTabWidget::tabCloseRequested, this,
          [this](int index) {
            if (m_secondaryTabWidget) {
              Terminal *terminal =
                  qobject_cast<Terminal *>(m_secondaryTabWidget->widget(index));
              if (terminal) {
                terminal->stopProcess();
                terminal->stopShell();
                m_secondaryTabWidget->removeTab(index);
                terminal->deleteLater();
              }

              if (m_secondaryTabWidget->count() == 0) {
                unsplit();
              }
            }
          });

  m_splitter->addWidget(m_secondaryTabWidget);

  Terminal *terminal = new Terminal(this);

  if (!m_currentWorkingDirectory.isEmpty()) {
    terminal->setWorkingDirectory(m_currentWorkingDirectory);
  }

  connect(terminal, &Terminal::processStarted, this,
          &TerminalTabWidget::processStarted);
  connect(terminal, &Terminal::processFinished, this,
          &TerminalTabWidget::processFinished);
  connect(terminal, &Terminal::processError, this,
          &TerminalTabWidget::errorOccurred);
  connect(terminal, &Terminal::linkClicked, this,
          &TerminalTabWidget::onTerminalLinkClicked);

  QString tabName = generateTerminalName();
  m_secondaryTabWidget->addTab(terminal, tabName);

  m_isSplit = true;
  m_splitButton->setToolTip(tr("Unsplit Terminal"));
}

bool TerminalTabWidget::isSplit() const { return m_isSplit; }

void TerminalTabWidget::unsplit() {
  if (!m_isSplit || !m_secondaryTabWidget) {
    return;
  }

  while (m_secondaryTabWidget->count() > 0) {
    Terminal *terminal =
        qobject_cast<Terminal *>(m_secondaryTabWidget->widget(0));
    if (terminal) {
      terminal->stopProcess();
      terminal->stopShell();
      m_secondaryTabWidget->removeTab(0);
      terminal->deleteLater();
    }
  }

  m_splitter->widget(1)->deleteLater();
  m_secondaryTabWidget = nullptr;
  m_isSplit = false;
  m_splitButton->setToolTip(tr("Split Terminal"));
}

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

void TerminalTabWidget::onCurrentTabChanged(int index) { Q_UNUSED(index); }

void TerminalTabWidget::onSplitTerminalClicked() {
  if (m_isSplit) {
    unsplit();
  } else {
    splitHorizontal();
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
