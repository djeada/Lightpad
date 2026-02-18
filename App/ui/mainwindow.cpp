#include <QAbstractItemView>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringListModel>
#include <QVBoxLayout>
#include <cstdio>
#include <functional>

#include "../completion/completionengine.h"
#include "../completion/completionproviderregistry.h"
#include "../completion/providers/keywordcompletionprovider.h"
#include "../completion/providers/plugincompletionprovider.h"
#include "../completion/providers/snippetcompletionprovider.h"
#include "../core/autosavemanager.h"
#include "../core/lightpadpage.h"
#include "../core/logging/logger.h"
#include "../core/navigationhistory.h"
#include "../core/recentfilesmanager.h"
#include "../core/textarea.h"
#include "../dap/debugsettings.h"
#include "../definition/symbolnavigationservice.h"
#include "../definition/idefinitionprovider.h"
#include "../definition/languagelspdefinitionprovider.h"
#include "../filetree/gitfilesystemmodel.h"
#include "../format_templates/formattemplatemanager.h"
#include "../run_templates/runtemplatemanager.h"
#include "../syntax/syntaxpluginregistry.h"
#include "dialogs/commandpalette.h"
#include "dialogs/debugconfigurationdialog.h"
#include "dialogs/filequickopen.h"
#include "dialogs/formattemplateselector.h"
#include "dialogs/gitdiffdialog.h"
#include "dialogs/gitfilehistorydialog.h"
#include "dialogs/gitlogdialog.h"
#include "dialogs/gitrebasedialog.h"
#include "dialogs/gotolinedialog.h"
#include "dialogs/gotosymboldialog.h"
#include "dialogs/preferences.h"
#include "dialogs/recentfilesdialog.h"
#include "dialogs/runconfigurations.h"
#include "dialogs/runtemplateselector.h"
#include "dialogs/shortcuts.h"
#include "mainwindow.h"
#include "panels/breadcrumbwidget.h"
#include "panels/debugpanel.h"
#include "panels/findreplacepanel.h"
#include "panels/problemspanel.h"
#include "panels/sourcecontrolpanel.h"
#include "panels/spliteditorcontainer.h"
#include "panels/terminaltabwidget.h"
#include "popup.h"
#include "ui_mainwindow.h"
#include "viewers/imageviewer.h"
#ifdef HAVE_PDF_SUPPORT
#include "viewers/pdfviewer.h"
#endif
#include "../dap/breakpointmanager.h"
#include "../dap/debugadapterregistry.h"
#include "../dap/debugconfiguration.h"
#include "../dap/debugsession.h"
#include "../dap/debugsettings.h"
#include "../dap/watchmanager.h"
#include "../git/gitintegration.h"
#include "../language/languagecatalog.h"
#include "../settings/settingsmanager.h"
#include "panels/spliteditorcontainer.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), popupTabWidth(nullptr),
      preferences(nullptr), findReplacePanel(nullptr), terminalWidget(nullptr),
      completer(nullptr), m_completionEngine(nullptr), highlightLanguage(""),
      font(QApplication::font()), commandPalette(nullptr),
      problemsPanel(nullptr), goToLineDialog(nullptr),
      goToSymbolDialog(nullptr), fileQuickOpen(nullptr),
      recentFilesDialog(nullptr), problemsStatusLabel(nullptr),
      vimStatusLabel(nullptr), m_vimCommandPanelActive(false),
      m_connectedVimMode(nullptr), breadcrumbWidget(nullptr),
      recentFilesManager(nullptr), navigationHistory(nullptr),
      m_symbolNavService(nullptr),
      autoSaveManager(nullptr), m_splitEditorContainer(nullptr),
      m_gitIntegration(nullptr), sourceControlPanel(nullptr),
      sourceControlDock(nullptr), m_inlineBlameEnabled(false),
      m_heatmapEnabled(false), m_codeLensEnabled(false),
      m_gitBranchLabel(nullptr), m_gitSyncLabel(nullptr),
      m_gitDirtyLabel(nullptr), debugPanel(nullptr), debugDock(nullptr),
      m_debugStartInProgress(false), m_breakpointsSetConnection(),
      m_breakpointChangedConnection(), m_sessionTerminatedConnection(),
      m_sessionErrorConnection(), m_sessionStateConnection(),
      m_formatProcessFinishedConnection(), m_formatProcessErrorConnection(),
      m_fileTreeModel(nullptr), m_treeScrollValue(0),
      m_treeScrollValueInitialized(false), m_treeScrollSyncing(false) {
  QApplication::instance()->installEventFilter(this);
  ui->setupUi(this);
  ui->menubar->setNativeMenuBar(false);
  ui->actionFind_in_file->setShortcut(QKeySequence::Find);
  ui->actionFind_in_file->setShortcutContext(Qt::ApplicationShortcut);
  ui->actionReplace_in_file->setShortcut(QKeySequence::Replace);
  ui->actionReplace_in_file->setShortcutContext(Qt::ApplicationShortcut);
  ensureFileTreeModel();

  showMaximized();

  auto layout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
  if (layout) {
    int tabIndex = layout->indexOf(ui->tabWidget);
    layout->removeWidget(ui->tabWidget);
    m_splitEditorContainer = new SplitEditorContainer(ui->centralwidget);
    m_splitEditorContainer->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Expanding);
    m_splitEditorContainer->adoptTabWidget(ui->tabWidget);
    layout->insertWidget(tabIndex >= 0 ? tabIndex : 0, m_splitEditorContainer);
    layout->setStretch(layout->indexOf(m_splitEditorContainer), 1);
    layout->setStretch(layout->indexOf(ui->backgroundBottom), 0);
    connect(m_splitEditorContainer, &SplitEditorContainer::currentGroupChanged,
            this, [this](LightpadTabWidget *tabWidget) {
              updateTabWidgetContext(
                  tabWidget, tabWidget ? tabWidget->currentIndex() : -1);
            });
    connect(m_splitEditorContainer, &SplitEditorContainer::splitCountChanged,
            this, [this](int) {
              for (LightpadTabWidget *tabWidget : allTabWidgets()) {
                setupTabWidgetConnections(tabWidget);
                applyTabWidgetTheme(tabWidget);
                updateTabWidgetContext(tabWidget, tabWidget->currentIndex());
              }
            });
  }

  if (m_splitEditorContainer) {
    m_splitEditorContainer->setMainWindow(this);
  } else {
  }
  setupTabWidgetConnections(ui->tabWidget);
  ui->magicButton->setIconSize(0.8 * ui->magicButton->size());
  ui->debugButton->setIconSize(0.8 * ui->debugButton->size());

  recentFilesManager = new RecentFilesManager(this);

  setupNavigationHistory();

  setupSymbolNavigation();

  setupAutoSave();

  setupCompletionSystem();

  QStringList wordList;
  wordList << "break" << "case" << "continue" << "default" << "do" << "else"
           << "for" << "if" << "return" << "switch" << "while";
  wordList << "auto" << "char" << "const" << "class" << "namespace"
           << "template" << "public" << "private" << "protected" << "virtual"
           << "override";
  wordList.sort();
  wordList.removeDuplicates();
  completer = new QCompleter(wordList, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);

  setupTextArea();
  setupTabWidget();
  setupCommandPalette();
  setupGoToLineDialog();
  setupGoToSymbolDialog();
  setupFileQuickOpen();
  setupRecentFilesDialog();
  setupBreadcrumb();
  setupGitIntegration();
  ensureDebugPanel();
  loadSettings();
  if (SettingsManager::instance()
          .getValue("showSourceControlDock", true)
          .toBool()) {
    ensureSourceControlPanel();
    if (sourceControlDock) {
      sourceControlDock->show();
    }
    if (ui->actionToggle_Source_Control) {
      ui->actionToggle_Source_Control->setChecked(true);
    }
  }
  if (debugDock) {
    debugDock->hide();
  }
  setWindowTitle("LightPad");
}

LightpadTabWidget *MainWindow::currentTabWidget() const {
  if (m_splitEditorContainer) {
    LightpadTabWidget *tabWidget = m_splitEditorContainer->currentTabWidget();
    if (tabWidget) {
      return tabWidget;
    }
  }
  return ui->tabWidget;
}

QList<LightpadTabWidget *> MainWindow::allTabWidgets() const {
  if (m_splitEditorContainer) {
    return m_splitEditorContainer->allTabWidgets();
  }
  return {ui->tabWidget};
}

void MainWindow::setRowCol(int row, int col) {
  ui->rowCol->setText("Ln " + QString::number(row) + ", Col " +
                      QString::number(col));
  ensureStatusLabels();
}

void MainWindow::connectVimMode(TextArea *textArea) {
  if (!textArea) {
    return;
  }
  VimMode *vimMode = textArea->vimMode();
  if (!vimMode) {
    return;
  }

  disconnectVimMode();
  ensureStatusLabels();
  m_connectedVimMode = vimMode;

  connect(vimMode, &VimMode::modeChanged, this,
          [this, textArea](VimEditMode mode) {
            if (!textArea || !textArea->isVimModeEnabled()) {
              updateVimStatusLabel("");
              hideVimCommandPanel();
              return;
            }
            if (mode == VimEditMode::Command) {
              showVimCommandPanel(":", textArea->vimMode()->commandBuffer());
            } else {
              hideVimCommandPanel();
            }
            updateVimStatusLabel(textArea->vimMode()->modeName());
          });

  connect(vimMode, &VimMode::statusMessage, this,
          [this](const QString &message) { showVimStatusMessage(message); });

  connect(vimMode, &VimMode::commandBufferChanged, this,
          [this, textArea](const QString &buffer) {
            if (!textArea || !textArea->isVimModeEnabled()) {
              return;
            }
            VimMode *currentVim = textArea->vimMode();
            if (!currentVim || currentVim->mode() != VimEditMode::Command) {
              return;
            }
            if (buffer.startsWith("/") || buffer.startsWith("?")) {
              showVimCommandPanel(buffer.left(1), buffer.mid(1));
            } else {
              showVimCommandPanel(":", buffer);
            }
          });

  connect(vimMode, &VimMode::commandExecuted, this,
          [this](const QString &command) {
            if (command == "save") {
              on_actionSave_triggered();
            } else if (command == "quit") {
              closeCurrentTab();
            } else if (command == "forceQuit") {
              on_actionQuit_triggered();
            } else if (command == "vim:on") {
              if (!settings.vimModeEnabled) {
                on_actionToggle_Vim_Mode_triggered();
              }
            } else if (command == "vim:off") {
              if (settings.vimModeEnabled) {
                on_actionToggle_Vim_Mode_triggered();
              }
            } else if (command == "nextTab") {
              LightpadTabWidget *tabWidget = currentTabWidget();
              if (tabWidget && tabWidget->count() > 1) {
                tabWidget->setCurrentIndex(
                    (tabWidget->currentIndex() + 1) % tabWidget->count());
              }
            } else if (command == "prevTab") {
              LightpadTabWidget *tabWidget = currentTabWidget();
              if (tabWidget && tabWidget->count() > 1) {
                int idx = tabWidget->currentIndex() - 1;
                if (idx < 0) idx = tabWidget->count() - 1;
                tabWidget->setCurrentIndex(idx);
              }
            } else if (command == "splitHorizontal") {
              on_actionSplit_Horizontally_triggered();
            } else if (command == "splitVertical") {
              on_actionSplit_Vertically_triggered();
            } else if (command.startsWith("edit:")) {
              openFileAndAddToNewTab(command.mid(QString("edit:").size()));
            }
          });

  connect(vimMode, &VimMode::pendingKeysChanged, this,
          [this, textArea](const QString &keys) {
            if (!textArea || !textArea->isVimModeEnabled())
              return;
            if (keys.isEmpty()) {
              if (vimStatusLabel)
                updateVimStatusLabel(textArea->vimMode()->modeName());
            } else {
              showVimStatusMessage(
                  textArea->vimMode()->modeName() + "  " + keys);
            }
          });

  connect(vimMode, &VimMode::macroRecordingChanged, this,
          [this](bool recording, QChar reg) {
            if (recording) {
              showVimStatusMessage(QString("recording @%1").arg(reg));
            }
          });

  connect(vimMode, &VimMode::searchHighlightRequested, this,
          [this, textArea](const QString &pattern, bool enabled) {
            if (!textArea)
              return;
            if (enabled && !pattern.isEmpty()) {
              // Convert regex pattern to plain search term for the
              // syntax highlighter (strip word boundary markers)
              QString searchTerm = pattern;
              searchTerm.remove("\\b");
              textArea->updateSyntaxHighlightTags(searchTerm);
            } else {
              // Clear search highlights
              textArea->updateSyntaxHighlightTags(QString());
            }
          });

  updateVimStatusLabel(textArea->isVimModeEnabled() ? vimMode->modeName() : "");
  if (!textArea->isVimModeEnabled()) {
    hideVimCommandPanel();
  }
}

void MainWindow::disconnectVimMode() {
  if (!m_connectedVimMode) {
    return;
  }
  m_connectedVimMode->disconnect(this);
  m_connectedVimMode = nullptr;
}

void MainWindow::showVimCommandPanel(const QString &prefix,
                                     const QString &buffer) {
  m_vimCommandPanelActive = true;
  showFindReplace(true);
  if (!findReplacePanel) {
    return;
  }
  ensureStatusLabels();
  findReplacePanel->setReplaceVisibility(false);
  findReplacePanel->setVimCommandMode(true);
  findReplacePanel->setSearchPrefix(prefix);
  findReplacePanel->setSearchText(buffer);
  findReplacePanel->setFocusOnSearchBox();
}

void MainWindow::hideVimCommandPanel() {
  if (findReplacePanel && findReplacePanel->isVimCommandMode()) {
    findReplacePanel->setVimCommandMode(false);
    findReplacePanel->close();
  }
  m_vimCommandPanelActive = false;
}

void MainWindow::setTabWidthLabel(QString text) {
  ui->tabWidth->setText(text);

  if (preferences)
    preferences->setTabWidthLabel(text);
}

void MainWindow::setLanguageHighlightLabel(QString text) {
  ui->languageHighlight->setText(text);
}

MainWindow::~MainWindow() {
  saveSettings();
  delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
  Q_UNUSED(event);

  if (preferences) {
    preferences->close();
  }
}

QString MainWindow::textAreaSettingsPath() const {
  QString settingsDir = SettingsManager::instance().getSettingsDirectory();
  if (!settingsDir.isEmpty()) {
    QDir dir;
    dir.mkpath(settingsDir);
    return QDir(settingsDir).filePath("editor_settings.json");
  }

  QString fallbackDir = QDir::home().filePath(".lightpad");
  QDir dir;
  dir.mkpath(fallbackDir);
  return QDir(fallbackDir).filePath("editor_settings.json");
}

void MainWindow::loadSettings() {
  QString editorSettingsPath = textAreaSettingsPath();
  if (QFileInfo(editorSettingsPath).exists()) {
    settings.loadSettings(editorSettingsPath);
  } else if (QFileInfo("settings.json").exists()) {

    settings.loadSettings("settings.json");
    settings.saveSettings(editorSettingsPath);
    LOG_INFO(QString("Migrated editor settings to global path: %1")
                 .arg(editorSettingsPath));
  } else {
    setTabWidth(defaultTabWidth);
  }

  SettingsManager &globalSettings = SettingsManager::instance();
  globalSettings.loadSettings();
  QString fontFamily =
      globalSettings.getValue("fontFamily", "Ubuntu Mono").toString();
  int fontSize = globalSettings.getValue("fontSize", defaultFontSize).toInt();
  int fontWeight = globalSettings.getValue("fontWeight", 50).toInt();
  bool fontItalic = globalSettings.getValue("fontItalic", false).toBool();
  settings.mainFont = QFont(fontFamily, fontSize, fontWeight, fontItalic);

  updateAllTextAreas(&TextArea::loadSettings, settings);
  setTheme(settings.theme);
  if (ui->actionToggle_Vim_Mode) {
    ui->actionToggle_Vim_Mode->setChecked(settings.vimModeEnabled);
  }

  QString lastProject =
      globalSettings.getValue("lastProjectPath", "").toString();
  if (!lastProject.isEmpty() && QDir(lastProject).exists()) {
    const QString resolvedRoot = resolveProjectRootForPath(lastProject);
    setProjectRootPath(resolvedRoot.isEmpty() ? lastProject : resolvedRoot);
    QDir::setCurrent(m_projectRootPath);
  }

  loadTreeStateFromSettings(m_projectRootPath);

  QJsonArray openTabs = globalSettings.getValue("openTabs").toJsonArray();
  for (const QJsonValue &val : openTabs) {
    QString filePath = val.toString();
    if (!filePath.isEmpty() && QFileInfo(filePath).exists()) {
      openFileAndAddToNewTab(filePath);
    }
  }

  applyTreeExpandedStateToViews();
}

void MainWindow::saveSettings() {
  LOG_DEBUG(QString("Saving settings, showLineNumberArea: %1")
                .arg(settings.showLineNumberArea));
  settings.saveSettings(textAreaSettingsPath());

  SettingsManager &globalSettings = SettingsManager::instance();
  globalSettings.setValue("lastProjectPath", m_projectRootPath);
  if (sourceControlDock) {
    globalSettings.setValue("showSourceControlDock",
                            sourceControlDock->isVisible());
  }
  if (debugDock) {
    globalSettings.setValue("showDebugDock", debugDock->isVisible());
  }

  QJsonArray openTabs;
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      QString filePath = tabWidget->getFilePath(i);
      if (!filePath.isEmpty()) {
        openTabs.append(filePath);
      }
    }
  }
  globalSettings.setValue("openTabs", openTabs);
  persistTreeStateToSettings();
  globalSettings.saveSettings();
}

void MainWindow::applyLanguageOverride(const QString &languageId) {
  auto textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty()) {
    return;
  }

  QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (canonicalLanguageId.isEmpty()) {
    LOG_WARNING(
        QString("No canonical language ID found for: %1").arg(languageId));
    return;
  }

  setHighlightOverrideForFile(filePath, canonicalLanguageId);
  textArea->updateSyntaxHighlightTags("", canonicalLanguageId);
  textArea->setLanguage(canonicalLanguageId);
  highlightLanguage = canonicalLanguageId;
  QString displayName = LanguageCatalog::displayName(canonicalLanguageId);
  setLanguageHighlightLabel(displayName.isEmpty() ? canonicalLanguageId
                                                  : displayName);
}

void MainWindow::applyHighlightForFile(const QString &filePath) {
  auto textArea = getCurrentTextArea();
  if (!textArea || filePath.isEmpty()) {
    return;
  }

  QString languageId = effectiveLanguageIdForFile(filePath);
  if (languageId.isEmpty()) {
    languageId = "plaintext";
  }

  QString extension = QFileInfo(filePath).completeSuffix().toLower();
  QString displayName = displayNameForLanguage(languageId, extension);
  if (!displayName.isEmpty()) {
    setLanguageHighlightLabel(displayName);
  }

  highlightLanguage = languageId;
  textArea->setLanguage(languageId);
  textArea->updateSyntaxHighlightTags("", languageId);

  if (m_gitIntegration) {
    QList<GitDiffLineInfo> diffLines = m_gitIntegration->getDiffLines(filePath);
    QList<QPair<int, int>> gutterLines;
    gutterLines.reserve(diffLines.size());
    for (const auto &info : diffLines) {
      int type = 1;
      if (info.type == GitDiffLineInfo::Type::Added) {
        type = 0;
      } else if (info.type == GitDiffLineInfo::Type::Deleted) {
        type = 2;
      }
      gutterLines.append(qMakePair(info.lineNumber, type));
    }
    textArea->setGitDiffLines(gutterLines);
  } else {
    textArea->clearGitDiffLines();
  }

  showGitBlameForCurrentFile(isGitBlameEnabledForFile(filePath));
  updateInlineBlameForCurrentFile();
  updateGitStatusBar();
}

QString MainWindow::effectiveLanguageIdForFile(const QString &filePath) {
  QString overrideLanguageId = highlightOverrideForFile(filePath);
  if (!overrideLanguageId.isEmpty()) {
    QString canonicalOverride = LanguageCatalog::normalize(overrideLanguageId);
    if (!canonicalOverride.isEmpty()) {
      return canonicalOverride;
    }
  }

  QString detectedLanguageId =
      LanguageCatalog::normalize(detectLanguageIdForFile(filePath));
  if (!detectedLanguageId.isEmpty()) {
    return detectedLanguageId;
  }

  return "plaintext";
}

void MainWindow::showGitBlameForCurrentFile(bool enable) {
  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  QString filePath;
  QObject *parent = textArea;
  while (parent && filePath.isEmpty()) {
    if (auto *page = qobject_cast<LightpadPage *>(parent)) {
      filePath = page->getFilePath();
      break;
    }
    parent = parent->parent();
  }
  if (filePath.isEmpty()) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    filePath = tabWidget ? tabWidget->getFilePath(tabWidget->currentIndex())
                         : QString();
  }
  if (!enable || !m_gitIntegration || filePath.isEmpty()) {
    textArea->clearGitBlameLines();
    return;
  }

  QList<GitBlameLineInfo> blameLines = m_gitIntegration->getBlameInfo(filePath);
  QMap<int, QString> blameMap;
  QMap<int, GitBlameLineInfo> richBlameMap;
  for (const auto &info : blameLines) {
    QString label =
        QString("%1 \u2022 %2").arg(info.shortHash).arg(info.author);
    blameMap.insert(info.lineNumber, label);
    richBlameMap.insert(info.lineNumber, info);
  }
  textArea->setGitBlameLines(blameMap);
  textArea->setRichBlameData(richBlameMap);
  textArea->setGutterGitIntegration(m_gitIntegration);
}

bool MainWindow::isGitBlameEnabledForFile(const QString &filePath) const {
  return !filePath.isEmpty() && m_blameEnabledFiles.contains(filePath);
}

void MainWindow::setGitBlameEnabledForFile(const QString &filePath,
                                           bool enabled) {
  if (filePath.isEmpty()) {
    return;
  }

  if (enabled) {
    m_blameEnabledFiles.insert(filePath);
  } else {
    m_blameEnabledFiles.remove(filePath);
  }
}

void MainWindow::updateInlineBlameForCurrentFile() {
  TextArea *textArea = getCurrentTextArea();
  if (!textArea || !m_gitIntegration || !m_inlineBlameEnabled) {
    if (textArea)
      textArea->clearInlineBlameData();
    return;
  }

  QString filePath;
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget) {
    filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  }

  if (filePath.isEmpty() || !m_gitIntegration->isValidRepository()) {
    textArea->clearInlineBlameData();
    return;
  }

  QList<GitBlameLineInfo> blameLines = m_gitIntegration->getBlameInfo(filePath);
  QMap<int, QString> inlineData;
  for (const auto &info : blameLines) {
    QString text = QString("%1, %2 \u2022 %3")
                       .arg(info.author)
                       .arg(info.relativeDate)
                       .arg(info.summary);
    inlineData.insert(info.lineNumber, text);
  }
  textArea->setInlineBlameEnabled(true);
  textArea->setInlineBlameData(inlineData);
}

void MainWindow::updateGitStatusBar() {
  if (!m_gitIntegration) {
    return;
  }

  if (!m_gitIntegration->isValidRepository()) {
    m_gitBranchLabel->clear();
    m_gitSyncLabel->clear();
    m_gitDirtyLabel->clear();
    return;
  }

  QString branch = m_gitIntegration->currentBranch();
  m_gitBranchLabel->setText(
      QString("\xF0\x9F\x94\x80 %1").arg(branch.isEmpty() ? "HEAD" : branch));

  int ahead = 0, behind = 0;
  if (m_gitIntegration->getAheadBehind(ahead, behind)) {
    QString syncText;
    if (ahead > 0)
      syncText += QString("\u2191%1").arg(ahead);
    if (behind > 0) {
      if (!syncText.isEmpty())
        syncText += " ";
      syncText += QString("\u2193%1").arg(behind);
    }
    if (syncText.isEmpty())
      syncText = "\u2713";
    m_gitSyncLabel->setText(syncText);
    m_gitSyncLabel->setToolTip(
        QString("Ahead: %1, Behind: %2").arg(ahead).arg(behind));
  } else {
    m_gitSyncLabel->clear();
  }

  bool dirty = m_gitIntegration->isDirty();
  m_gitDirtyLabel->setText(dirty ? "\u25CF" : "");
  m_gitDirtyLabel->setToolTip(dirty ? tr("Uncommitted changes")
                                    : tr("Working tree clean"));
}

QString
MainWindow::detectLanguageIdForExtension(const QString &extension) const {
  return LanguageCatalog::languageForExtension(extension);
}

QString MainWindow::detectLanguageIdForFile(const QString &filePath) const {
  const QFileInfo info(filePath);
  const QString fileName = info.fileName();
  const QString fileNameLower = fileName.toLower();
  if (fileNameLower == "makefile" || fileNameLower == "gnumakefile") {
    return "make";
  }
  if (fileName == "BUILD" || fileName == "WORKSPACE" ||
      fileNameLower == "build.bazel" || fileNameLower == "workspace.bazel" ||
      fileNameLower == "module.bazel") {
    return "bazel";
  }
  if (fileNameLower == "meson.build" || fileNameLower == "meson_options.txt") {
    return "meson";
  }
  if (fileNameLower == "build.ninja") {
    return "ninja";
  }
  if (fileNameLower == "cmakelists.txt") {
    return "cmake";
  }
  return detectLanguageIdForExtension(info.completeSuffix().toLower());
}

QString MainWindow::displayNameForLanguage(const QString &languageId,
                                           const QString &extension) const {
  Q_UNUSED(extension);
  QString displayName = LanguageCatalog::displayName(languageId);
  if (!displayName.isEmpty()) {
    return displayName;
  }
  return languageId;
}

void MainWindow::loadHighlightOverridesForDir(const QString &dirPath) {
  if (dirPath.isEmpty()) {
    return;
  }

  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/highlight_config.json";
  if (m_loadedHighlightOverrideDirs.contains(configDir)) {
    return;
  }

  if (!QFileInfo(configFile).exists()) {
    m_loadedHighlightOverrideDirs.insert(configDir);
    return;
  }

  QFile file(configFile);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARNING(QString("Failed to open highlight config: %1").arg(configFile));
    return;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    LOG_WARNING(QString("Failed to parse highlight config: %1")
                    .arg(parseError.errorString()));
    return;
  }

  QJsonObject root = doc.object();
  QJsonArray assignments = root.value("assignments").toArray();
  for (const QJsonValue &value : assignments) {
    QJsonObject obj = value.toObject();
    QString fileName = obj.value("file").toString();
    QString languageId =
        LanguageCatalog::normalize(obj.value("language").toString());
    if (fileName.isEmpty() || languageId.isEmpty()) {
      continue;
    }
    QString absolutePath = dirPath + "/" + fileName;
    m_highlightOverrides[absolutePath] = languageId;
  }

  m_loadedHighlightOverrideDirs.insert(configDir);
  LOG_INFO(QString("Loaded %1 highlight overrides from %2")
               .arg(assignments.size())
               .arg(configFile));
}

bool MainWindow::saveHighlightOverridesForDir(const QString &dirPath) const {
  if (dirPath.isEmpty()) {
    return false;
  }

  QString configDir = dirPath + "/.lightpad";
  QString configFile = configDir + "/highlight_config.json";

  QJsonArray assignments;
  for (auto it = m_highlightOverrides.begin(); it != m_highlightOverrides.end();
       ++it) {
    QFileInfo fileInfo(it.key());
    if (fileInfo.absoluteDir().path() == dirPath) {
      QJsonObject obj;
      obj["file"] = fileInfo.fileName();
      obj["language"] = it.value();
      assignments.append(obj);
    }
  }

  QDir dir;
  if (!dir.exists(configDir)) {
    if (!dir.mkpath(configDir)) {
      LOG_ERROR(
          QString("Failed to create config directory: %1").arg(configDir));
      return false;
    }
  }

  QJsonObject root;
  root["version"] = "1.0";
  root["assignments"] = assignments;

  QFile file(configFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to write highlight config: %1").arg(configFile));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  LOG_INFO(QString("Saved %1 highlight overrides to %2")
               .arg(assignments.size())
               .arg(configFile));
  return true;
}

QString MainWindow::highlightOverrideForFile(const QString &filePath) {
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists()) {
    return QString();
  }

  loadHighlightOverridesForDir(fileInfo.absoluteDir().path());
  auto it = m_highlightOverrides.find(filePath);
  if (it != m_highlightOverrides.end()) {
    return it.value();
  }
  return QString();
}

void MainWindow::setHighlightOverrideForFile(const QString &filePath,
                                             const QString &languageId) {
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists()) {
    return;
  }

  QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (canonicalLanguageId.isEmpty()) {
    m_highlightOverrides.remove(filePath);
  } else {
    m_highlightOverrides[filePath] = canonicalLanguageId;
  }
  saveHighlightOverridesForDir(fileInfo.absoluteDir().path());
}

void MainWindow::ensureProjectSettings(const QString &path) {
  if (path.isEmpty()) {
    return;
  }

  QFileInfo rootInfo(path);
  if (!rootInfo.exists() || !rootInfo.isDir()) {
    LOG_WARNING(
        QString("Skipping project settings initialization for invalid path: %1")
            .arg(path));
    return;
  }

  QDir rootDir(path);
  QFileInfo configInfo(rootDir.filePath(".lightpad"));
  if (configInfo.exists() && !configInfo.isDir()) {
    LOG_ERROR(QString(".lightpad exists but is not a directory: %1")
                  .arg(configInfo.absoluteFilePath()));
    return;
  }

  if (!rootDir.exists(".lightpad")) {
    if (!rootDir.mkpath(".lightpad")) {
      LOG_ERROR(QString("Failed to create project config directory: %1")
                    .arg(path + "/.lightpad"));
      return;
    }
  }

  QString configDir = rootDir.filePath(".lightpad");

  QString highlightConfigPath = configDir + "/highlight_config.json";
  if (!QFileInfo(highlightConfigPath).exists()) {
    saveHighlightOverridesForDir(path);
  }

  DebugSettings::instance().initialize(path);
}

QString MainWindow::resolveProjectRootForPath(const QString &path) const {
  if (path.isEmpty()) {
    return QString();
  }

  QFileInfo pathInfo(path);
  QString startDirPath;
  if (pathInfo.exists() && pathInfo.isDir()) {
    startDirPath = pathInfo.absoluteFilePath();
  } else if (pathInfo.exists()) {
    startDirPath = pathInfo.absolutePath();
  } else {
    QFileInfo absoluteInfo(QDir::current().absoluteFilePath(path));
    startDirPath = absoluteInfo.isDir() ? absoluteInfo.absoluteFilePath()
                                        : absoluteInfo.absolutePath();
  }

  if (startDirPath.isEmpty()) {
    return QString();
  }

  QDir dir(startDirPath);
  QString outermostLightpadRoot;

  while (dir.exists()) {
    QFileInfo gitInfo(dir.filePath(".git"));
    if (gitInfo.exists()) {
      return QDir::cleanPath(dir.absolutePath());
    }

    QFileInfo lightpadInfo(dir.filePath(".lightpad"));
    if (lightpadInfo.exists() && lightpadInfo.isDir()) {
      outermostLightpadRoot = dir.absolutePath();
    }

    if (!dir.cdUp()) {
      break;
    }
  }

  if (!outermostLightpadRoot.isEmpty()) {
    return QDir::cleanPath(outermostLightpadRoot);
  }

  return QDir::cleanPath(startDirPath);
}

bool MainWindow::isPathWithinRoot(const QString &path,
                                  const QString &rootPath) const {
  if (path.isEmpty() || rootPath.isEmpty()) {
    return false;
  }

  QFileInfo pathInfo(path);
  QString normalizedPath =
      pathInfo.isDir() ? pathInfo.absoluteFilePath() : pathInfo.absolutePath();
  normalizedPath = QDir::cleanPath(normalizedPath);
  const QString normalizedRoot = QDir::cleanPath(rootPath);

  if (normalizedPath == normalizedRoot) {
    return true;
  }

#ifdef Q_OS_WIN
  return normalizedPath.startsWith(normalizedRoot + "/") ||
         normalizedPath.startsWith(normalizedRoot + "\\");
#else
  return normalizedPath.startsWith(normalizedRoot + "/");
#endif
}

void MainWindow::ensureProjectRootForPath(const QString &path) {
  const QString resolvedRoot = resolveProjectRootForPath(path);
  if (resolvedRoot.isEmpty()) {
    return;
  }

  const QString normalizedCurrent = QDir::cleanPath(m_projectRootPath);
  const QString normalizedResolved = QDir::cleanPath(resolvedRoot);
  if (normalizedCurrent == normalizedResolved) {
    return;
  }

  QFileInfo gitInfo(normalizedResolved + "/.git");
  const bool shouldPromoteToGitRoot =
      gitInfo.exists() &&
      (!normalizedCurrent.isEmpty() &&
       isPathWithinRoot(normalizedCurrent, normalizedResolved));

  if (!shouldPromoteToGitRoot && !normalizedCurrent.isEmpty() &&
      isPathWithinRoot(path, normalizedCurrent)) {
    return;
  }

  setProjectRootPath(normalizedResolved);
}

template <typename... Args>
void MainWindow::updateAllTextAreas(void (TextArea::*f)(Args... args),
                                    Args... args) {
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    auto textAreas = tabWidget->findChildren<TextArea *>();
    for (auto &textArea : textAreas)
      (textArea->*f)(args...);
  }

  if (ui->actionToggle_Vim_Mode) {
    ui->actionToggle_Vim_Mode->setChecked(settings.vimModeEnabled);
  }
}

void MainWindow::updateAllTextAreas(void (TextArea::*f)(const Theme &),
                                    const Theme &theme) {
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    auto textAreas = tabWidget->findChildren<TextArea *>();
    for (auto &textArea : textAreas)
      (textArea->*f)(theme);
  }
}

void MainWindow::keyPressEvent(QKeyEvent *keyEvent) {

  if (keyEvent->matches(QKeySequence::Undo))
    undo();

  else if (keyEvent->matches(QKeySequence::Redo))
    redo();

  else if (keyEvent->matches(QKeySequence::ZoomIn))
    on_actionIncrease_Font_Size_triggered();

  else if (keyEvent->matches(QKeySequence::ZoomOut))
    on_actionDecrease_Font_Size_triggered();

  else if (keyEvent->matches(QKeySequence::Save))
    on_actionSave_triggered();

  else if (keyEvent->matches(QKeySequence::SaveAs))
    on_actionSave_as_triggered();

  else if (keyEvent->matches(QKeySequence::Find))
    showFindReplace();

  else if (keyEvent->matches(QKeySequence::Replace))
    showFindReplace(false);

  else if (keyEvent->key() == Qt::Key_Escape && m_vimCommandPanelActive) {
    hideVimCommandPanel();
  }

  else if (keyEvent->matches(QKeySequence::Close))
    closeCurrentTab();

  else if (keyEvent->matches(QKeySequence::AddTab))
    currentTabWidget()->addNewTab();

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_P) {
    showCommandPalette();
  }

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_M) {
    showProblemsPanel();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_G) {
    showGoToLineDialog();
  }

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_O) {
    showGoToSymbolDialog();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_P) {
    showFileQuickOpen();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_E) {
    showRecentFilesDialog();
  }

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_W) {
    toggleShowWhitespace();
  }

  else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
           keyEvent->key() == Qt::Key_I) {
    toggleShowIndentGuides();
  }

  else if (keyEvent->key() == Qt::Key_F12 &&
           keyEvent->modifiers() == Qt::NoModifier) {
    goToDefinitionAtCursor();
  }

  else if (keyEvent->modifiers() == Qt::ControlModifier &&
           keyEvent->key() == Qt::Key_B) {
    goToDefinitionAtCursor();
  }

  else if (keyEvent->modifiers() == Qt::AltModifier &&
           keyEvent->key() == Qt::Key_Left) {
    navigateBack();
  }

  else if (keyEvent->modifiers() == Qt::AltModifier &&
           keyEvent->key() == Qt::Key_Right) {
    navigateForward();
  }
}

int MainWindow::getTabWidth() { return settings.tabWidth; }

int MainWindow::getFontSize() { return settings.mainFont.pointSize(); }

void MainWindow::openFileAndAddToNewTab(QString filePath) {
  LightpadTabWidget *tabWidget = currentTabWidget();

  QFileInfo fileInfo(filePath);
  if (filePath.isEmpty() || !fileInfo.exists() || fileInfo.isDir())
    return;

  if (m_projectRootPath.isEmpty()) {
    updateGitIntegrationForPath(filePath);
  }

  for (int i = 0; i < tabWidget->count(); i++) {
    QString tabFilePath = tabWidget->getFilePath(i);
    if (tabFilePath == filePath) {
      tabWidget->setCurrentIndex(i);
      return;
    }
  }

  QString extension = fileInfo.suffix().toLower();

  if (ImageViewer::isSupportedImageFormat(extension)) {
    ImageViewer *imageViewer = new ImageViewer(this);
    if (imageViewer->loadImage(filePath)) {
      tabWidget->addViewerTab(imageViewer, filePath, m_projectRootPath);
    } else {
      delete imageViewer;
    }
    return;
  }

#ifdef HAVE_PDF_SUPPORT

  if (PdfViewer::isSupportedPdfFormat(extension)) {
    PdfViewer *pdfViewer = new PdfViewer(this);
    if (pdfViewer->loadPdf(filePath)) {
      tabWidget->addViewerTab(pdfViewer, filePath, m_projectRootPath);
    } else {
      delete pdfViewer;
    }
    return;
  }
#endif

  TextArea *currentTextArea = getCurrentTextArea();
  bool currentIsViewer = tabWidget->isViewerTab(tabWidget->currentIndex());
  if (tabWidget->count() == 0 || currentIsViewer || !currentTextArea ||
      !currentTextArea->toPlainText().isEmpty()) {
    tabWidget->addNewTab();
  }

  open(filePath);
  setFilePathAsTabText(filePath);

  auto page = tabWidget->getCurrentPage();

  if (page) {

    if (!m_projectRootPath.isEmpty()) {
      page->setProjectRootPath(m_projectRootPath);
      page->setModelRootIndex(m_projectRootPath);
    }
    page->setTreeViewVisible(!m_projectRootPath.isEmpty());
    page->setFilePath(filePath);
  }

  if (getCurrentTextArea())
    applyHighlightForFile(filePath);

  if (recentFilesManager) {
    recentFilesManager->addFile(filePath);
  }

  updateBreadcrumb(filePath);

  tabWidget->currentChanged(tabWidget->currentIndex());
}

void MainWindow::closeTabPage(QString filePath) {
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      if (tabWidget->getFilePath(i) == filePath) {
        tabWidget->removeTab(i);
      }
    }
  }
}

void MainWindow::on_actionToggle_Full_Screen_triggered() {
  const auto state = windowState();
  if (state & Qt::WindowFullScreen)
    setWindowState(state & ~Qt::WindowFullScreen);
  else
    setWindowState(state | Qt::WindowFullScreen);
}

void MainWindow::on_actionQuit_triggered() { close(); }

void MainWindow::undo() {
  if (getCurrentTextArea())
    getCurrentTextArea()->undo();
}

void MainWindow::redo() {
  if (getCurrentTextArea())
    getCurrentTextArea()->redo();
}

TextArea *MainWindow::getCurrentTextArea() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget->currentWidget()->findChild<LightpadPage *>("widget"))
    return tabWidget->currentWidget()
        ->findChild<LightpadPage *>("widget")
        ->getTextArea();

  else if (tabWidget->currentWidget()->findChild<TextArea *>(""))
    return tabWidget->currentWidget()->findChild<TextArea *>("");

  return nullptr;
}

Theme MainWindow::getTheme() { return settings.theme; }

QFont MainWindow::getFont() { return settings.mainFont; }

TextAreaSettings MainWindow::getSettings() { return settings; }

void MainWindow::setTabWidth(int width) {
  updateAllTextAreas(&TextArea::setTabWidth, width);
  settings.tabWidth = width;
}

void MainWindow::on_actionToggle_Undo_triggered() { undo(); }

void MainWindow::on_actionToggle_Redo_triggered() { redo(); }

void MainWindow::on_actionIncrease_Font_Size_triggered() {
  updateAllTextAreas(&TextArea::increaseFontSize);
  settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
  SettingsManager::instance().setValue("fontSize",
                                       settings.mainFont.pointSize());
  SettingsManager::instance().saveSettings();
}

void MainWindow::on_actionDecrease_Font_Size_triggered() {
  updateAllTextAreas(&TextArea::decreaseFontSize);
  settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
  SettingsManager::instance().setValue("fontSize",
                                       settings.mainFont.pointSize());
  SettingsManager::instance().saveSettings();
}

void MainWindow::on_actionReset_Font_Size_triggered() {
  updateAllTextAreas(&TextArea::setFontSize, defaultFontSize);
  settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
  SettingsManager::instance().setValue("fontSize",
                                       settings.mainFont.pointSize());
  SettingsManager::instance().saveSettings();
}

void MainWindow::on_actionCut_triggered() {

  if (getCurrentTextArea())
    getCurrentTextArea()->cut();
}

void MainWindow::on_actionCopy_triggered() {

  if (getCurrentTextArea())
    getCurrentTextArea()->copy();
}

void MainWindow::on_actionPaste_triggered() {

  if (getCurrentTextArea())
    getCurrentTextArea()->paste();
}

void MainWindow::on_actionNew_Window_triggered() { new MainWindow(); }

void MainWindow::on_actionClose_Tab_triggered() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget->currentIndex() > -1)
    tabWidget->removeTab(tabWidget->currentIndex());
}

void MainWindow::on_actionClose_All_Tabs_triggered() {
  currentTabWidget()->closeAllTabs();
}

void MainWindow::on_actionFind_in_file_triggered() {
  showFindReplace(true);
  if (findReplacePanel) {
    findReplacePanel->setGlobalMode(false);
    // Pre-populate with vim's last search pattern if available
    TextArea *textArea = getCurrentTextArea();
    if (textArea && textArea->isVimModeEnabled() && textArea->vimMode()) {
      QString vimPattern = textArea->vimMode()->searchPattern();
      if (!vimPattern.isEmpty()) {
        QString searchTerm = vimPattern;
        searchTerm.remove("\\b");
        findReplacePanel->setSearchText(searchTerm);
      }
    }
    findReplacePanel->setFocusOnSearchBox();
  }
}

void MainWindow::on_actionFind_in_project_triggered() {
  showFindReplace(true);
  if (!findReplacePanel) {
    return;
  }

  QString projectPath = m_projectRootPath;
  if (projectPath.isEmpty()) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    QString filePath = tabWidget
                           ? tabWidget->getFilePath(tabWidget->currentIndex())
                           : QString();
    if (!filePath.isEmpty()) {
      projectPath = QFileInfo(filePath).absolutePath();
    } else {
      projectPath = QDir::currentPath();
    }
  }

  findReplacePanel->setProjectPath(projectPath);
  findReplacePanel->setGlobalMode(true);
  findReplacePanel->setFocusOnSearchBox();
}

void MainWindow::on_actionNew_File_triggered() {
  currentTabWidget()->addNewTab();
}

void MainWindow::on_actionOpen_File_triggered() {
  auto filePath =
      QFileDialog::getOpenFileName(this, tr("Open Document"), QDir::homePath());

  openFileAndAddToNewTab(filePath);
}

void MainWindow::on_actionOpen_Project_triggered() {
  QString folderPath = QFileDialog::getExistingDirectory(
      this, tr("Open Project"), QDir::homePath(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (folderPath.isEmpty()) {
    return;
  }

  const QString normalizedCurrentRoot = QDir::cleanPath(m_projectRootPath);
  const QString normalizedNextRoot = QDir::cleanPath(folderPath);
  if (!normalizedCurrentRoot.isEmpty() &&
      normalizedCurrentRoot != normalizedNextRoot) {
    for (LightpadTabWidget *tabWidget : allTabWidgets()) {
      if (tabWidget) {
        tabWidget->closeAllTabs();
      }
    }
  }

  setProjectRootPath(folderPath);

  QDir::setCurrent(folderPath);
  setMainWindowTitle(QFileInfo(folderPath).fileName());
  if (fileQuickOpen) {
    fileQuickOpen->setRootDirectory(folderPath);
  }
}

void MainWindow::on_actionSave_triggered() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  auto filePath = tabWidget->getFilePath(tabIndex);

  if (filePath.isEmpty()) {
    on_actionSave_as_triggered();
    return;
  }

  save(filePath);
}

void MainWindow::on_actionSave_as_triggered() {

  auto filePath =
      QFileDialog::getSaveFileName(this, tr("Save Document"), QDir::homePath());

  if (filePath.isEmpty())
    return;

  LightpadTabWidget *tabWidget = currentTabWidget();
  int tabIndex = tabWidget->currentIndex();
  tabWidget->setFilePath(tabIndex, filePath);

  save(filePath);
}

void MainWindow::open(const QString &filePath) {

  QFile file(filePath);

  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::critical(this, tr("Error"), tr("Can't open file."));
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  int tabIndex = tabWidget->currentIndex();
  tabWidget->setFilePath(tabIndex, filePath);

  if (getCurrentTextArea()) {
    auto *textArea = getCurrentTextArea();
    textArea->setPlainText(QString::fromUtf8(file.readAll()));
    textArea->moveCursor(QTextCursor::Start);
    textArea->centerCursor();
  }
}

void MainWindow::save(const QString &filePath) {
  QFile file(filePath);

  if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
    return;

  if (getCurrentTextArea()) {
    TextArea *textArea = getCurrentTextArea();

    SettingsManager &sm = SettingsManager::instance();
    if (sm.getValue("trimTrailingWhitespace", false).toBool()) {
      trimTrailingWhitespace(textArea);
    }
    if (sm.getValue("insertFinalNewline", false).toBool()) {
      ensureFinalNewline(textArea);
    }

    LightpadTabWidget *tabWidget = currentTabWidget();
    auto tabIndex = tabWidget->currentIndex();
    tabWidget->setFilePath(tabIndex, filePath);

    file.write(textArea->toPlainText().toUtf8());
    textArea->document()->setModified(false);
    textArea->removeIconUnsaved();
    setFilePathAsTabText(filePath);

    if (problemsPanel) {
      problemsPanel->onFileSaved(filePath);
    }
  }
}

void MainWindow::trimTrailingWhitespace(TextArea *textArea) {
  if (!textArea) {
    return;
  }

  QTextCursor cursor(textArea->document());
  cursor.beginEditBlock();

  QTextBlock block = textArea->document()->firstBlock();
  while (block.isValid()) {
    QString text = block.text();
    int originalLength = text.length();

    int i = originalLength - 1;
    while (i >= 0 && (text[i] == ' ' || text[i] == '\t')) {
      --i;
    }

    int newLength = i + 1;
    if (newLength < originalLength) {
      cursor.setPosition(block.position() + newLength);
      cursor.setPosition(block.position() + originalLength,
                         QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
    }

    block = block.next();
  }

  cursor.endEditBlock();
}

void MainWindow::ensureFinalNewline(TextArea *textArea) {
  if (!textArea) {
    return;
  }

  QString text = textArea->toPlainText();
  if (!text.isEmpty() && !text.endsWith('\n')) {
    QTextCursor cursor(textArea->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n");
  }
}

void MainWindow::showFindReplace(bool onlyFind) {
  if (!findReplacePanel) {
    findReplacePanel = new FindReplacePanel(onlyFind);

    auto layout = qobject_cast<QBoxLayout *>(ui->centralwidget->layout());

    if (layout != 0)
      layout->insertWidget(layout->count() - 1, findReplacePanel, 0);

    connect(findReplacePanel, &FindReplacePanel::navigateToFile, this,
            [this](const QString &filePath, int lineNumber, int columnNumber) {
              if (!filePath.isEmpty()) {
                openFileAndAddToNewTab(filePath);
              }
              TextArea *textArea = getCurrentTextArea();
              if (textArea) {
                QTextCursor cursor = textArea->textCursor();
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                    qMax(0, lineNumber - 1));
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                    qMax(0, columnNumber - 1));
                textArea->setTextCursor(cursor);
                textArea->setFocus();
              }
            });

    connect(findReplacePanel, &QObject::destroyed, this, [this]() {
      findReplacePanel = nullptr;
      m_vimCommandPanelActive = false;
    });
  }

  bool targetOnlyFind = m_vimCommandPanelActive ? true : onlyFind;
  findReplacePanel->setVisible(true);
  findReplacePanel->setOnlyFind(targetOnlyFind);

  if (findReplacePanel->isVisible() && getCurrentTextArea()) {
    findReplacePanel->setReplaceVisibility(!targetOnlyFind);
  }

  if (findReplacePanel->isVisible()) {
    findReplacePanel->setTextArea(getCurrentTextArea());
    QString projectPath = m_projectRootPath;
    if (projectPath.isEmpty()) {
      LightpadTabWidget *tabWidget = currentTabWidget();
      QString filePath = tabWidget
                             ? tabWidget->getFilePath(tabWidget->currentIndex())
                             : QString();
      projectPath = filePath.isEmpty() ? QDir::currentPath()
                                       : QFileInfo(filePath).absolutePath();
    }
    findReplacePanel->setProjectPath(projectPath);
    findReplacePanel->setMainWindow(this);
    findReplacePanel->setFocusOnSearchBox();
  }
}

void MainWindow::openDialog(Dialog dialog) {
  switch (dialog) {
  case Dialog::runConfiguration: {

    auto page = currentTabWidget()->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();

    if (filePath.isEmpty()) {
      QMessageBox::information(
          this, "Run Configuration",
          "Please open a file first to configure run settings.");
      return;
    }

    ensureProjectRootForPath(filePath);

    if (findChildren<RunTemplateSelector *>().isEmpty()) {
      auto selector = new RunTemplateSelector(filePath, this);
      selector->setAttribute(Qt::WA_DeleteOnClose);
      selector->show();
    }
    break;
  }

  case Dialog::formatConfiguration: {

    auto page = currentTabWidget()->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();

    if (filePath.isEmpty()) {
      QMessageBox::information(
          this, "Format Configuration",
          "Please open a file first to configure format settings.");
      return;
    }

    ensureProjectRootForPath(filePath);

    if (findChildren<FormatTemplateSelector *>().isEmpty()) {
      auto selector = new FormatTemplateSelector(filePath, this);
      selector->setAttribute(Qt::WA_DeleteOnClose);
      selector->show();
    }
    break;
  }

  case Dialog::debugConfiguration: {
    auto page = currentTabWidget()->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();

    if (filePath.isEmpty()) {
      QMessageBox::information(
          this, "Debug Configurations",
          "Please open a file first to configure debug settings.");
      return;
    }
    ensureProjectRootForPath(filePath);

    DebugSettings::instance().initialize(m_projectRootPath);
    DebugConfigurationManager::instance().setWorkspaceFolder(m_projectRootPath);
    DebugConfigurationManager::instance().loadFromLightpadDir();

    if (findChildren<DebugConfigurationDialog *>().isEmpty()) {
      auto dialog = new DebugConfigurationDialog(this);
      dialog->setAttribute(Qt::WA_DeleteOnClose);
      dialog->show();
    }
    break;
  }

  case Dialog::shortcuts:
    if (findChildren<ShortcutsDialog *>().isEmpty())
      new ShortcutsDialog(this);
    break;

  default:
    return;
  }
}

void MainWindow::openConfigurationDialog() {
  openDialog(Dialog::runConfiguration);
}

void MainWindow::openFormatConfigurationDialog() {
  openDialog(Dialog::formatConfiguration);
}

void MainWindow::openDebugConfigurationDialog() {
  openDialog(Dialog::debugConfiguration);
}

void MainWindow::openShortcutsDialog() { openDialog(Dialog::shortcuts); }

TerminalTabWidget *MainWindow::ensureTerminalWidget() {
  if (!terminalWidget) {
    terminalWidget = new TerminalTabWidget();
    terminalWidget->applyTheme(settings.theme);

    connect(terminalWidget, &TerminalTabWidget::closeRequested, this, [this]() {
      if (terminalWidget) {
        terminalWidget->hide();
      }
      if (ui->actionToggle_Terminal) {
        ui->actionToggle_Terminal->setChecked(false);
      }
    });

    auto layout = qobject_cast<QBoxLayout *>(ui->centralwidget->layout());
    if (layout != nullptr) {
      layout->insertWidget(layout->count() - 1, terminalWidget, 0);
    }
  }

  return terminalWidget;
}

void MainWindow::showTerminalPanel() {
  TerminalTabWidget *widget = ensureTerminalWidget();
  if (!widget) {
    return;
  }

  widget->show();
  if (ui->actionToggle_Terminal) {
    ui->actionToggle_Terminal->setChecked(true);
  }
}

void MainWindow::showTerminal() {
  auto page = currentTabWidget()->getCurrentPage();
  QString filePath = page ? page->getFilePath() : QString();

  if (filePath.isEmpty()) {
    noScriptAssignedWarning();
    return;
  }

  ensureProjectRootForPath(filePath);

  showTerminalPanel();
  terminalWidget->runFile(filePath, effectiveLanguageIdForFile(filePath));
}

void MainWindow::showProblemsPanel() {
  if (m_vimCommandPanelActive) {
    ensureStatusLabels();
    return;
  }
  if (!problemsPanel) {
    problemsPanel = new ProblemsPanel(this);

    connect(problemsPanel, &ProblemsPanel::problemClicked, this,
            [this](const QString &filePath, int line, int column) {
              openFileAndAddToNewTab(filePath);
              TextArea *textArea = getCurrentTextArea();
              if (textArea) {
                QTextCursor cursor = textArea->textCursor();
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                    line);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                    column);
                textArea->setTextCursor(cursor);
                textArea->setFocus();
              }
            });

    connect(problemsPanel, &ProblemsPanel::countsChanged, this,
            &MainWindow::updateProblemsStatusLabel);

    ensureStatusLabels();

    auto layout = qobject_cast<QBoxLayout *>(ui->centralwidget->layout());
    if (layout != 0)
      layout->insertWidget(layout->count() - 1, problemsPanel, 0);
  }

  if (!m_vimCommandPanelActive) {
    problemsPanel->setVisible(!problemsPanel->isVisible());
  }
}

void MainWindow::ensureStatusLabels() {
  if (!problemsStatusLabel) {
    problemsStatusLabel = new QLabel(this);
    problemsStatusLabel->setStyleSheet("color: #9aa4b2; padding: 0 8px;");
    problemsStatusLabel->setText("✓ No problems");
    problemsStatusLabel->setCursor(Qt::PointingHandCursor);

    problemsStatusLabel->installEventFilter(this);

    auto layout = qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout());
    if (layout) {

      layout->insertWidget(layout->count() - 1, problemsStatusLabel);
    }
  }

  if (!vimStatusLabel) {
    vimStatusLabel = new QLabel(this);
    vimStatusLabel->setStyleSheet(
        "QLabel {"
        "  color: #ffffff;"
        "  background-color: #3fb950;"
        "  padding: 1px 10px;"
        "  border-radius: 3px;"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  letter-spacing: 1px;"
        "}");
    vimStatusLabel->setText("");
    vimStatusLabel->setVisible(false);
    vimStatusLabel->setMinimumWidth(70);
    vimStatusLabel->setAlignment(Qt::AlignCenter);

    auto layout = qobject_cast<QHBoxLayout *>(ui->backgroundBottom->layout());
    if (layout) {
      int insertIndex = layout->count() - 1;
      if (problemsStatusLabel) {
        insertIndex = qMax(0, layout->indexOf(problemsStatusLabel) + 1);
      }
      layout->insertWidget(insertIndex, vimStatusLabel);
    }
  }
}

void MainWindow::ensureSourceControlPanel() {
  if (sourceControlDock) {
    return;
  }

  sourceControlPanel = new SourceControlPanel(this);
  sourceControlPanel->setGitIntegration(m_gitIntegration);
  sourceControlPanel->setWorkingPath(
      m_projectRootPath.isEmpty() ? QDir::currentPath() : m_projectRootPath);

  connect(
      sourceControlPanel, &SourceControlPanel::fileOpenRequested, this,
      [this](const QString &filePath) { openFileAndAddToNewTab(filePath); });
  connect(sourceControlPanel, &SourceControlPanel::diffRequested, this,
          [this](const QString &filePath, bool staged) {
            if (!m_gitIntegration) {
              return;
            }
            QString diff = m_gitIntegration->getFileDiff(filePath, staged);
            if (diff.trimmed().isEmpty()) {
              QMessageBox::information(this, tr("Diff"),
                                       tr("No changes to show for this file."));
              return;
            }

            GitDiffDialog dialog(m_gitIntegration, filePath,
                                 GitDiffDialog::DiffTarget::File, staged,
                                 getTheme(), this);
            dialog.setWindowTitle(staged ? tr("Staged Diff")
                                         : tr("Unstaged Diff"));
            dialog.setDiffText(diff);
            dialog.exec();
          });
  connect(sourceControlPanel, &SourceControlPanel::commitDiffRequested, this,
          [this](const QString &commitHash, const QString &shortHash) {
            if (!m_gitIntegration) {
              return;
            }
            QString diff = m_gitIntegration->getCommitDiff(commitHash);
            if (diff.trimmed().isEmpty()) {
              QMessageBox::information(
                  this, tr("Commit Diff"),
                  tr("No changes to show for this commit."));
              return;
            }

            GitDiffDialog dialog(m_gitIntegration, commitHash,
                                 GitDiffDialog::DiffTarget::Commit, false,
                                 getTheme(), this);
            dialog.setWindowTitle(tr("Commit %1").arg(shortHash));

            QString author = m_gitIntegration->getCommitAuthor(commitHash);
            QString date = m_gitIntegration->getCommitDate(commitHash);
            QString message = m_gitIntegration->getCommitMessage(commitHash);
            dialog.setCommitInfo(author, date, message);

            dialog.setDiffText(diff);
            dialog.exec();
          });
  connect(sourceControlPanel, &SourceControlPanel::repositoryInitialized, this,
          [this](const QString &path) {
            setProjectRootPath(path);
            updateGitIntegrationForPath(path);
          });

  connect(sourceControlPanel, &SourceControlPanel::compareBranchesRequested,
          this, [this](const QString &branch1, const QString &branch2) {
            if (!m_gitIntegration)
              return;
            QString targetId = QString("%1...%2").arg(branch1).arg(branch2);
            GitDiffDialog diffDialog(m_gitIntegration, targetId,
                                     GitDiffDialog::DiffTarget::Commit, false,
                                     settings.theme, this);
            diffDialog.setWindowTitle(
                tr("Compare: %1 ↔ %2").arg(branch1).arg(branch2));
            diffDialog.exec();
          });

  sourceControlDock = new QDockWidget(tr("Source Control"), this);
  sourceControlDock->setObjectName("sourceControlDock");
  sourceControlDock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                     Qt::RightDockWidgetArea);
  sourceControlDock->setWidget(sourceControlPanel);
  addDockWidget(Qt::RightDockWidgetArea, sourceControlDock);
  sourceControlDock->hide();
  updateSourceControlDockTitle(
      m_gitIntegration ? m_gitIntegration->repositoryPath() : QString(),
      m_gitIntegration ? m_gitIntegration->isValidRepository() : false);

  connect(sourceControlDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (ui->actionToggle_Source_Control) {
              ui->actionToggle_Source_Control->setChecked(visible);
            }
            SettingsManager::instance().setValue("showSourceControlDock",
                                                 visible);
            SettingsManager::instance().saveSettings();
          });
}

void MainWindow::ensureDebugPanel() {
  if (debugDock) {
    return;
  }

  debugPanel = new DebugPanel(this);
  debugPanel->setObjectName("debugPanel");
  debugPanel->applyTheme(settings.theme);
  debugPanel->hide();

  connect(debugPanel, &DebugPanel::locationClicked, this,
          [this](const QString &filePath, int line, int column) {
            QString targetPath = filePath;
            QFileInfo sourceInfo(targetPath);
            if (!targetPath.isEmpty() && sourceInfo.isRelative()) {
              if (!m_projectRootPath.isEmpty()) {
                const QString projectResolved =
                    QDir(m_projectRootPath).absoluteFilePath(targetPath);
                if (QFileInfo::exists(projectResolved)) {
                  targetPath = projectResolved;
                }
              }
              if (QFileInfo(targetPath).isRelative()) {
                const QString cwdResolved =
                    QDir::current().absoluteFilePath(targetPath);
                if (QFileInfo::exists(cwdResolved)) {
                  targetPath = cwdResolved;
                }
              }
            }

            if (!targetPath.isEmpty()) {
              openFileAndAddToNewTab(targetPath);
            }
            updateAllTextAreas(&TextArea::setDebugExecutionLine, 0);
            TextArea *textArea = getCurrentTextArea();
            if (textArea) {
              QTextCursor cursor = textArea->textCursor();
              cursor.movePosition(QTextCursor::Start);
              int targetLine = line > 0 ? line - 1 : 0;
              int targetColumn = column > 0 ? column - 1 : 0;
              cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                  targetLine);
              cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                  targetColumn);
              textArea->setTextCursor(cursor);
              textArea->setDebugExecutionLine(line);
              textArea->centerCursor();
              textArea->setFocus();
            }
          });
  connect(debugPanel, &DebugPanel::startDebugRequested, this,
          &MainWindow::startDebuggingForCurrentFile);
  connect(debugPanel, &DebugPanel::restartDebugRequested, this, [this]() {
    if (!m_activeDebugSessionId.isEmpty()) {
      if (DebugSession *session =
              DebugSessionManager::instance().session(m_activeDebugSessionId)) {
        session->restart();
        return;
      }
    }
    startDebuggingForCurrentFile();
  });
  connect(debugPanel, &DebugPanel::stopDebugRequested, this, [this]() {
    QString sessionIdToStop = m_activeDebugSessionId;
    if (sessionIdToStop.isEmpty()) {
      if (DebugSession *focused =
              DebugSessionManager::instance().focusedSession()) {
        sessionIdToStop = focused->id();
      }
    }

    if (sessionIdToStop.isEmpty()) {
      return;
    }

    clearDebugSession();
    DebugSessionManager::instance().stopSession(sessionIdToStop, true);
  });

  debugDock = new QDockWidget(tr("Debug"), this);
  debugDock->setObjectName("debugDock");
  debugDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea |
                             Qt::RightDockWidgetArea);
  debugDock->setWidget(debugPanel);
  addDockWidget(Qt::BottomDockWidgetArea, debugDock);
  debugDock->hide();

  connect(debugDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            SettingsManager::instance().setValue("showDebugDock", visible);
            SettingsManager::instance().saveSettings();
          });

  connect(&DebugSessionManager::instance(),
          &DebugSessionManager::focusedSessionChanged, this,
          [this](const QString &sessionId) { attachDebugSession(sessionId); });
  connect(&DebugSessionManager::instance(),
          &DebugSessionManager::sessionStarted, this,
          [this](const QString &sessionId) { attachDebugSession(sessionId); });
  connect(&DebugSessionManager::instance(),
          &DebugSessionManager::allSessionsEnded, this,
          [this]() { clearDebugSession(); });
}

void MainWindow::showCommandPalette() {
  if (commandPalette) {
    commandPalette->showPalette();
  }
}

void MainWindow::setupCommandPalette() {
  commandPalette = new CommandPalette(this);

  QMenuBar *menuBar = this->menuBar();
  for (QAction *action : menuBar->actions()) {
    if (action->menu()) {
      commandPalette->registerMenu(action->menu());
    }
  }
}

void MainWindow::showGoToLineDialog() {
  if (!goToLineDialog) {
    setupGoToLineDialog();
  }

  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    int maxLine = textArea->blockCount();
    goToLineDialog->setMaxLine(maxLine);
    goToLineDialog->showDialog();
  }
}

void MainWindow::setupGoToLineDialog() {
  goToLineDialog = new GoToLineDialog(this);

  connect(goToLineDialog, &GoToLineDialog::lineSelected, this,
          [this](int lineNumber) {
            TextArea *textArea = getCurrentTextArea();
            if (textArea) {
              QTextCursor cursor = textArea->textCursor();
              cursor.movePosition(QTextCursor::Start);
              cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                  lineNumber - 1);
              textArea->setTextCursor(cursor);
              textArea->centerCursor();
              textArea->setFocus();
            }
          });
}

void MainWindow::showGoToSymbolDialog() {
  if (!goToSymbolDialog) {
    setupGoToSymbolDialog();
  }

  TextArea *textArea = getCurrentTextArea();
  if (textArea) {

    QList<LspDocumentSymbol> symbols;

    QTextBlock block = textArea->document()->begin();
    while (block.isValid()) {
      QString text = block.text().trimmed();

      if (text.contains('(') && !text.startsWith("//") &&
          !text.startsWith("/*")) {

        int parenPos = text.indexOf('(');
        QString beforeParen = text.left(parenPos).trimmed();

        QStringList parts = beforeParen.split(QRegularExpression("\\s+"));
        if (!parts.isEmpty()) {
          QString name = parts.last();

          while (!name.isEmpty() && (name[0] == '*' || name[0] == '&')) {
            name = name.mid(1);
          }

          if (!name.isEmpty() && name[0].isLetter()) {
            LspDocumentSymbol sym;
            sym.name = name;
            sym.kind = LspSymbolKind::Function;
            sym.selectionRange.start.line = block.blockNumber();
            sym.selectionRange.start.character = 0;
            sym.range = sym.selectionRange;

            if (beforeParen.startsWith("class ") ||
                beforeParen.startsWith("struct ")) {
              sym.kind = LspSymbolKind::Class;
            }

            symbols.append(sym);
          }
        }
      }

      else if (text.startsWith("class ") || text.startsWith("struct ")) {
        QStringList parts = text.split(QRegularExpression("\\s+"));
        if (parts.size() >= 2) {
          QString name = parts[1];

          name = name.split(QRegularExpression("[:{]")).first();

          if (!name.isEmpty()) {
            LspDocumentSymbol sym;
            sym.name = name;
            sym.kind = LspSymbolKind::Class;
            sym.selectionRange.start.line = block.blockNumber();
            sym.selectionRange.start.character = 0;
            sym.range = sym.selectionRange;
            symbols.append(sym);
          }
        }
      }

      else if (text.startsWith("def ") || text.startsWith("class ")) {
        QString keyword = text.startsWith("def ") ? "def " : "class ";
        QString rest = text.mid(keyword.length());
        int endPos = rest.indexOf(QRegularExpression("[:(]"));
        if (endPos > 0) {
          QString name = rest.left(endPos).trimmed();

          LspDocumentSymbol sym;
          sym.name = name;
          sym.kind = text.startsWith("def ") ? LspSymbolKind::Function
                                             : LspSymbolKind::Class;
          sym.selectionRange.start.line = block.blockNumber();
          sym.selectionRange.start.character = 0;
          sym.range = sym.selectionRange;
          symbols.append(sym);
        }
      }

      else if (text.startsWith("function ")) {
        QString rest = text.mid(9);
        int endPos = rest.indexOf('(');
        if (endPos > 0) {
          QString name = rest.left(endPos).trimmed();

          LspDocumentSymbol sym;
          sym.name = name;
          sym.kind = LspSymbolKind::Function;
          sym.selectionRange.start.line = block.blockNumber();
          sym.selectionRange.start.character = 0;
          sym.range = sym.selectionRange;
          symbols.append(sym);
        }
      }

      block = block.next();
    }

    goToSymbolDialog->setSymbols(symbols);
    goToSymbolDialog->showDialog();
  }
}

void MainWindow::setupGoToSymbolDialog() {
  goToSymbolDialog = new GoToSymbolDialog(this);

  connect(goToSymbolDialog, &GoToSymbolDialog::symbolSelected, this,
          [this](int line, int column) {
            TextArea *textArea = getCurrentTextArea();
            if (textArea) {
              QTextCursor cursor = textArea->textCursor();
              cursor.movePosition(QTextCursor::Start);
              cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                                  line);
              cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                  column);
              textArea->setTextCursor(cursor);
              textArea->centerCursor();
              textArea->setFocus();
            }
          });
}

void MainWindow::showFileQuickOpen() {
  if (!fileQuickOpen) {
    setupFileQuickOpen();
  }

  QString rootPath = QDir::currentPath();

  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  auto filePath = tabWidget->getFilePath(tabIndex);
  if (!filePath.isEmpty()) {
    QFileInfo fileInfo(filePath);
    rootPath = fileInfo.absolutePath();

    QDir dir(rootPath);
    while (dir.exists()) {
      if (dir.exists(".git") || dir.exists("CMakeLists.txt") ||
          dir.exists("package.json") || dir.exists("Makefile")) {
        rootPath = dir.absolutePath();
        break;
      }
      if (!dir.cdUp()) {
        break;
      }
    }
  }

  fileQuickOpen->setRootDirectory(rootPath);
  fileQuickOpen->showDialog();
}

void MainWindow::setupFileQuickOpen() {
  fileQuickOpen = new FileQuickOpen(this);

  connect(
      fileQuickOpen, &FileQuickOpen::fileSelected, this,
      [this](const QString &filePath) { openFileAndAddToNewTab(filePath); });
}

void MainWindow::showRecentFilesDialog() {
  if (!recentFilesDialog) {
    setupRecentFilesDialog();
  }

  recentFilesDialog->showDialog();
}

void MainWindow::setupRecentFilesDialog() {
  recentFilesDialog = new RecentFilesDialog(recentFilesManager, this);

  connect(
      recentFilesDialog, &RecentFilesDialog::fileSelected, this,
      [this](const QString &filePath) { openFileAndAddToNewTab(filePath); });
}

void MainWindow::setupBreadcrumb() {
  breadcrumbWidget = new BreadcrumbWidget(this);

  auto layout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
  if (layout) {

    layout->insertWidget(0, breadcrumbWidget);
  }

  connect(breadcrumbWidget, &BreadcrumbWidget::pathSegmentClicked, this,
          [this](const QString &path) {
            QFileInfo fileInfo(path);
            if (fileInfo.isDir()) {

            } else if (fileInfo.isFile()) {
              openFileAndAddToNewTab(path);
            }
          });
}

void MainWindow::updateBreadcrumb(const QString &filePath) {
  if (breadcrumbWidget) {
    breadcrumbWidget->setFilePath(filePath);
    if (!m_projectRootPath.isEmpty()) {
      breadcrumbWidget->setProjectRoot(m_projectRootPath);
    }
  }
}

void MainWindow::toggleShowWhitespace() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setShowWhitespace(!textArea->showWhitespace());
  }
}

void MainWindow::toggleShowIndentGuides() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setShowIndentGuides(!textArea->showIndentGuides());
  }
}

void MainWindow::navigateBack() {
  if (!navigationHistory || !navigationHistory->canGoBack()) {
    return;
  }

  NavigationLocation loc = navigationHistory->goBack();
  if (loc.isValid()) {

    openFileAndAddToNewTab(loc.filePath);

    TextArea *textArea = getCurrentTextArea();
    if (textArea) {
      QTextCursor cursor = textArea->textCursor();
      cursor.movePosition(QTextCursor::Start);
      cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                          loc.line - 1);
      cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                          loc.column);
      textArea->setTextCursor(cursor);
      textArea->centerCursor();
    }
  }
}

void MainWindow::navigateForward() {
  if (!navigationHistory || !navigationHistory->canGoForward()) {
    return;
  }

  NavigationLocation loc = navigationHistory->goForward();
  if (loc.isValid()) {

    openFileAndAddToNewTab(loc.filePath);

    TextArea *textArea = getCurrentTextArea();
    if (textArea) {
      QTextCursor cursor = textArea->textCursor();
      cursor.movePosition(QTextCursor::Start);
      cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                          loc.line - 1);
      cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                          loc.column);
      textArea->setTextCursor(cursor);
      textArea->centerCursor();
    }
  }
}

void MainWindow::recordNavigationLocation() {
  if (!navigationHistory) {
    return;
  }

  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  QString filePath = tabWidget->getFilePath(tabIndex);
  if (filePath.isEmpty()) {
    return;
  }

  QTextCursor cursor = textArea->textCursor();
  NavigationLocation loc;
  loc.filePath = filePath;
  loc.line = cursor.blockNumber() + 1;
  loc.column = cursor.positionInBlock();

  navigationHistory->recordLocationIfSignificant(loc);
}

void MainWindow::setupNavigationHistory() {
  navigationHistory = new NavigationHistory(this);
}

void MainWindow::setupSymbolNavigation() {
  m_symbolNavService = new SymbolNavigationService(this);

  const auto configs = LanguageLspDefinitionProvider::defaultConfigs();
  for (const LanguageServerConfig &config : configs) {
    auto *provider = new LanguageLspDefinitionProvider(config, this);
    m_symbolNavService->registerProvider(provider);
  }

  connect(m_symbolNavService, &SymbolNavigationService::definitionFound, this,
          &MainWindow::handleDefinitionResults);

  connect(m_symbolNavService, &SymbolNavigationService::noDefinitionFound, this,
          [this](const QString &message) {
            statusBar()->showMessage(message, 5000);
          });

  connect(m_symbolNavService,
          &SymbolNavigationService::definitionRequestStarted, this, [this]() {
            statusBar()->showMessage(
                QCoreApplication::translate("MainWindow",
                                            "Searching for definition..."));
          });

  connect(m_symbolNavService,
          &SymbolNavigationService::definitionRequestFinished, this, [this]() {
            QString expected = QCoreApplication::translate(
                "MainWindow", "Searching for definition...");
            if (statusBar()->currentMessage() == expected) {
              statusBar()->clearMessage();
            }
          });
}

void MainWindow::goToDefinitionAtCursor() {
  if (!m_symbolNavService) {
    return;
  }

  if (m_symbolNavService->isRequestInFlight()) {
    statusBar()->showMessage(tr("Definition lookup already in progress..."),
                             3000);
    return;
  }

  TextArea *textArea = getCurrentTextArea();
  if (!textArea) {
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return;
  }

  int tabIndex = tabWidget->currentIndex();
  QString filePath = tabWidget->getFilePath(tabIndex);
  if (filePath.isEmpty()) {
    statusBar()->showMessage(tr("No file open"), 3000);
    return;
  }

  QString languageId = effectiveLanguageIdForFile(filePath);

  QTextCursor cursor = textArea->textCursor();

  recordNavigationLocation();

  DefinitionRequest req;
  req.filePath = filePath;
  req.line = cursor.blockNumber() + 1;
  req.column = cursor.positionInBlock();
  req.languageId = languageId;

  m_symbolNavService->goToDefinition(req);
}

void MainWindow::handleDefinitionResults(
    const QList<DefinitionTarget> &targets) {
  if (targets.size() == 1) {
    jumpToTarget(targets.first());
  } else if (targets.size() > 1) {
    QStringList items;
    for (const DefinitionTarget &t : targets) {
      QString label = t.label.isEmpty()
                          ? QStringLiteral("%1:%2")
                                .arg(QFileInfo(t.filePath).fileName())
                                .arg(t.line)
                          : t.label;
      items << label;
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(
        this, tr("Go to Definition"), tr("Multiple definitions found:"), items,
        0, false, &ok);
    if (ok && !selected.isEmpty()) {
      int idx = items.indexOf(selected);
      if (idx >= 0 && idx < targets.size()) {
        jumpToTarget(targets.at(idx));
      }
    }
  }
}

void MainWindow::jumpToTarget(const DefinitionTarget &target) {
  if (!target.isValid()) {
    return;
  }

  openFileAndAddToNewTab(target.filePath);

  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    QTextCursor cursor = textArea->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                        target.line - 1);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                        target.column);
    textArea->setTextCursor(cursor);
    textArea->centerCursor();
  }

  if (navigationHistory) {
    NavigationLocation loc;
    loc.filePath = target.filePath;
    loc.line = target.line;
    loc.column = target.column;
    navigationHistory->recordLocation(loc);
  }
}

void MainWindow::setupAutoSave() {
  autoSaveManager = new AutoSaveManager(this, this);
}

void MainWindow::setupGitIntegration() {
  if (m_gitIntegration) {
    return;
  }

  m_gitIntegration = new GitIntegration(this);
  m_inlineBlameEnabled = true;

  m_gitBranchLabel = new QLabel(this);
  m_gitBranchLabel->setToolTip(tr("Current branch (click to switch)"));
  m_gitBranchLabel->setCursor(Qt::PointingHandCursor);
  m_gitBranchLabel->setStyleSheet("QLabel { padding: 0 6px; }");

  m_gitSyncLabel = new QLabel(this);
  m_gitSyncLabel->setToolTip(tr("Ahead/behind upstream"));
  m_gitSyncLabel->setStyleSheet("QLabel { padding: 0 4px; }");

  m_gitDirtyLabel = new QLabel(this);
  m_gitDirtyLabel->setToolTip(tr("Working tree status"));
  m_gitDirtyLabel->setStyleSheet("QLabel { padding: 0 4px; }");

  statusBar()->addPermanentWidget(m_gitDirtyLabel);
  statusBar()->addPermanentWidget(m_gitSyncLabel);
  statusBar()->addPermanentWidget(m_gitBranchLabel);

  m_gitStatusBarTimer.setSingleShot(true);
  m_gitStatusBarTimer.setInterval(500);
  connect(&m_gitStatusBarTimer, &QTimer::timeout, this,
          &MainWindow::updateGitStatusBar);

  connect(m_gitIntegration, &GitIntegration::statusChanged, this,
          [this]() { m_gitStatusBarTimer.start(); });
  connect(m_gitIntegration, &GitIntegration::branchChanged, this,
          [this](const QString &) { m_gitStatusBarTimer.start(); });

  updateGitIntegrationForPath(QDir::currentPath());
}

void MainWindow::updateGitIntegrationForPath(const QString &path) {
  if (!m_gitIntegration || path.isEmpty()) {
    return;
  }

  bool isRepo = m_gitIntegration->setRepositoryPath(path);
  if (!isRepo) {
    m_gitIntegration->setWorkingPath(path);
  } else {
    m_gitIntegration->setWorkingPath(m_gitIntegration->repositoryPath());
  }

  applyGitIntegrationToAllPages();
  m_gitIntegration->refresh();

  if (sourceControlPanel) {
    sourceControlPanel->setWorkingPath(
        isRepo ? m_gitIntegration->repositoryPath() : path);
    sourceControlPanel->refresh();
  }
  updateSourceControlDockTitle(m_gitIntegration->repositoryPath(), isRepo);

  auto textArea = getCurrentTextArea();
  LightpadTabWidget *tabWidget = currentTabWidget();
  QString currentFilePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (textArea && !currentFilePath.isEmpty()) {
    textArea->setGutterGitIntegration(m_gitIntegration);
    QList<GitDiffLineInfo> diffLines =
        m_gitIntegration->getDiffLines(currentFilePath);
    QList<QPair<int, int>> gutterLines;
    gutterLines.reserve(diffLines.size());
    for (const auto &info : diffLines) {
      int type = 1;
      if (info.type == GitDiffLineInfo::Type::Added) {
        type = 0;
      } else if (info.type == GitDiffLineInfo::Type::Deleted) {
        type = 2;
      }
      gutterLines.append(qMakePair(info.lineNumber, type));
    }
    textArea->setGitDiffLines(gutterLines);
  }

  showGitBlameForCurrentFile(isGitBlameEnabledForFile(currentFilePath));
  updateInlineBlameForCurrentFile();
  updateGitStatusBar();
}

void MainWindow::updateSourceControlDockTitle(const QString &repoRoot,
                                              bool isRepo) {
  if (!sourceControlDock) {
    return;
  }

  if (isRepo && !repoRoot.isEmpty()) {
    sourceControlDock->setWindowTitle(
        tr("Source Control — %1").arg(QDir(repoRoot).absolutePath()));
    return;
  }

  sourceControlDock->setWindowTitle(tr("Source Control"));
}

void MainWindow::applyGitIntegrationToAllPages() {
  if (!m_gitIntegration) {
    return;
  }

  if (m_fileTreeModel) {
    m_fileTreeModel->setGitIntegration(m_gitIntegration);
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (page) {
        page->setGitIntegration(m_gitIntegration);
      }
    }
  }
}

void MainWindow::updateProblemsStatusLabel(int errors, int warnings,
                                           int infos) {
  if (problemsStatusLabel) {
    QString text;
    if (errors > 0 || warnings > 0) {
      text = QString("⛔ %1  ⚠️ %2").arg(errors).arg(warnings);
    } else {
      text = "✓ No problems";
    }
    problemsStatusLabel->setText(text);
  }
}

void MainWindow::updateVimStatusLabel(const QString &text) {
  if (vimStatusLabel) {
    vimStatusLabel->setText(text);
    vimStatusLabel->setVisible(!text.isEmpty());
    if (!text.isEmpty()) {
      QString bgColor;
      if (text == "NORMAL")
        bgColor = "#3fb950";
      else if (text == "INSERT")
        bgColor = "#58a6ff";
      else if (text == "VISUAL" || text == "V-LINE" || text == "V-BLOCK")
        bgColor = "#d29922";
      else if (text == "REPLACE")
        bgColor = "#f85149";
      else if (text == "COMMAND")
        bgColor = "#bc8cff";
      else
        bgColor = "#8b949e";
      vimStatusLabel->setStyleSheet(
          QString("QLabel {"
                  "  color: #ffffff;"
                  "  background-color: %1;"
                  "  padding: 1px 10px;"
                  "  border-radius: 3px;"
                  "  font-weight: bold;"
                  "  font-size: 11px;"
                  "  letter-spacing: 1px;"
                  "}")
              .arg(bgColor));
    }
  }
}

void MainWindow::showVimStatusMessage(const QString &message) {
  if (!vimStatusLabel) {
    return;
  }
  updateVimStatusLabel(message);
  QTimer::singleShot(2500, this, [this, message]() {
    if (vimStatusLabel && vimStatusLabel->text() == message) {
      updateVimStatusLabel("");
    }
  });
}

void MainWindow::setMainWindowTitle(QString title) {
  setWindowTitle(title + " - Lightpad");
}

void MainWindow::setFont(QFont newFont) {
  updateAllTextAreas(&TextArea::setFont, newFont);
  font = newFont;
}

void MainWindow::showLineNumbers(bool flag) {
  updateAllTextAreas(&TextArea::showLineNumbers, flag);
  settings.showLineNumberArea = flag;
}

void MainWindow::highlihtCurrentLine(bool flag) {
  updateAllTextAreas(&TextArea::highlihtCurrentLine, flag);
  settings.lineHighlighted = flag;
}

void MainWindow::highlihtMatchingBracket(bool flag) {
  updateAllTextAreas(&TextArea::highlihtMatchingBracket, flag);
  settings.matchingBracketsHighlighted = flag;
}

void MainWindow::runCurrentScript() {
  on_actionSave_triggered();
  auto textArea = getCurrentTextArea();

  if (textArea && !textArea->changesUnsaved())
    showTerminal();
}

bool MainWindow::prepareDebugTargetForFile(const QString &filePath,
                                           const QString &languageId,
                                           QString *errorMessage) const {
  const QString canonicalLanguageId = LanguageCatalog::normalize(languageId);
  if (canonicalLanguageId != "cpp" && canonicalLanguageId != "c") {
    return true;
  }

  QFileInfo sourceInfo(filePath);
  if (!sourceInfo.exists()) {
    if (errorMessage) {
      *errorMessage = tr("Source file does not exist: %1").arg(filePath);
    }
    return false;
  }

  QString outputPath =
      sourceInfo.absolutePath() + "/" + sourceInfo.completeBaseName();
#ifdef Q_OS_WIN
  outputPath += ".exe";
#endif

  QFileInfo outputInfo(outputPath);
  const bool needsBuild = !outputInfo.exists() ||
                          outputInfo.lastModified() < sourceInfo.lastModified();
  if (!needsBuild) {
    return true;
  }

  return compileSourceForDebug(filePath, canonicalLanguageId, outputPath,
                               errorMessage);
}

bool MainWindow::compileSourceForDebug(const QString &filePath,
                                       const QString &languageId,
                                       const QString &outputPath,
                                       QString *errorMessage) const {
  const QString compiler = (languageId == "c") ? "gcc" : "g++";

  QStringList args;
  if (languageId == "c") {
    args << "-g" << "-O0" << "-std=c11";
  } else {
    args << "-g" << "-O0" << "-std=c++17";
  }

  FileTemplateAssignment assignment =
      RunTemplateManager::instance().getAssignmentForFile(filePath);

  for (const QString &flag : assignment.compilerFlags) {
    args << RunTemplateManager::substituteVariables(flag, filePath);
  }

  args << "-o" << outputPath << filePath;

  for (const QString &src : assignment.sourceFiles) {
    args << RunTemplateManager::substituteVariables(src, filePath);
  }

  QProcess process;
  process.setProgram(compiler);
  process.setArguments(args);
  process.setWorkingDirectory(QFileInfo(filePath).absolutePath());
  process.start();

  if (!process.waitForStarted(5000)) {
    if (errorMessage) {
      *errorMessage = tr("Failed to start compiler '%1': %2")
                          .arg(compiler, process.errorString());
    }
    return false;
  }

  if (!process.waitForFinished(120000)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = tr("Compilation timed out for %1").arg(filePath);
    }
    return false;
  }

  const QByteArray output =
      process.readAllStandardOutput() + process.readAllStandardError();
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      const QString commandLine = compiler + " " + args.join(" ");
      QString details = QString::fromUtf8(output).trimmed();
      if (details.isEmpty()) {
        details = tr("Compiler exited with code %1").arg(process.exitCode());
      }
      *errorMessage =
          tr("Debug build command failed:\n%1\n\n%2").arg(commandLine, details);
    }
    return false;
  }

  return true;
}

void MainWindow::startDebuggingForCurrentFile() {
  if (m_debugStartInProgress) {
    return;
  }

  if (!m_activeDebugSessionId.isEmpty()) {
    if (DebugSession *activeSession =
            DebugSessionManager::instance().session(m_activeDebugSessionId)) {
      if (activeSession->state() != DebugSession::State::Idle &&
          activeSession->state() != DebugSession::State::Terminated) {
        attachDebugSession(m_activeDebugSessionId);
        if (debugDock) {
          debugDock->show();
        }
        return;
      }
    }
  }

  on_actionSave_triggered();
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return;
  }
  auto page = tabWidget->getCurrentPage();
  QString filePath = page ? page->getFilePath() : QString();

  if (filePath.isEmpty()) {
    noScriptAssignedWarning();
    return;
  }

  ensureProjectRootForPath(filePath);

  m_debugStartInProgress = true;

  DebugSettings::instance().initialize(m_projectRootPath);
  DebugConfigurationManager::instance().setWorkspaceFolder(m_projectRootPath);
  DebugConfigurationManager::instance().loadFromLightpadDir();
  BreakpointManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (BreakpointManager::instance().allBreakpoints().isEmpty()) {
    BreakpointManager::instance().loadFromLightpadDir();
  }
  WatchManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (WatchManager::instance().allWatches().isEmpty()) {
    WatchManager::instance().loadFromLightpadDir();
  }

  const QString languageId = effectiveLanguageIdForFile(filePath);
  QString prepareError;
  if (!prepareDebugTargetForFile(filePath, languageId, &prepareError)) {
    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Warning);
    msg.setWindowTitle(tr("Debug Build Failed"));
    msg.setText(tr("Unable to prepare a debuggable target for this file."));
    msg.setDetailedText(prepareError);
    msg.exec();
    m_debugStartInProgress = false;
    return;
  }

  QString sessionId =
      DebugSessionManager::instance().quickStart(filePath, languageId);
  if (sessionId.isEmpty()) {
    QString details;
    DebugConfiguration quickConfig =
        DebugConfigurationManager::instance().createQuickConfig(filePath,
                                                                languageId);
    if (!quickConfig.type.isEmpty()) {
      const auto adapters =
          DebugAdapterRegistry::instance().adaptersForType(quickConfig.type);
      QStringList adapterStatuses;
      for (const auto &adapter : adapters) {
        if (!adapter) {
          continue;
        }
        adapterStatuses << QString("%1: %2").arg(adapter->config().name,
                                                 adapter->statusMessage());
      }
      details = adapterStatuses.join("\n");
    }

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Warning);
    msg.setWindowTitle(tr("Debug"));
    msg.setText(tr("Unable to start debug session for this file."));
    if (!details.isEmpty()) {
      msg.setDetailedText(details);
    }
    msg.exec();
    m_debugStartInProgress = false;
    return;
  }

  attachDebugSession(sessionId);
  if (debugDock) {
    debugDock->show();
  }
  m_debugStartInProgress = false;
}

void MainWindow::attachDebugSession(const QString &sessionId) {
  if (sessionId.isEmpty()) {
    return;
  }

  DebugSession *session = DebugSessionManager::instance().session(sessionId);
  if (!session || !session->client()) {
    return;
  }

  if (sessionId == m_activeDebugSessionId && debugPanel &&
      debugPanel->dapClient() == session->client()) {
    if (debugDock) {
      debugDock->show();
    }
    return;
  }

  m_activeDebugSessionId = sessionId;
  DebugSessionManager::instance().setFocusedSession(sessionId);
  debugPanel->setDapClient(session->client());
  WatchManager::instance().setDapClient(session->client());
  if (debugDock) {
    debugDock->show();
  }
  if (m_breakpointsSetConnection) {
    disconnect(m_breakpointsSetConnection);
  }
  if (m_breakpointChangedConnection) {
    disconnect(m_breakpointChangedConnection);
  }
  if (m_sessionTerminatedConnection) {
    disconnect(m_sessionTerminatedConnection);
  }
  if (m_sessionErrorConnection) {
    disconnect(m_sessionErrorConnection);
  }
  if (m_sessionStateConnection) {
    disconnect(m_sessionStateConnection);
  }
  m_breakpointsSetConnection = connect(
      session->client(), &DapClient::breakpointsSet, this,
      [](const QString &sourcePath, const QList<DapBreakpoint> &breakpoints) {
        if (!sourcePath.isEmpty()) {
          BreakpointManager::instance().updateVerification(sourcePath,
                                                           breakpoints);
        }
      });
  m_breakpointChangedConnection =
      connect(session->client(), &DapClient::breakpointChanged, this,
              [](const DapBreakpoint &breakpoint, const QString &) {
                if (!breakpoint.source.path.isEmpty()) {
                  BreakpointManager::instance().updateVerification(
                      breakpoint.source.path, {breakpoint});
                }
              });

  m_sessionTerminatedConnection =
      connect(session, &DebugSession::terminated, this, [this, sessionId]() {
        if (sessionId == m_activeDebugSessionId) {
          clearDebugSession();
        }
      });
  m_sessionErrorConnection = connect(
      session, &DebugSession::error, this, [this](const QString &message) {
        QMessageBox::warning(this, tr("Debug Session Error"), message);
      });
  m_sessionStateConnection =
      connect(session, &DebugSession::stateChanged, this,
              [this](DebugSession::State state) {
                if (state == DebugSession::State::Running ||
                    state == DebugSession::State::Terminated ||
                    state == DebugSession::State::Idle) {
                  updateAllTextAreas(&TextArea::setDebugExecutionLine, 0);
                }
              });
}

void MainWindow::clearDebugSession() {
  m_activeDebugSessionId.clear();
  if (debugPanel) {
    debugPanel->setDapClient(nullptr);
    debugPanel->clearAll();
  }
  WatchManager::instance().setDapClient(nullptr);
  if (m_breakpointsSetConnection) {
    disconnect(m_breakpointsSetConnection);
    m_breakpointsSetConnection = {};
  }
  if (m_breakpointChangedConnection) {
    disconnect(m_breakpointChangedConnection);
    m_breakpointChangedConnection = {};
  }
  if (m_sessionTerminatedConnection) {
    disconnect(m_sessionTerminatedConnection);
    m_sessionTerminatedConnection = {};
  }
  if (m_sessionErrorConnection) {
    disconnect(m_sessionErrorConnection);
    m_sessionErrorConnection = {};
  }
  if (m_sessionStateConnection) {
    disconnect(m_sessionStateConnection);
    m_sessionStateConnection = {};
  }
  updateAllTextAreas(&TextArea::setDebugExecutionLine, 0);
}

void MainWindow::formatCurrentDocument() {
  auto page = currentTabWidget()->getCurrentPage();
  QString filePath = page ? page->getFilePath() : QString();

  if (filePath.isEmpty()) {
    QMessageBox::information(this, "Format Document",
                             "Please save the file first before formatting.");
    return;
  }

  auto textArea = getCurrentTextArea();
  on_actionSave_triggered();

  if (textArea && textArea->changesUnsaved()) {
    QMessageBox::warning(this, "Format Document",
                         "Could not save the file. Formatting cancelled.");
    return;
  }

  FormatTemplateManager &manager = FormatTemplateManager::instance();

  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  QPair<QString, QStringList> command = manager.buildCommand(filePath);

  if (command.first.isEmpty()) {
    QMessageBox::information(
        this, "Format Document",
        "No formatter found for this file type.\nUse Format > Edit Format "
        "Configurations to assign one.");
    return;
  }

  ensureProjectRootForPath(filePath);
  showTerminalPanel();

  if (m_formatProcessFinishedConnection) {
    disconnect(m_formatProcessFinishedConnection);
    m_formatProcessFinishedConnection = {};
  }
  if (m_formatProcessErrorConnection) {
    disconnect(m_formatProcessErrorConnection);
    m_formatProcessErrorConnection = {};
  }

  FileFormatAssignment assignment = manager.getAssignmentForFile(filePath);

  QString workingDirectory = assignment.workingDirectory.trimmed();
  if (workingDirectory.isEmpty()) {
    workingDirectory = QFileInfo(filePath).absoluteDir().path();
  } else {
    workingDirectory =
        FormatTemplateManager::substituteVariables(workingDirectory, filePath);
  }

  QMap<QString, QString> customEnv;
  for (auto it = assignment.customEnv.begin(); it != assignment.customEnv.end();
       ++it) {
    const QString key = it.key().trimmed();
    if (key.isEmpty()) {
      continue;
    }
    customEnv[key] =
        FormatTemplateManager::substituteVariables(it.value(), filePath);
  }

  QString preFormatCommand = assignment.preFormatCommand.trimmed();
  if (!preFormatCommand.isEmpty()) {
    preFormatCommand =
        FormatTemplateManager::substituteVariables(preFormatCommand, filePath);
  }
  QString postFormatCommand = assignment.postFormatCommand.trimmed();
  if (!postFormatCommand.isEmpty()) {
    postFormatCommand =
        FormatTemplateManager::substituteVariables(postFormatCommand, filePath);
  }

  auto shellProgramAndArgs =
      [](const QString &commandText) -> QPair<QString, QStringList> {
#ifdef Q_OS_WIN
    return qMakePair(QString("cmd"), QStringList() << "/C" << commandText);
#else
    return qMakePair(QString("bash"), QStringList() << "-lc" << commandText);
#endif
  };

  struct FormatExecutionState {
    enum class Stage {
      PreFormat,
      Formatter,
      PostFormat,
    };

    Stage stage = Stage::Formatter;
    bool formatterRan = false;
    int formatterExitCode = 0;
    QString activeStageName;
  };

  QSharedPointer<FormatExecutionState> executionState =
      QSharedPointer<FormatExecutionState>::create();
  if (!preFormatCommand.isEmpty()) {
    executionState->stage = FormatExecutionState::Stage::PreFormat;
  }

  QPointer<TextArea> targetTextArea = textArea;
  QSharedPointer<std::function<void()>> finalizeExecution =
      QSharedPointer<std::function<void()>>::create();
  *finalizeExecution = [this, filePath, targetTextArea, executionState]() {
    if (m_formatProcessFinishedConnection) {
      disconnect(m_formatProcessFinishedConnection);
      m_formatProcessFinishedConnection = {};
    }
    if (m_formatProcessErrorConnection) {
      disconnect(m_formatProcessErrorConnection);
      m_formatProcessErrorConnection = {};
    }

    if (executionState->formatterRan &&
        executionState->formatterExitCode != 0) {
      LOG_WARNING(QString("Formatter exited with code %1")
                      .arg(executionState->formatterExitCode));
      QMessageBox::warning(
          this, "Format Document",
          QString("Formatter exited with error code %1.\nCheck the "
                  "Terminal panel for details.")
              .arg(executionState->formatterExitCode));
    }

    if (!targetTextArea || !executionState->formatterRan) {
      return;
    }

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QString newContent = QString::fromUtf8(file.readAll());
      file.close();

      if (targetTextArea->toPlainText() != newContent) {
        int cursorPos = targetTextArea->textCursor().position();

        targetTextArea->setPlainText(newContent);
        targetTextArea->document()->setModified(false);

        QTextCursor cursor = targetTextArea->textCursor();
        cursor.setPosition(
            qMin(cursorPos, targetTextArea->toPlainText().length()));
        targetTextArea->setTextCursor(cursor);
      }
    } else {
      LOG_WARNING(
          QString("Failed to reload file after formatting: %1").arg(filePath));
    }
  };

  QSharedPointer<std::function<void()>> runCurrentStage =
      QSharedPointer<std::function<void()>>::create();
  *runCurrentStage = [this, runCurrentStage, executionState, preFormatCommand,
                      command, postFormatCommand, workingDirectory, customEnv,
                      shellProgramAndArgs]() {
    switch (executionState->stage) {
    case FormatExecutionState::Stage::PreFormat: {
      executionState->activeStageName = "pre-format command";
      QPair<QString, QStringList> shellCommand =
          shellProgramAndArgs(preFormatCommand);
      terminalWidget->executeCommand(shellCommand.first, shellCommand.second,
                                     workingDirectory, customEnv);
      return;
    }
    case FormatExecutionState::Stage::Formatter:
      executionState->activeStageName = "formatter";
      terminalWidget->executeCommand(command.first, command.second,
                                     workingDirectory, customEnv);
      return;
    case FormatExecutionState::Stage::PostFormat: {
      executionState->activeStageName = "post-format command";
      QPair<QString, QStringList> shellCommand =
          shellProgramAndArgs(postFormatCommand);
      terminalWidget->executeCommand(shellCommand.first, shellCommand.second,
                                     workingDirectory, customEnv);
      return;
    }
    }
  };

  m_formatProcessFinishedConnection = connect(
      terminalWidget, &TerminalTabWidget::processFinished, this,
      [this, executionState, postFormatCommand, runCurrentStage,
       finalizeExecution](int exitCode) {
        if (executionState->stage == FormatExecutionState::Stage::PreFormat) {
          if (exitCode != 0) {
            QMessageBox::warning(
                this, "Format Document",
                QString("Pre-format command failed with exit code %1.\nCheck "
                        "the Terminal panel for details.")
                    .arg(exitCode));
            (*finalizeExecution)();
            return;
          }

          executionState->stage = FormatExecutionState::Stage::Formatter;
          (*runCurrentStage)();
          return;
        }

        if (executionState->stage == FormatExecutionState::Stage::Formatter) {
          executionState->formatterRan = true;
          executionState->formatterExitCode = exitCode;

          if (!postFormatCommand.isEmpty()) {
            executionState->stage = FormatExecutionState::Stage::PostFormat;
            (*runCurrentStage)();
            return;
          }

          (*finalizeExecution)();
          return;
        }

        if (executionState->stage == FormatExecutionState::Stage::PostFormat) {
          if (exitCode != 0) {
            QMessageBox::warning(
                this, "Format Document",
                QString("Post-format command failed with exit code %1.\nCheck "
                        "the Terminal panel for details.")
                    .arg(exitCode));
          }
          (*finalizeExecution)();
          return;
        }
      });

  m_formatProcessErrorConnection = connect(
      terminalWidget, &TerminalTabWidget::errorOccurred, this,
      [this, executionState, finalizeExecution](const QString &errorMessage) {
        QMessageBox::warning(
            this, "Format Document",
            QString("Failed to start %1.\n\n%2")
                .arg(executionState->activeStageName, errorMessage));
        (*finalizeExecution)();
      });

  (*runCurrentStage)();
}

void MainWindow::setFilePathAsTabText(QString filePath) {

  auto fileName = QFileInfo(filePath).fileName();

  LightpadTabWidget *tabWidget = currentTabWidget();
  auto tabIndex = tabWidget->currentIndex();
  auto tabText = tabWidget->tabText(tabIndex);

  setMainWindowTitle(fileName);
  tabWidget->setTabText(tabIndex, fileName);
}

void MainWindow::closeCurrentTab() {
  auto textArea = getCurrentTextArea();

  if (textArea && textArea->changesUnsaved())
    on_actionSave_triggered();

  currentTabWidget()->closeCurrentTab();
}

void MainWindow::setupTabWidgetConnections(LightpadTabWidget *tabWidget) {
  QObject::connect(tabWidget, &QTabWidget::currentChanged, this,
                   [this, tabWidget](int index) {
                     updateTabWidgetContext(tabWidget, index);
                   });
}

void MainWindow::updateTabWidgetContext(LightpadTabWidget *tabWidget,
                                        int index) {
  if (!tabWidget) {
    return;
  }

  auto text = tabWidget->tabText(index);
  setMainWindowTitle(text);

  if (!ui->menuRun->actions().empty()) {
    ui->menuRun->actions().front()->setText("Run " + text);
  }

  QString filePath = tabWidget->getFilePath(index);
  applyHighlightForFile(filePath);

  setupTextArea();

  if (findReplacePanel && tabWidget == currentTabWidget()) {
    findReplacePanel->setTextArea(getCurrentTextArea());
  }

  auto *page = tabWidget->getPage(index);
  if (page) {
    auto *view = qobject_cast<LightpadTreeView *>(page->getTreeView());
    if (view) {
      registerTreeView(view);
    }
  }
}

void MainWindow::applyTabWidgetTheme(LightpadTabWidget *tabWidget) {
  if (!tabWidget) {
    return;
  }
  tabWidget->setTheme(
      settings.theme.backgroundColor.name(),
      settings.theme.foregroundColor.name(), settings.theme.surfaceColor.name(),
      settings.theme.hoverColor.name(), settings.theme.accentColor.name(),
      settings.theme.borderColor.name());
}

void MainWindow::setupTabWidget() {
  applyTabWidgetTheme(ui->tabWidget);
  setupTabWidgetConnections(ui->tabWidget);
  updateTabWidgetContext(ui->tabWidget, 0);
}

void MainWindow::setupCompletionSystem() {

  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(std::make_shared<KeywordCompletionProvider>());

  registry.registerProvider(std::make_shared<SnippetCompletionProvider>());

  registry.registerProvider(std::make_shared<PluginCompletionProvider>());

  m_completionEngine = new CompletionEngine(this);

  QStringList providerIds = registry.allProviderIds();
  if (providerIds.isEmpty()) {
    LOG_WARNING("Completion system initialized but no providers registered");
  } else {
    LOG_INFO("Completion system initialized with providers: " +
             providerIds.join(", "));
  }
}

void MainWindow::setupTextArea() {

  if (getCurrentTextArea()) {
    auto *textArea = getCurrentTextArea();
    textArea->setMainWindow(this);
    textArea->setFont(settings.mainFont);
    textArea->setTabWidth(settings.tabWidth);
    textArea->setVimModeEnabled(settings.vimModeEnabled);
    ensureStatusLabels();
    connectVimMode(textArea);

    if (m_completionEngine) {
      textArea->setCompletionEngine(m_completionEngine);
      LightpadTabWidget *tabWidget = currentTabWidget();
      QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
      if (filePath.isEmpty()) {
        textArea->setLanguage("plaintext");
        textArea->updateSyntaxHighlightTags("", "plaintext");
        QString displayName = LanguageCatalog::displayName("plaintext");
        setLanguageHighlightLabel(displayName.isEmpty() ? "Normal Text"
                                                        : displayName);
      } else {
        applyHighlightForFile(filePath);
      }
    }

    if (completer && !m_completionEngine)
      textArea->setCompleter(completer);
  }
}

void MainWindow::noScriptAssignedWarning() {
  QMessageBox msgBox(this);
  msgBox.setText("No file is currently open.");
  msgBox.setInformativeText(
      "Open a file first, then you can run it or configure a run template.");
  auto openButton = msgBox.addButton(tr("Open File"), QMessageBox::ActionRole);
  msgBox.addButton(QMessageBox::Cancel);
  msgBox.exec();

  if (msgBox.clickedButton() == openButton)
    on_actionOpen_File_triggered();
}

void MainWindow::on_languageHighlight_clicked() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget) {
    return;
  }
  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty()) {
    return;
  }

  QMenu menu(this);
  QActionGroup actionGroup(&menu);
  actionGroup.setExclusive(true);

  QAction *autoDetectAction = menu.addAction("Auto Detect");
  autoDetectAction->setCheckable(true);
  autoDetectAction->setChecked(highlightOverrideForFile(filePath).isEmpty());
  actionGroup.addAction(autoDetectAction);

  menu.addSeparator();

  QVector<LanguageInfo> languages = LanguageCatalog::allLanguages();
  if (languages.isEmpty()) {
    return;
  }
  for (const LanguageInfo &language : languages) {
    QAction *action = menu.addAction(language.displayName);
    action->setCheckable(true);
    action->setData(language.id);
    action->setChecked(effectiveLanguageIdForFile(filePath) == language.id);
    actionGroup.addAction(action);
  }

  QAction *selectedAction = menu.exec(ui->languageHighlight->mapToGlobal(
      QPoint(0, ui->languageHighlight->height())));
  if (!selectedAction) {
    return;
  }

  QVariant selectedData = selectedAction->data();
  if (!selectedData.isValid()) {
    setHighlightOverrideForFile(filePath, "");
    applyHighlightForFile(filePath);
    return;
  }

  applyLanguageOverride(selectedData.toString());
}

void MainWindow::on_actionAbout_triggered() {

  QFile TextFile(":/resources/messages/About.txt");

  if (TextFile.open(QIODevice::ReadOnly)) {
    QTextStream in(&TextFile);
    QString license = in.readAll();
    QMessageBox::information(this, tr("About Lightpad"), license,
                             QMessageBox::Close);
    TextFile.close();
  }
}

void MainWindow::on_actionAbout_Qt_triggered() { QApplication::aboutQt(); }

void MainWindow::on_tabWidth_clicked() {

  if (!popupTabWidth) {
    popupTabWidth = new PopupTabWidth(QStringList({"2", "4", "8"}), this);
    auto point = mapToGlobal(ui->tabWidth->pos());
    QRect rect(point.x(), point.y() - 2 * popupTabWidth->height() + height(),
               popupTabWidth->width(), popupTabWidth->height());
    popupTabWidth->setGeometry(rect);
  }

  else if (popupTabWidth->isHidden())
    popupTabWidth->show();

  else
    popupTabWidth->hide();
}

void MainWindow::on_actionReplace_in_file_triggered() {
  showFindReplace(false);
  if (findReplacePanel) {
    findReplacePanel->setGlobalMode(false);
    findReplacePanel->setFocusOnSearchBox();
  }
}

void MainWindow::on_actionReplace_in_project_triggered() {
  showFindReplace(false);
  if (!findReplacePanel) {
    return;
  }

  QString projectPath = m_projectRootPath;
  if (projectPath.isEmpty()) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    QString filePath = tabWidget
                           ? tabWidget->getFilePath(tabWidget->currentIndex())
                           : QString();
    if (!filePath.isEmpty()) {
      projectPath = QFileInfo(filePath).absolutePath();
    } else {
      projectPath = QDir::currentPath();
    }
  }

  findReplacePanel->setProjectPath(projectPath);
  findReplacePanel->setGlobalMode(true);
  findReplacePanel->setFocusOnSearchBox();
}

void MainWindow::on_actionKeyboard_shortcuts_triggered() {
  openShortcutsDialog();
}

void MainWindow::on_actionPreferences_triggered() {
  if (!preferences) {
    preferences = new Preferences(this);

    connect(preferences, &QObject::destroyed, this,
            [&] { preferences = nullptr; });
  }
}

void MainWindow::on_runButton_clicked() { runCurrentScript(); }

void MainWindow::on_debugButton_clicked() { startDebuggingForCurrentFile(); }

void MainWindow::on_actionRun_file_name_triggered() { runCurrentScript(); }

void MainWindow::on_actionDebug_file_name_triggered() {
  startDebuggingForCurrentFile();
}

void MainWindow::on_actionEdit_Configurations_triggered() {
  openConfigurationDialog();
}

void MainWindow::on_actionEdit_Debug_Configurations_triggered() {
  openDebugConfigurationDialog();
}

void MainWindow::on_magicButton_clicked() { formatCurrentDocument(); }

void MainWindow::on_actionFormat_Document_triggered() {
  formatCurrentDocument();
}

void MainWindow::on_actionEdit_Format_Configurations_triggered() {
  openFormatConfigurationDialog();
}

void MainWindow::on_actionGo_to_Line_triggered() { showGoToLineDialog(); }

void MainWindow::on_actionToggle_Minimap_triggered() {
  LightpadTabWidget *tabWidget = currentTabWidget();
  LightpadPage *page = qobject_cast<LightpadPage *>(tabWidget->currentWidget());
  if (page) {
    bool visible = page->isMinimapVisible();

    for (LightpadTabWidget *targetWidget : allTabWidgets()) {
      for (int i = 0; i < targetWidget->count(); i++) {
        LightpadPage *p = qobject_cast<LightpadPage *>(targetWidget->widget(i));
        if (p) {
          p->setMinimapVisible(!visible);
        }
      }
    }
  }
}

void MainWindow::on_actionSplit_Horizontally_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->splitHorizontal();
  }
}

void MainWindow::on_actionSplit_Vertically_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->splitVertical();
  }
}

void MainWindow::on_actionClose_Editor_Group_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->closeCurrentGroup();
  }
}

void MainWindow::on_actionFocus_Next_Group_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->focusNextGroup();
  }
}

void MainWindow::on_actionFocus_Previous_Group_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->focusPreviousGroup();
  }
}

void MainWindow::on_actionUnsplit_All_triggered() {
  if (m_splitEditorContainer) {
    m_splitEditorContainer->unsplitAll();
  }
}

void MainWindow::on_actionToggle_Terminal_triggered() {
  TerminalTabWidget *widget = ensureTerminalWidget();
  bool visible = widget->isVisible();
  widget->setVisible(!visible);
  ui->actionToggle_Terminal->setChecked(!visible);
}

void MainWindow::on_actionToggle_Source_Control_triggered() {
  ensureSourceControlPanel();

  bool visible = sourceControlDock->isVisible();
  sourceControlDock->setVisible(!visible);
  if (ui->actionToggle_Source_Control) {
    ui->actionToggle_Source_Control->setChecked(!visible);
  }
}

void MainWindow::on_actionOpen_To_Side_triggered() {
  if (!m_splitEditorContainer)
    return;

  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  QString filePath;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (tabWidget) {
    int index = tabWidget->currentIndex();
    if (index >= 0) {
      filePath = tabWidget->tabToolTip(index);
    }
  }

  if (filePath.isEmpty())
    return;

  LightpadTabWidget *newGroup = m_splitEditorContainer->splitHorizontal();
  if (newGroup) {
    openFileAndAddToNewTab(filePath);
  }
}

void MainWindow::on_actionGit_Log_triggered() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    QMessageBox::information(this, tr("Git Log"),
                             tr("No valid Git repository found."));
    return;
  }

  GitLogDialog dialog(m_gitIntegration, settings.theme, this);

  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    if (tabWidget) {
      int index = tabWidget->currentIndex();
      if (index >= 0) {
        QString filePath = tabWidget->tabToolTip(index);
        if (!filePath.isEmpty()) {
          dialog.setFilePath(filePath);
        }
      }
    }
  }

  dialog.exec();
}

void MainWindow::on_actionGit_File_History_triggered() { showFileHistory(); }

void MainWindow::showFileHistory() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    QMessageBox::information(this, tr("File History"),
                             tr("No valid Git repository found."));
    return;
  }

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty()) {
    QMessageBox::information(this, tr("File History"),
                             tr("No file is currently open."));
    return;
  }

  GitFileHistoryDialog dialog(m_gitIntegration, filePath, this);
  connect(&dialog, &GitFileHistoryDialog::viewCommitDiff, this,
          [this](const QString &hash) {
            GitDiffDialog diffDialog(m_gitIntegration, hash,
                                     GitDiffDialog::DiffTarget::Commit, false,
                                     settings.theme, this);
            diffDialog.exec();
          });
  dialog.exec();
}

void MainWindow::openReadOnlyTab(const QString &content, const QString &title,
                                 const QString &originalFilePath) {
  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  int newIndex = tabWidget->addTab(new LightpadPage(tabWidget), title);
  tabWidget->setCurrentIndex(newIndex);

  auto *page = tabWidget->getPage(newIndex);
  if (page) {
    auto *textArea = page->getTextArea();
    if (textArea) {
      textArea->setMainWindow(this);
      textArea->setPlainText(content);
      textArea->setReadOnly(true);

      if (!originalFilePath.isEmpty()) {
        applyHighlightForFile(originalFilePath);
      }
    }
  }
}

void MainWindow::on_actionGit_Rebase_triggered() {
  if (!m_gitIntegration || !m_gitIntegration->isValidRepository()) {
    QMessageBox::information(this, tr("Interactive Rebase"),
                             tr("No valid Git repository found."));
    return;
  }

  GitRebaseDialog dialog(m_gitIntegration, settings.theme, this);
  dialog.loadCommits("HEAD~10");
  dialog.exec();
}

void MainWindow::on_actionToggle_Heatmap_triggered(bool checked) {
  m_heatmapEnabled = checked;
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setHeatmapEnabled(checked);
    if (checked) {
      updateHeatmapForCurrentFile();
    }
  }
}

void MainWindow::on_actionToggle_CodeLens_triggered(bool checked) {
  m_codeLensEnabled = checked;
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->setCodeLensEnabled(checked);
    if (checked) {
      updateCodeLensForCurrentFile();
    } else {
      textArea->clearCodeLensEntries();
    }
  }
}

void MainWindow::updateHeatmapForCurrentFile() {
  if (!m_heatmapEnabled || !m_gitIntegration ||
      !m_gitIntegration->isValidRepository())
    return;

  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty())
    return;

  QMap<int, qint64> timestamps = m_gitIntegration->getBlameTimestamps(filePath);
  textArea->setHeatmapData(timestamps);
  textArea->setHeatmapEnabled(true);
}

void MainWindow::updateCodeLensForCurrentFile() {
  if (!m_codeLensEnabled || !m_gitIntegration ||
      !m_gitIntegration->isValidRepository())
    return;

  TextArea *textArea = getCurrentTextArea();
  if (!textArea)
    return;

  LightpadTabWidget *tabWidget = currentTabWidget();
  if (!tabWidget)
    return;

  QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
  if (filePath.isEmpty())
    return;

  QList<GitBlameLineInfo> blameLines = m_gitIntegration->getBlameInfo(filePath);
  if (blameLines.isEmpty())
    return;

  QMap<int, GitBlameLineInfo> blameMap;
  for (const auto &info : blameLines) {
    blameMap[info.lineNumber] = info;
  }

  QList<TextArea::CodeLensEntry> entries;
  QTextDocument *doc = textArea->document();
  if (!doc)
    return;

  for (int i = 0; i < doc->blockCount(); ++i) {
    QTextBlock block = doc->findBlockByNumber(i);
    QString line = block.text().trimmed();

    bool looksLikeFunction = false;
    if (line.contains('(') && !line.startsWith("//") &&
        !line.startsWith("/*") && !line.startsWith("*") &&
        !line.startsWith("#")) {

      if (line.endsWith('{') || line.endsWith(") {")) {
        looksLikeFunction = true;
      } else if (i + 1 < doc->blockCount()) {
        QString nextLine = doc->findBlockByNumber(i + 1).text().trimmed();
        if (nextLine == "{")
          looksLikeFunction = true;
      }
    }

    if (line.startsWith("class ") || line.startsWith("struct ")) {
      looksLikeFunction = true;
    }

    if (!looksLikeFunction)
      continue;

    int startLine = i + 1;
    int endLine = startLine;
    int braceDepth = 0;
    bool foundOpen = false;
    for (int j = i; j < doc->blockCount(); ++j) {
      QString bLine = doc->findBlockByNumber(j).text();
      for (QChar c : bLine) {
        if (c == '{') {
          braceDepth++;
          foundOpen = true;
        } else if (c == '}') {
          braceDepth--;
        }
      }
      if (foundOpen && braceDepth <= 0) {
        endLine = j + 1;
        break;
      }
    }

    QSet<QString> authors;
    int changeCount = 0;
    QString latestAuthor;
    QString latestDate;
    qint64 latestEpoch = 0;

    for (int ln = startLine; ln <= endLine; ++ln) {
      auto it = blameMap.find(ln);
      if (it != blameMap.end()) {
        authors.insert(it->author);
        changeCount++;

        if (latestAuthor.isEmpty() || it->relativeDate < latestDate) {
          latestAuthor = it->author;
          latestDate = it->relativeDate;
        }
      }
    }

    if (authors.isEmpty())
      continue;

    QString authorsText;
    if (authors.size() <= 3) {
      authorsText = QString("%1 author%2 (%3)")
                        .arg(authors.size())
                        .arg(authors.size() > 1 ? "s" : "")
                        .arg(QStringList(authors.values()).join(", "));
    } else {
      QStringList authorsList = authors.values();
      authorsText = QString("%1 authors (%2, %3, ...)")
                        .arg(authors.size())
                        .arg(authorsList[0], authorsList[1]);
    }

    TextArea::CodeLensEntry entry;
    entry.line = i;
    entry.text = QString("%1 | %2").arg(authorsText, latestDate);
    entry.symbolName = line.left(60);
    entries.append(entry);
  }

  textArea->setCodeLensEntries(entries);
}

void MainWindow::on_actionTransform_Uppercase_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->transformToUppercase();
  }
}

void MainWindow::on_actionTransform_Lowercase_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->transformToLowercase();
  }
}

void MainWindow::on_actionTransform_Title_Case_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->transformToTitleCase();
  }
}

void MainWindow::on_actionSort_Lines_Ascending_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->sortLinesAscending();
  }
}

void MainWindow::on_actionSort_Lines_Descending_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->sortLinesDescending();
  }
}

void MainWindow::on_actionToggle_Word_Wrap_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    bool enabled = textArea->wordWrapEnabled();
    textArea->setWordWrapEnabled(!enabled);
    ui->actionToggle_Word_Wrap->setChecked(!enabled);
  }
}

void MainWindow::on_actionToggle_Vim_Mode_triggered() {
  bool enabled = !settings.vimModeEnabled;
  updateAllTextAreas(&TextArea::setVimModeEnabled, enabled);
  settings.vimModeEnabled = enabled;
  ui->actionToggle_Vim_Mode->setChecked(enabled);
  if (enabled) {
    connectVimMode(getCurrentTextArea());
  } else {
    updateVimStatusLabel("");
    hideVimCommandPanel();
  }
  saveSettings();
}

void MainWindow::on_actionFold_Current_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->foldCurrentBlock();
  }
}

void MainWindow::on_actionUnfold_Current_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->unfoldCurrentBlock();
  }
}

void MainWindow::on_actionFold_All_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->foldAll();
  }
}

void MainWindow::on_actionUnfold_All_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->unfoldAll();
  }
}

void MainWindow::on_actionFold_Comments_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->foldComments();
  }
}

void MainWindow::on_actionUnfold_Comments_triggered() {
  TextArea *textArea = getCurrentTextArea();
  if (textArea) {
    textArea->unfoldComments();
  }
}

void MainWindow::setTheme(Theme theme) {
  settings.theme = theme;

  QString bgColor = settings.theme.backgroundColor.name();
  QString fgColor = settings.theme.foregroundColor.name();
  QString surfaceColor = settings.theme.surfaceColor.name();
  QString surfaceAltColor = settings.theme.surfaceAltColor.name();
  QString hoverColor = settings.theme.hoverColor.name();
  QString pressedColor = settings.theme.pressedColor.name();
  QString borderColor = settings.theme.borderColor.name();
  QString accentColor = settings.theme.accentColor.name();
  QString accentSoftColor = settings.theme.accentSoftColor.name();
  QString mutedTextColor = settings.theme.singleLineCommentFormat.name();
  QString successColor = settings.theme.successColor.name();
  QString warningColor = settings.theme.warningColor.name();
  QString errorColor = settings.theme.errorColor.name();

  QString styleSheet =

      "QWidget { "
      "background-color: " +
      bgColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "}"
      "QDialog { background-color: " +
      bgColor +
      "; }"

      "QMenu { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      surfaceColor +
      "; "
      "selection-background-color: " +
      hoverColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 8px; "
      "padding: 4px; "
      "}"
      "QMenu::item { "
      "padding: 8px 32px 8px 12px; "
      "border-radius: 4px; "
      "margin: 2px 4px; "
      "}"
      "QMenu::item:selected { "
      "background-color: " +
      hoverColor +
      "; "
      "}"
      "QMenu::separator { "
      "height: 1px; "
      "background: " +
      borderColor +
      "; "
      "margin: 4px 8px; "
      "}"
      "QMenu::icon { "
      "padding-left: 8px; "
      "}"

      "QMenuBar { "
      "background-color: " +
      surfaceColor +
      "; "
      "border-bottom: 1px solid " +
      borderColor +
      "; "
      "spacing: 4px; "
      "padding: 4px 6px; "
      "min-height: 28px; "
      "}"
      "QMenuBar::item { "
      "color: " +
      fgColor +
      "; "
      "padding: 6px 10px; "
      "margin: 0 2px; "
      "border-radius: 6px; "
      "}"
      "QMenuBar::item:selected { "
      "background-color: " +
      hoverColor +
      "; "
      "}"
      "QMenuBar::item:pressed { "
      "background-color: " +
      pressedColor +
      "; "
      "}"

      "QMessageBox { background-color: " +
      surfaceColor + "; color: " + fgColor +
      "; }"
      "QMessageBox QLabel { color: " +
      fgColor +
      "; }"
      "QMessageBox QCheckBox { color: " +
      fgColor +
      "; }"
      "QMessageBox QTextEdit, QMessageBox QPlainTextEdit { "
      "background-color: " +
      surfaceAltColor + "; color: " + fgColor + "; border: 1px solid " +
      borderColor +
      "; border-radius: 4px; "
      "}"

      "QPushButton { "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "padding: 8px 16px; "
      "background-color: " +
      surfaceColor +
      "; "
      "border-radius: 6px; "
      "font-weight: 500; "
      "}"
      "QPushButton:hover { "
      "background-color: " +
      hoverColor +
      "; "
      "border-color: " +
      accentColor +
      "; "
      "}"
      "QPushButton:pressed { "
      "background-color: " +
      pressedColor +
      "; "
      "}"
      "QPushButton:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "outline: none; "
      "}"
      "QPushButton:default { "
      "background-color: " +
      accentColor +
      "; "
      "border: 1px solid " +
      accentColor +
      "; "
      "color: " +
      bgColor +
      "; "
      "}"
      "QPushButton:default:hover { "
      "background-color: #6eb5ff; "
      "}"

      "QToolButton { "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid transparent; "
      "padding: 6px 10px; "
      "background-color: transparent; "
      "border-radius: 6px; "
      "}"
      "QToolButton:hover { "
      "background-color: " +
      hoverColor +
      "; "
      "border-color: " +
      borderColor +
      "; "
      "}"
      "QToolButton:pressed { "
      "background-color: " +
      pressedColor +
      "; "
      "}"
      "QToolButton:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QToolButton#runButton, QToolButton#debugButton, QToolButton#magicButton "
      "{ "
      "background-color: " +
      surfaceAltColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "padding: 6px; "
      "border-radius: 6px; "
      "}"
      "QToolButton#runButton:hover, QToolButton#debugButton:hover, "
      "QToolButton#magicButton:hover { "
      "background-color: " +
      hoverColor +
      "; "
      "border-color: " +
      accentColor +
      "; "
      "}"
      "QToolButton#languageHighlight, QToolButton#tabWidth { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "padding: 6px 10px; "
      "font-size: 12px; "
      "}"
      "QToolButton#languageHighlight:hover, QToolButton#tabWidth:hover { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QLabel#rowCol { "
      "color: " +
      mutedTextColor +
      "; "
      "font-size: 12px; "
      "padding: 0 4px; "
      "}"

      "QAbstractItemView { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      bgColor +
      "; "
      "outline: 0; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "}"
      "QAbstractItemView::item { "
      "padding: 6px 8px; "
      "border-radius: 4px; "
      "margin: 1px 2px; "
      "}"
      "QAbstractItemView::item:hover { "
      "background-color: " +
      hoverColor +
      "; "
      "}"
      "QAbstractItemView::item:focus { "
      "outline: none; "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QAbstractItemView::item:selected { "
      "background-color: " +
      accentSoftColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "}"
      "QHeaderView::section { "
      "background-color: " +
      surfaceColor +
      "; "
      "color: " +
      mutedTextColor +
      "; "
      "padding: 8px 10px; "
      "border: none; "
      "border-bottom: 1px solid " +
      borderColor +
      "; "
      "font-weight: 600; "
      "text-transform: uppercase; "
      "font-size: 11px; "
      "}"

      "QLineEdit { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 8px 12px; "
      "selection-background-color: " +
      accentSoftColor +
      "; "
      "selection-color: " +
      fgColor +
      "; "
      "}"
      "QLineEdit:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QLineEdit:disabled { "
      "background-color: " +
      surfaceColor +
      "; "
      "color: " +
      mutedTextColor +
      "; "
      "}"

      "QComboBox { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 6px 12px; "
      "min-height: 20px; "
      "}"
      "QComboBox::drop-down { "
      "border: none; "
      "width: 20px; "
      "}"
      "QComboBox:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QComboBox::down-arrow { "
      "image: none; "
      "border: 4px solid transparent; "
      "border-top-color: " +
      mutedTextColor +
      "; "
      "margin-top: 4px; "
      "}"
      "QComboBox QAbstractItemView { "
      "background-color: " +
      surfaceColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 4px; "
      "selection-background-color: " +
      hoverColor +
      "; "
      "}"

      "QPlainTextEdit { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      bgColor +
      "; "
      "border: none; "
      "}"
      "QTextEdit { "
      "color: " +
      fgColor +
      "; "
      "background-color: " +
      bgColor +
      "; "
      "border: none; "
      "}"

      "QGroupBox { "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 8px; "
      "margin-top: 16px; "
      "padding: 12px; "
      "font-weight: 600; "
      "}"
      "QGroupBox::title { "
      "subcontrol-origin: margin; "
      "subcontrol-position: top left; "
      "padding: 0 8px; "
      "color: " +
      mutedTextColor +
      "; "
      "font-size: 12px; "
      "}"

      "QRadioButton { "
      "color: " +
      fgColor +
      "; "
      "spacing: 8px; "
      "}"
      "QRadioButton::indicator { "
      "width: 16px; "
      "height: 16px; "
      "border-radius: 8px; "
      "}"
      "QRadioButton::indicator:checked { "
      "background-color: " +
      accentColor +
      "; "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"
      "QRadioButton::indicator:unchecked { "
      "background-color: " +
      bgColor +
      "; "
      "border: 2px solid " +
      mutedTextColor +
      "; "
      "}"
      "QRadioButton::indicator:unchecked:hover { "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"

      "QCheckBox { "
      "color: " +
      fgColor +
      "; "
      "spacing: 8px; "
      "}"
      "QCheckBox::indicator { "
      "width: 16px; "
      "height: 16px; "
      "border-radius: 4px; "
      "border: 2px solid " +
      mutedTextColor +
      "; "
      "background-color: " +
      bgColor +
      "; "
      "}"
      "QCheckBox::indicator:checked { "
      "background-color: " +
      accentColor +
      "; "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"
      "QCheckBox::indicator:hover { "
      "border: 2px solid " +
      accentColor +
      "; "
      "}"

      "QScrollBar:vertical { "
      "background-color: transparent; "
      "width: 12px; "
      "margin: 0; "
      "}"
      "QScrollBar::handle:vertical { "
      "background-color: " +
      borderColor +
      "; "
      "min-height: 32px; "
      "border-radius: 4px; "
      "margin: 2px 3px; "
      "}"
      "QScrollBar::handle:vertical:hover { "
      "background-color: " +
      mutedTextColor +
      "; "
      "}"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
      "height: 0; "
      "}"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
      "background: none; "
      "}"
      "QScrollBar:horizontal { "
      "background-color: transparent; "
      "height: 12px; "
      "margin: 0; "
      "}"
      "QScrollBar::handle:horizontal { "
      "background-color: " +
      borderColor +
      "; "
      "min-width: 32px; "
      "border-radius: 4px; "
      "margin: 3px 2px; "
      "}"
      "QScrollBar::handle:horizontal:hover { "
      "background-color: " +
      mutedTextColor +
      "; "
      "}"
      "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
      "width: 0; "
      "}"
      "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
      "background: none; "
      "}"

      "QToolTip { "
      "background-color: " +
      surfaceColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 6px 10px; "
      "}"

      "QSplitter::handle { "
      "background-color: " +
      borderColor +
      "; "
      "}"
      "QSplitter::handle:hover { "
      "background-color: " +
      accentColor +
      "; "
      "}"
      "QSplitter::handle:horizontal { width: 1px; }"
      "QSplitter::handle:vertical { height: 1px; }"

      "QStatusBar { "
      "background-color: " +
      surfaceColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "border-top: 1px solid " +
      borderColor +
      "; "
      "}"

      "QWidget#backgroundBottom { "
      "background-color: " +
      surfaceColor +
      "; "
      "border-top: 1px solid " +
      borderColor +
      "; "
      "}"
      "QWidget#backgroundBottom QToolButton { "
      "min-height: 28px; "
      "max-height: 28px; "
      "}"
      "QWidget#FindReplacePanel { "
      "background-color: " +
      surfaceColor +
      "; "
      "border-top: 1px solid " +
      borderColor +
      "; "
      "}"
      "QWidget#Terminal { "
      "background-color: " +
      surfaceColor +
      "; "
      "border-top: 1px solid " +
      borderColor +
      "; "
      "}"
      "QWidget#TerminalTabWidget { "
      "background-color: " +
      surfaceColor +
      "; "
      "border-top: 1px solid " +
      borderColor +
      "; "
      "}"

      "QDialog QPushButton { "
      "min-height: 32px; "
      "}"
      "QDialogButtonBox QPushButton { "
      "min-width: 80px; "
      "min-height: 32px; "
      "}"

      "LineEditIcon { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 2px; "
      "}"
      "LineEditIcon:hover { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "LineEditIcon QLineEdit { "
      "background: transparent; "
      "border: none; "
      "padding: 4px 6px; "
      "color: " +
      fgColor +
      "; "
      "}"
      "LineEditIcon QToolButton { "
      "background: transparent; "
      "border: none; "
      "padding: 4px; "
      "}"

      "QSpinBox { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "color: " +
      fgColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 6px; "
      "padding: 4px 8px; "
      "}"
      "QSpinBox:focus { "
      "border: 1px solid " +
      accentColor +
      "; "
      "}"
      "QSpinBox::up-button, QSpinBox::down-button { "
      "background-color: " +
      hoverColor +
      "; "
      "border: none; "
      "width: 16px; "
      "}"
      "QSpinBox::up-button:hover, QSpinBox::down-button:hover { "
      "background-color: " +
      pressedColor +
      "; "
      "}"

      "QProgressBar { "
      "background-color: " +
      surfaceAltColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-radius: 4px; "
      "text-align: center; "
      "color: " +
      fgColor +
      "; "
      "}"
      "QProgressBar::chunk { "
      "background-color: " +
      accentColor +
      "; "
      "border-radius: 3px; "
      "}";

  qApp->setStyleSheet(styleSheet);

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    applyTabWidgetTheme(tabWidget);
  }
  if (terminalWidget) {
    terminalWidget->applyTheme(theme);
  }

  if (commandPalette) {
    commandPalette->applyTheme(theme);
  }
  if (goToLineDialog) {
    goToLineDialog->applyTheme(theme);
  }
  if (goToSymbolDialog) {
    goToSymbolDialog->applyTheme(theme);
  }
  if (fileQuickOpen) {
    fileQuickOpen->applyTheme(theme);
  }
  if (recentFilesDialog) {
    recentFilesDialog->applyTheme(theme);
  }
  if (breadcrumbWidget) {
    breadcrumbWidget->applyTheme(theme);
  }
  if (problemsPanel) {
    problemsPanel->applyTheme(theme);
  }
  if (sourceControlPanel) {
    sourceControlPanel->applyTheme(theme);
  }
  if (debugPanel) {
    debugPanel->applyTheme(theme);
  }

  updateAllTextAreas(&TextArea::applySelectionPalette, settings.theme);
}

void MainWindow::setProjectRootPath(const QString &path) {
  QString normalizedPath = QDir::cleanPath(path);
  if (!normalizedPath.isEmpty()) {
    normalizedPath = QFileInfo(normalizedPath).absoluteFilePath();
  }

  QString previousRoot = m_projectRootPath;
  m_projectRootPath = normalizedPath;

  if (previousRoot != normalizedPath) {
    m_treeExpandedPaths.clear();
    loadTreeStateFromSettings(normalizedPath);
    m_treeScrollValue = 0;
    m_treeScrollValueInitialized = false;
  }

  if (!normalizedPath.isEmpty()) {
    ensureProjectSettings(normalizedPath);
  }

  ensureFileTreeModel();
  if (m_fileTreeModel) {
    QString rootPath =
        normalizedPath.isEmpty() ? QDir::home().path() : normalizedPath;
    m_fileTreeModel->setRootPath(rootPath);
    m_fileTreeModel->setRootHeaderLabel(normalizedPath);
    if (m_gitIntegration) {
      m_fileTreeModel->setGitIntegration(m_gitIntegration);
    }
  }

  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (page) {
        page->setProjectRootPath(normalizedPath);
        page->setTreeViewVisible(!normalizedPath.isEmpty());
        if (!normalizedPath.isEmpty()) {
          page->setModelRootIndex(normalizedPath);
        }
      }
    }
  }

  if (!normalizedPath.isEmpty()) {
    updateGitIntegrationForPath(normalizedPath);
  }

  if (!normalizedPath.isEmpty()) {
    DebugSettings::instance().initialize(normalizedPath);
    DebugConfigurationManager::instance().setWorkspaceFolder(normalizedPath);
    DebugConfigurationManager::instance().loadFromLightpadDir();
    BreakpointManager::instance().setWorkspaceFolder(normalizedPath);
    BreakpointManager::instance().loadFromLightpadDir();
    WatchManager::instance().setWorkspaceFolder(normalizedPath);
    WatchManager::instance().loadFromLightpadDir();
    RunTemplateManager::instance().setWorkspaceFolder(normalizedPath);
  }

  applyTreeExpandedStateToViews();
}

QString MainWindow::getProjectRootPath() const { return m_projectRootPath; }

GitIntegration *MainWindow::getGitIntegration() const {
  return m_gitIntegration;
}

GitFileSystemModel *MainWindow::getFileTreeModel() const {
  return m_fileTreeModel;
}

void MainWindow::ensureFileTreeModel() {
  if (m_fileTreeModel) {
    return;
  }

  m_fileTreeModel = new GitFileSystemModel(this);
  connect(m_fileTreeModel, &QFileSystemModel::directoryLoaded, this,
          [this](const QString &) { applyTreeExpandedStateToViews(); });
  QString rootPath =
      m_projectRootPath.isEmpty() ? QDir::home().path() : m_projectRootPath;
  m_fileTreeModel->setRootPath(rootPath);
  m_fileTreeModel->setRootHeaderLabel(m_projectRootPath);
  if (m_gitIntegration) {
    m_fileTreeModel->setGitIntegration(m_gitIntegration);
  }
}

QList<LightpadTreeView *> MainWindow::allTreeViews() const {
  QList<LightpadTreeView *> views;
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; i < tabWidget->count(); i++) {
      auto page = tabWidget->getPage(i);
      if (!page) {
        continue;
      }
      auto *view = qobject_cast<LightpadTreeView *>(page->getTreeView());
      if (view) {
        views.append(view);
      }
    }
  }
  return views;
}

void MainWindow::registerTreeView(LightpadTreeView *treeView) {
  if (!treeView) {
    return;
  }
  ensureFileTreeModel();
  if (!m_fileTreeModel) {
    return;
  }

  if (treeView->model() != m_fileTreeModel) {
    treeView->setModel(m_fileTreeModel);
  }

  QObject::disconnect(treeView, &QTreeView::expanded, this, nullptr);
  QObject::disconnect(treeView, &QTreeView::collapsed, this, nullptr);
  QObject::disconnect(treeView->verticalScrollBar(), &QScrollBar::valueChanged,
                      this, nullptr);
  connect(treeView, &QTreeView::expanded, this,
          [this](const QModelIndex &index) {
            trackTreeExpandedState(index, true);
          });
  connect(treeView, &QTreeView::collapsed, this,
          [this](const QModelIndex &index) {
            trackTreeExpandedState(index, false);
          });
  connect(treeView->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](int value) {
            if (m_treeScrollSyncing) {
              return;
            }
            m_treeScrollValue = value;
            m_treeScrollValueInitialized = true;
            m_treeScrollSyncing = true;
            for (LightpadTreeView *view : allTreeViews()) {
              if (!view) {
                continue;
              }
              QScrollBar *scrollBar = view->verticalScrollBar();
              if (!scrollBar || scrollBar->value() == value) {
                continue;
              }
              scrollBar->setValue(value);
            }
            m_treeScrollSyncing = false;
          });

  applyTreeStateToView(treeView);
  if (m_treeScrollValueInitialized) {
    treeView->verticalScrollBar()->setValue(m_treeScrollValue);
  }
}

void MainWindow::trackTreeExpandedState(const QModelIndex &index,
                                        bool expanded) {
  if (!m_fileTreeModel || !index.isValid()) {
    return;
  }

  QString path = QDir::cleanPath(m_fileTreeModel->filePath(index));
  if (path.isEmpty()) {
    return;
  }

  if (expanded) {
    QString rootPath = QDir::cleanPath(m_projectRootPath);
    QString currentPath = path;
    while (!currentPath.isEmpty()) {
      m_treeExpandedPaths.insert(currentPath);
      if (!rootPath.isEmpty() && currentPath == rootPath) {
        break;
      }
      QString parentPath = QFileInfo(currentPath).absolutePath();
      if (parentPath == currentPath) {
        break;
      }
      currentPath = parentPath;
    }
  } else {
    QString prefix = path + "/";
    for (auto it = m_treeExpandedPaths.begin();
         it != m_treeExpandedPaths.end();) {
      const QString &existing = *it;
      if (existing == path || existing.startsWith(prefix)) {
        it = m_treeExpandedPaths.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void MainWindow::applyTreeStateToView(QTreeView *treeView) {
  if (!treeView || !m_fileTreeModel) {
    return;
  }

  QString normalizedRoot = QDir::cleanPath(m_projectRootPath);
  for (const QString &path : m_treeExpandedPaths) {
    if (!normalizedRoot.isEmpty() && !path.startsWith(normalizedRoot)) {
      continue;
    }
    QModelIndex idx = m_fileTreeModel->index(path);
    if (idx.isValid()) {
      expandIndexInView(treeView, idx);
    }
  }
}

void MainWindow::applyTreeExpandedStateToViews() {
  if (!m_fileTreeModel) {
    return;
  }

  QString rootPath = m_projectRootPath.isEmpty() ? m_fileTreeModel->rootPath()
                                                 : m_projectRootPath;
  QString normalizedRoot = QDir::cleanPath(rootPath);

  for (LightpadTreeView *view : allTreeViews()) {
    if (!view) {
      continue;
    }
    for (const QString &path : m_treeExpandedPaths) {
      QString normalizedPath = QDir::cleanPath(path);
      if (!normalizedRoot.isEmpty() &&
          !normalizedPath.startsWith(normalizedRoot)) {
        continue;
      }

      QModelIndex idx = m_fileTreeModel->index(normalizedPath);
      if (idx.isValid()) {
        expandIndexInView(view, idx);
      }
    }
  }
}

void MainWindow::expandIndexInView(QTreeView *treeView,
                                   const QModelIndex &index) {
  if (!treeView || !index.isValid()) {
    return;
  }

  QList<QModelIndex> chain;
  QModelIndex current = index;
  while (current.isValid()) {
    chain.prepend(current);
    if (current == treeView->rootIndex()) {
      break;
    }
    current = current.parent();
  }

  for (const QModelIndex &item : chain) {
    if (m_fileTreeModel && m_fileTreeModel->canFetchMore(item)) {
      m_fileTreeModel->fetchMore(item);
    }
    treeView->expand(item);
  }
}

void MainWindow::loadTreeStateFromSettings(const QString &rootPath) {
  m_treeExpandedPaths.clear();

  if (rootPath.isEmpty()) {
    return;
  }

  SettingsManager &globalSettings = SettingsManager::instance();
  QJsonObject treeStates =
      globalSettings.getSettingsObject().value("treeStateByRoot").toObject();
  QString normalizedRoot = QDir::cleanPath(rootPath);
  QJsonObject state = treeStates.value(normalizedRoot).toObject();
  if (state.isEmpty() && normalizedRoot != rootPath) {
    state = treeStates.value(rootPath).toObject();
  }
  QJsonArray expanded = state.value("expanded").toArray();

  for (const QJsonValue &value : expanded) {
    QString path = QDir::cleanPath(value.toString());
    if (!path.isEmpty() &&
        (normalizedRoot.isEmpty() || path.startsWith(normalizedRoot))) {
      m_treeExpandedPaths.insert(path);
    }
  }
}

void MainWindow::persistTreeStateToSettings() {
  if (m_projectRootPath.isEmpty()) {
    return;
  }

  SettingsManager &globalSettings = SettingsManager::instance();
  QJsonObject treeStates =
      globalSettings.getSettingsObject().value("treeStateByRoot").toObject();
  QJsonObject state;

  QJsonArray expanded;
  for (const QString &path : m_treeExpandedPaths) {
    expanded.append(QDir::cleanPath(path));
  }
  state["expanded"] = expanded;

  QString normalizedRoot = QDir::cleanPath(m_projectRootPath);
  treeStates[normalizedRoot.isEmpty() ? m_projectRootPath : normalizedRoot] =
      state;
  globalSettings.setValue("treeStateByRoot", treeStates);
}
