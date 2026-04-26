#include "terminaltabwidget.h"
#include "../../python/pythonprojectenvironment.h"
#include "../../run_templates/runtemplatemanager.h"
#include "../../settings/theme.h"
#include "../../theme/themeengine.h"
#include "shellprofile.h"
#include "terminal.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString rgba(const QColor &color, qreal alpha) {
  QColor c = color.isValid() ? color : QColor("#ffffff");
  return QString("rgba(%1, %2, %3, %4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(qBound(0.0, alpha, 1.0), 0, 'f', 3);
}
} // namespace

TerminalTabWidget::TerminalTabWidget(QWidget *parent)
    : QWidget(parent), m_splitter(nullptr), m_tabWidget(nullptr),
      m_splitTabWidget(nullptr), m_activeTabWidget(nullptr),
      m_newTerminalButton(nullptr), m_clearButton(nullptr),
      m_killButton(nullptr), m_closeButton(nullptr),
      m_shellProfileMenu(nullptr), m_terminalCounter(0) {
  setObjectName("TerminalTabWidget");
  setContentsMargins(0, 0, 0, 0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_currentWorkingDirectory = QDir::currentPath();

  setupUI();

  addNewTerminal();
}

TerminalTabWidget::~TerminalTabWidget() { closeAllTerminals(); }

void TerminalTabWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setObjectName("terminalSplitter");
  m_splitter->setContentsMargins(0, 0, 0, 0);
  m_splitter->setFrameShape(QFrame::NoFrame);
  m_splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_tabWidget = createTabWidget();
  m_tabWidget->setObjectName("terminalTabs");
  m_activeTabWidget = m_tabWidget;
  setupToolbar();

  m_splitter->addWidget(m_tabWidget);
  mainLayout->addWidget(m_splitter, 1);
}

QTabWidget *TerminalTabWidget::createTabWidget() {
  QTabWidget *tabWidget = new QTabWidget(this);
  tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  tabWidget->setAttribute(Qt::WA_StyledBackground, true);
  tabWidget->setTabsClosable(true);
  tabWidget->setMovable(true);
  tabWidget->setDocumentMode(true);
  tabWidget->setUsesScrollButtons(false);
  tabWidget->tabBar()->setDrawBase(false);

  connect(tabWidget, &QTabWidget::tabCloseRequested, this,
          &TerminalTabWidget::onTabCloseRequested);
  connect(tabWidget, &QTabWidget::currentChanged, this,
          &TerminalTabWidget::onCurrentTabChanged);
  return tabWidget;
}

void TerminalTabWidget::setupToolbar() {
  QWidget *toolbar = new QWidget(m_tabWidget);
  toolbar->setObjectName("terminalToolbar");
  toolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
  toolbarLayout->setContentsMargins(8, 0, 0, 0);
  toolbarLayout->setSpacing(8);
  toolbarLayout->setAlignment(Qt::AlignVCenter);

  m_newTerminalButton = new QToolButton(toolbar);
  m_newTerminalButton->setObjectName("newTerminalButton");
  m_newTerminalButton->setText(tr("+ New"));
  m_newTerminalButton->setToolTip(tr("New Terminal (Ctrl+Shift+`)"));
  m_newTerminalButton->setIcon(QIcon());
  m_newTerminalButton->setAutoRaise(false);
  m_newTerminalButton->setCursor(Qt::PointingHandCursor);
  m_newTerminalButton->setPopupMode(QToolButton::DelayedPopup);
  m_newTerminalButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_newTerminalButton->setFixedHeight(26);

  setupShellProfileMenu();
  m_newTerminalButton->setMenu(m_shellProfileMenu);

  connect(m_newTerminalButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onNewTerminalClicked);

  m_clearButton = new QToolButton(toolbar);
  m_clearButton->setObjectName("clearTerminalButton");
  m_clearButton->setText(tr("Clear"));
  m_clearButton->setToolTip(tr("Clear Terminal (Ctrl+L)"));
  m_clearButton->setIcon(QIcon());
  m_clearButton->setAutoRaise(false);
  m_clearButton->setCursor(Qt::PointingHandCursor);
  m_clearButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_clearButton->setFixedHeight(26);
  connect(m_clearButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onClearTerminalClicked);

  m_killButton = new QToolButton(toolbar);
  m_killButton->setObjectName("killTerminalButton");
  m_killButton->setText(tr("Stop"));
  m_killButton->setToolTip(
      tr("Stop running program or interrupt the terminal (Ctrl+C)"));
  m_killButton->setIcon(QIcon());
  m_killButton->setAutoRaise(false);
  m_killButton->setCursor(Qt::PointingHandCursor);
  m_killButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_killButton->setEnabled(false);
  m_killButton->setFixedHeight(26);
  connect(m_killButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onKillProcessClicked);

  m_closeButton = new QToolButton(toolbar);
  m_closeButton->setObjectName("closeTerminalPanelButton");
  m_closeButton->setToolTip(tr("Close Terminal Panel"));
  m_closeButton->setText(QStringLiteral("\u00D7"));
  m_closeButton->setAutoRaise(true);
  m_closeButton->setCursor(Qt::ArrowCursor);
  m_closeButton->setFixedSize(QSize(22, 22));
  m_closeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(m_closeButton, &QToolButton::clicked, this,
          &TerminalTabWidget::onCloseButtonClicked);

  toolbarLayout->addWidget(m_newTerminalButton);
  toolbarLayout->addWidget(m_clearButton);
  toolbarLayout->addWidget(m_killButton);
  toolbarLayout->addWidget(m_closeButton);

  m_tabWidget->setCornerWidget(toolbar, Qt::TopRightCorner);
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
  QString workDir =
      workingDirectory.isEmpty() ? m_currentWorkingDirectory : workingDirectory;
  return addTerminalToTabWidget(m_activeTabWidget ? m_activeTabWidget
                                                  : m_tabWidget,
                                workDir, generateTerminalName());
}

Terminal *
TerminalTabWidget::addNewTerminalWithProfile(const QString &profileName,
                                             const QString &workingDirectory) {
  QTabWidget *target = m_activeTabWidget ? m_activeTabWidget : m_tabWidget;
  Terminal *terminal = addTerminalToTabWidget(
      target,
      workingDirectory.isEmpty() ? m_currentWorkingDirectory : workingDirectory,
      generateTerminalName(profileName));
  terminal->setShellProfileByName(profileName);
  return terminal;
}

void TerminalTabWidget::connectTerminal(Terminal *terminal) {
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
}

Terminal *
TerminalTabWidget::addTerminalToTabWidget(QTabWidget *tabWidget,
                                          const QString &workingDirectory,
                                          const QString &tabName) {
  Terminal *terminal = new Terminal(this);
  if (!workingDirectory.isEmpty()) {
    terminal->setWorkingDirectory(workingDirectory);
  }
  connectTerminal(terminal);

  int index = tabWidget->addTab(terminal, tabName);
  tabWidget->setCurrentIndex(index);
  m_activeTabWidget = tabWidget;
  return terminal;
}

Terminal *TerminalTabWidget::currentTerminal() {
  QTabWidget *tabs = m_activeTabWidget ? m_activeTabWidget : m_tabWidget;
  Terminal *terminal = qobject_cast<Terminal *>(tabs->currentWidget());
  if (!terminal && tabs != m_tabWidget) {
    terminal = qobject_cast<Terminal *>(m_tabWidget->currentWidget());
  }
  return terminal;
}

Terminal *TerminalTabWidget::terminalAt(int index) {
  if (index < 0) {
    return nullptr;
  }
  if (index < m_tabWidget->count()) {
    return qobject_cast<Terminal *>(m_tabWidget->widget(index));
  }
  index -= m_tabWidget->count();
  if (m_splitTabWidget && index < m_splitTabWidget->count()) {
    return qobject_cast<Terminal *>(m_splitTabWidget->widget(index));
  }
  return nullptr;
}

int TerminalTabWidget::terminalCount() const {
  return m_tabWidget->count() +
         (m_splitTabWidget ? m_splitTabWidget->count() : 0);
}

void TerminalTabWidget::closeTerminal(int index) {
  if (index < 0 || index >= terminalCount()) {
    return;
  }

  QTabWidget *tabWidget = m_tabWidget;
  int localIndex = index;
  if (index >= m_tabWidget->count() && m_splitTabWidget) {
    tabWidget = m_splitTabWidget;
    localIndex = index - m_tabWidget->count();
  }

  Terminal *terminal = terminalAt(index);
  if (terminal) {
    terminal->stopProcess();
    terminal->stopShell();
    tabWidget->removeTab(localIndex);
    terminal->deleteLater();
  }

  if (m_splitTabWidget && m_splitTabWidget->count() == 0) {
    m_splitTabWidget->setParent(nullptr);
    m_splitTabWidget->deleteLater();
    m_splitTabWidget = nullptr;
    m_activeTabWidget = m_tabWidget;
  }

  if (terminalCount() == 0) {
    addNewTerminal();
  }
}

void TerminalTabWidget::closeAllTerminals() {
  while (terminalCount() > 0) {
    Terminal *terminal = terminalAt(0);
    if (terminal) {
      terminal->stopProcess();
      terminal->stopShell();
      QTabWidget *tabs =
          m_tabWidget->indexOf(terminal) >= 0 ? m_tabWidget : m_splitTabWidget;
      if (tabs) {
        tabs->removeTab(tabs->indexOf(terminal));
      }
      terminal->deleteLater();
    }
  }
  if (m_splitTabWidget) {
    m_splitTabWidget->setParent(nullptr);
    m_splitTabWidget->deleteLater();
    m_splitTabWidget = nullptr;
  }
  m_activeTabWidget = m_tabWidget;
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
  const ThemeDefinition &td = ThemeEngine::instance().activeTheme();
  const ThemeColors &colors = td.colors;

  QColor bg = colors.termBg.isValid()
                  ? colors.termBg
                  : (theme.backgroundColor.isValid() ? theme.backgroundColor
                                                     : QColor("#0a0e14"));
  QColor text = colors.termFg.isValid()
                    ? colors.termFg
                    : (theme.foregroundColor.isValid() ? theme.foregroundColor
                                                       : QColor("#b3b1ad"));
  QColor border = colors.borderSubtle.isValid()
                      ? colors.borderSubtle
                      : (theme.borderColor.isValid() ? theme.borderColor
                                                     : QColor("#1c2a1c"));
  QColor raised = colors.surfaceRaised.isValid()
                      ? colors.surfaceRaised
                      : (theme.surfaceColor.isValid() ? theme.surfaceColor
                                                      : bg.lighter(115));
  QColor accent =
      colors.termCursor.isValid()
          ? colors.termCursor
          : (colors.accentPrimary.isValid()
                 ? colors.accentPrimary
                 : (theme.accentColor.isValid() ? theme.accentColor : text));
  QColor pressed =
      colors.statusError.isValid()
          ? colors.statusError
          : (theme.errorColor.isValid() ? theme.errorColor : QColor("#e81123"));
  const QString chromeBg =
      td.ui.chromeOpacity >= 0.999 ? bg.name() : rgba(bg, td.ui.chromeOpacity);
  const QString splitHandle =
      td.ui.panelBorders ? border.name() : QStringLiteral("transparent");

  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QString("QWidget#TerminalTabWidget {"
                        "  background-color: %1;"
                        "  border: none;"
                        "  margin: 0;"
                        "  padding: 0;"
                        "}")
                    .arg(chromeBg));

  applyTabStyle(m_tabWidget, border.name(), chromeBg, raised.name(),
                text.name(), accent.name());
  if (m_splitTabWidget) {
    applyTabStyle(m_splitTabWidget, border.name(), chromeBg, raised.name(),
                  text.name(), accent.name());
  }

  const QString closeButtonStyle =
      Terminal::closeButtonStyle(text.name(), pressed.name());
  m_closeButton->setStyleSheet(closeButtonStyle);

  QWidget *toolbar = findChild<QWidget *>("terminalToolbar");
  if (toolbar) {
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    toolbar->setFixedHeight(30);
    const QString normalButtonBg = rgba(accent, 0.06);
    const QString normalButtonBorder = rgba(accent, 0.42);
    const QString hoverButtonBg = rgba(accent, 0.18);
    const QString hoverButtonBorder = rgba(accent, 0.82);
    const QString pressedButtonBg = rgba(accent, 0.28);
    const QString stopButtonBg = rgba(pressed, 0.08);
    const QString stopButtonBorder = rgba(pressed, 0.44);
    const QString stopHoverButtonBg = rgba(pressed, 0.18);
    const QString stopHoverButtonBorder = rgba(pressed, 0.78);
    toolbar->setStyleSheet(
        QString(
            "QWidget#terminalToolbar {"
            "  background-color: %1;"
            "  border: none;"
            "}"
            "QWidget#terminalToolbar QToolButton#newTerminalButton,"
            "QWidget#terminalToolbar QToolButton#clearTerminalButton,"
            "QWidget#terminalToolbar QToolButton#killTerminalButton {"
            "  color: %2;"
            "  background-color: %4;"
            "  border: 1px solid %5;"
            "  border-radius: 4px;"
            "  padding: 0 12px;"
            "  margin: 0;"
            "  font-size: 11px;"
            "  font-weight: 700;"
            "  min-height: 26px;"
            "}"
            "QWidget#terminalToolbar QToolButton#newTerminalButton:hover,"
            "QWidget#terminalToolbar QToolButton#clearTerminalButton:hover,"
            "QWidget#terminalToolbar QToolButton#killTerminalButton:hover,"
            "QWidget#terminalToolbar QToolButton#newTerminalButton:pressed,"
            "QWidget#terminalToolbar QToolButton#clearTerminalButton:pressed,"
            "QWidget#terminalToolbar QToolButton#killTerminalButton:pressed {"
            "  color: %2;"
            "  background-color: %11;"
            "  border: 1px solid %12;"
            "}"
            "QWidget#terminalToolbar QToolButton#newTerminalButton:pressed,"
            "QWidget#terminalToolbar QToolButton#clearTerminalButton:pressed {"
            "  background-color: %13;"
            "}"
            "QWidget#terminalToolbar QToolButton#killTerminalButton {"
            "  color: %7;"
            "  background-color: %8;"
            "  border: 1px solid %9;"
            "}"
            "QWidget#terminalToolbar QToolButton#killTerminalButton:hover,"
            "QWidget#terminalToolbar QToolButton#killTerminalButton:pressed {"
            "  color: %7;"
            "  background-color: %14;"
            "  border: 1px solid %15;"
            "}"
            "QWidget#terminalToolbar QToolButton#killTerminalButton:pressed {"
            "  background-color: %16;"
            "}"
            "QWidget#terminalToolbar QToolButton#killTerminalButton:disabled {"
            "  color: %3;"
            "  background-color: transparent;"
            "  border: 1px solid %6;"
            "}"
            "QWidget#terminalToolbar QToolButton::menu-indicator {"
            "  image: none;"
            "  width: 0;"
            "}")
            .arg(chromeBg, text.name(), rgba(text, 0.48), normalButtonBg,
                 normalButtonBorder, rgba(text, 0.22), pressed.name(),
                 stopButtonBg, stopButtonBorder, hoverButtonBg,
                 hoverButtonBorder, pressedButtonBg, stopHoverButtonBg,
                 stopHoverButtonBorder, rgba(pressed, 0.28)));
  }

  m_splitter->setStyleSheet(QString("QSplitter::handle {"
                                    "  background: %1;"
                                    "  width: 1px;"
                                    "}")
                                .arg(splitHandle));

  for (int i = 0; i < terminalCount(); ++i) {
    Terminal *terminal = terminalAt(i);
    if (terminal) {
      terminal->applyTheme(bg.name(), text.name());
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

void TerminalTabWidget::applyTabStyle(QTabWidget *tabWidget,
                                      const QString &borderColor,
                                      const QString &bgColor,
                                      const QString &raisedColor,
                                      const QString &textColor,
                                      const QString &accentColor) {
  Q_UNUSED(borderColor);
  Q_UNUSED(raisedColor);
  if (!tabWidget) {
    return;
  }
  QString tabWidgetStyle = QString("QTabWidget::pane {"
                                   "  border: none;"
                                   "  background-color: %1;"
                                   "  top: 0px;"
                                   "  margin: 0;"
                                   "  padding: 0;"
                                   "}"
                                   "QTabWidget::tab-bar {"
                                   "  left: 0;"
                                   "}"
                                   "QTabWidget::right-corner {"
                                   "  background-color: %1;"
                                   "  border: none;"
                                   "  margin: 0;"
                                   "  padding: 0;"
                                   "}"
                                   "QTabBar {"
                                   "  background: %1;"
                                   "  border: none;"
                                   "  qproperty-drawBase: 0;"
                                   "}"
                                   "QTabBar::base {"
                                   "  height: 0px;"
                                   "  min-height: 0px;"
                                   "  max-height: 0px;"
                                   "  border: none;"
                                   "  background: transparent;"
                                   "}"
                                   "QTabBar::tab {"
                                   "  background-color: transparent;"
                                   "  color: %2;"
                                   "  padding: 6px 13px;"
                                   "  border: none;"
                                   "  margin: 0;"
                                   "  font-weight: 600;"
                                   "}"
                                   "QTabBar::tab:selected {"
                                   "  color: %3;"
                                   "  background-color: transparent;"
                                   "  border: none;"
                                   "}"
                                   "QTabBar::tab:hover {"
                                   "  color: %3;"
                                   "  background-color: transparent;"
                                   "}"
                                   "QTabBar::close-button {"
                                   "  image: none;"
                                   "  width: 8px;"
                                   "  height: 8px;"
                                   "  border-radius: 4px;"
                                   "  background: transparent;"
                                   "  margin-left: 4px;"
                                   "}"
                                   "QTabBar::close-button:hover {"
                                   "  background: %3;"
                                   "}")
                               .arg(bgColor, textColor, accentColor);
  tabWidget->setStyleSheet(tabWidgetStyle);
}

void TerminalTabWidget::splitHorizontal() {
  if (!m_splitTabWidget) {
    m_splitTabWidget = createTabWidget();
    m_splitTabWidget->setObjectName("terminalSplitTabs");
    m_splitter->addWidget(m_splitTabWidget);
    m_splitter->setSizes({width() / 2, width() / 2});
  }
  m_activeTabWidget = m_splitTabWidget;
  addNewTerminal(m_currentWorkingDirectory);
}

bool TerminalTabWidget::isSplit() const {
  return m_splitTabWidget && m_splitTabWidget->count() > 0;
}

void TerminalTabWidget::unsplit() {
  if (!m_splitTabWidget) {
    return;
  }
  while (m_splitTabWidget->count() > 0) {
    QWidget *widget = m_splitTabWidget->widget(0);
    QString label = m_splitTabWidget->tabText(0);
    QIcon icon = m_splitTabWidget->tabIcon(0);
    m_splitTabWidget->removeTab(0);
    int index = m_tabWidget->addTab(widget, icon, label);
    m_tabWidget->setCurrentIndex(index);
  }
  m_splitTabWidget->setParent(nullptr);
  m_splitTabWidget->deleteLater();
  m_splitTabWidget = nullptr;
  m_activeTabWidget = m_tabWidget;
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

void TerminalTabWidget::onTabCloseRequested(int index) {
  QTabWidget *tabs = qobject_cast<QTabWidget *>(sender());
  if (tabs == m_splitTabWidget) {
    closeTerminal(m_tabWidget->count() + index);
    return;
  }
  closeTerminal(index);
}

void TerminalTabWidget::onCurrentTabChanged(int index) {
  Q_UNUSED(index);
  QTabWidget *tabs = qobject_cast<QTabWidget *>(sender());
  if (tabs) {
    m_activeTabWidget = tabs;
  }
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

  auto updateTabs = [this](QTabWidget *tabs) {
    if (!tabs) {
      return;
    }
    for (int i = 0; i < tabs->count(); ++i) {
      Terminal *t = qobject_cast<Terminal *>(tabs->widget(i));
      if (t && t->hasActiveRunProcess()) {
        tabs->setTabIcon(i, qApp->style()->standardIcon(QStyle::SP_MediaPlay));
      } else {
        tabs->setTabIcon(i, QIcon());
      }
    }
  };
  updateTabs(m_tabWidget);
  updateTabs(m_splitTabWidget);
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
