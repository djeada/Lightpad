#include <QApplication>
#include <QBoxLayout>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStringListModel>
#include <QCompleter>
#include <QProcess>
#include <QMenuBar>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDockWidget>
#include <QSizePolicy>
#include <cstdio>

#include "panels/findreplacepanel.h"
#include "panels/problemspanel.h"
#include "../core/lightpadpage.h"
#include "../core/logging/logger.h"
#include "mainwindow.h"
#include "popup.h"
#include "dialogs/preferences.h"
#include "dialogs/runconfigurations.h"
#include "dialogs/runtemplateselector.h"
#include "dialogs/formattemplateselector.h"
#include "dialogs/shortcuts.h"
#include "dialogs/commandpalette.h"
#include "dialogs/gotolinedialog.h"
#include "dialogs/gotosymboldialog.h"
#include "dialogs/filequickopen.h"
#include "dialogs/recentfilesdialog.h"
#include "panels/terminaltabwidget.h"
#include "panels/breadcrumbwidget.h"
#include "panels/sourcecontrolpanel.h"
#include "../core/textarea.h"
#include "../core/recentfilesmanager.h"
#include "../core/navigationhistory.h"
#include "../core/autosavemanager.h"
#include "../run_templates/runtemplatemanager.h"
#include "../format_templates/formattemplatemanager.h"
#include "../syntax/syntaxpluginregistry.h"
#include "ui_mainwindow.h"
#include "../completion/completionengine.h"
#include "../completion/completionproviderregistry.h"
#include "../completion/providers/keywordcompletionprovider.h"
#include "../completion/providers/snippetcompletionprovider.h"
#include "../completion/providers/plugincompletionprovider.h"
#include "panels/spliteditorcontainer.h"
#include "viewers/imageviewer.h"
#ifdef HAVE_PDF_SUPPORT
#include "viewers/pdfviewer.h"
#endif
#include "../settings/settingsmanager.h"
#include "../git/gitintegration.h"
#include "panels/spliteditorcontainer.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , popupHighlightLanguage(nullptr)
    , popupTabWidth(nullptr)
    , preferences(nullptr)
    , findReplacePanel(nullptr)
    , terminalWidget(nullptr)
    , completer(nullptr)
    , m_completionEngine(nullptr)
    , highlightLanguage("")
    , font(QApplication::font())
    , commandPalette(nullptr)
    , problemsPanel(nullptr)
    , goToLineDialog(nullptr)
    , goToSymbolDialog(nullptr)
    , fileQuickOpen(nullptr)
    , recentFilesDialog(nullptr)
    , problemsStatusLabel(nullptr)
    , vimStatusLabel(nullptr)
    , m_vimCommandPanelActive(false)
    , m_connectedVimMode(nullptr)
    , breadcrumbWidget(nullptr)
    , recentFilesManager(nullptr)
    , navigationHistory(nullptr)
    , autoSaveManager(nullptr)
    , m_splitEditorContainer(nullptr)
    , m_gitIntegration(nullptr)
    , sourceControlPanel(nullptr)
    , sourceControlDock(nullptr)
{
    QApplication::instance()->installEventFilter(this);
    ui->setupUi(this);
    ui->menubar->setNativeMenuBar(false);

    showMaximized();

    auto layout = qobject_cast<QVBoxLayout*>(ui->centralwidget->layout());
    if (layout) {
        int tabIndex = layout->indexOf(ui->tabWidget);
        layout->removeWidget(ui->tabWidget);
        m_splitEditorContainer = new SplitEditorContainer(ui->centralwidget);
        m_splitEditorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_splitEditorContainer->adoptTabWidget(ui->tabWidget);
        layout->insertWidget(tabIndex >= 0 ? tabIndex : 0, m_splitEditorContainer);
        layout->setStretch(layout->indexOf(m_splitEditorContainer), 1);
        layout->setStretch(layout->indexOf(ui->backgroundBottom), 0);
        connect(m_splitEditorContainer, &SplitEditorContainer::currentGroupChanged, this,
            [this](LightpadTabWidget* tabWidget) {
                updateTabWidgetContext(tabWidget, tabWidget ? tabWidget->currentIndex() : -1);
            });
        connect(m_splitEditorContainer, &SplitEditorContainer::splitCountChanged, this,
            [this](int) {
                for (LightpadTabWidget* tabWidget : allTabWidgets()) {
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
    
    // Initialize recent files manager
    recentFilesManager = new RecentFilesManager(this);
    
    // Initialize navigation history
    setupNavigationHistory();
    
    // Initialize auto-save
    setupAutoSave();
    
    // Initialize new completion system
    setupCompletionSystem();
    
    // Legacy completer initialization kept for backward compatibility
    // TODO: Remove after confirming new system works
    QStringList wordList;
    wordList << "break" << "case" << "continue" << "default" << "do" << "else" 
             << "for" << "if" << "return" << "switch" << "while";
    wordList << "auto" << "char" << "const" << "class" << "namespace" << "template"
             << "public" << "private" << "protected" << "virtual" << "override";
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
    loadSettings();
    setWindowTitle("LightPad");
}

LightpadTabWidget* MainWindow::currentTabWidget() const
{
    if (m_splitEditorContainer) {
        LightpadTabWidget* tabWidget = m_splitEditorContainer->currentTabWidget();
        if (tabWidget) {
            return tabWidget;
        }
    }
    return ui->tabWidget;
}

QList<LightpadTabWidget*> MainWindow::allTabWidgets() const
{
    if (m_splitEditorContainer) {
        return m_splitEditorContainer->allTabWidgets();
    }
    return { ui->tabWidget };
}

void MainWindow::setRowCol(int row, int col)
{
    ui->rowCol->setText("Ln " + QString::number(row) + ", Col " + QString::number(col));
    ensureStatusLabels();
}

void MainWindow::connectVimMode(TextArea* textArea)
{
    if (!textArea) {
        return;
    }
    VimMode* vimMode = textArea->vimMode();
    if (!vimMode) {
        return;
    }

    disconnectVimMode();
    ensureStatusLabels();
    m_connectedVimMode = vimMode;

    connect(vimMode, &VimMode::modeChanged, this, [this, textArea](VimEditMode mode) {
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

    connect(vimMode, &VimMode::statusMessage, this, [this](const QString& message) {
        showVimStatusMessage(message);
    });

    connect(vimMode, &VimMode::commandBufferChanged, this, [this, textArea](const QString& buffer) {
        if (!textArea || !textArea->isVimModeEnabled()) {
            return;
        }
        VimMode* currentVim = textArea->vimMode();
        if (!currentVim || currentVim->mode() != VimEditMode::Command) {
            return;
        }
        if (buffer.startsWith("/") || buffer.startsWith("?")) {
            showVimCommandPanel(buffer.left(1), buffer.mid(1));
        } else {
            showVimCommandPanel(":", buffer);
        }
    });

    connect(vimMode, &VimMode::commandExecuted, this, [this](const QString& command) {
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
        } else if (command.startsWith("edit:")) {
            openFileAndAddToNewTab(command.mid(QString("edit:").size()));
        }
    });

    updateVimStatusLabel(textArea->isVimModeEnabled() ? vimMode->modeName() : "");
    if (!textArea->isVimModeEnabled()) {
        hideVimCommandPanel();
    }
}

void MainWindow::disconnectVimMode()
{
    if (!m_connectedVimMode) {
        return;
    }
    m_connectedVimMode->disconnect(this);
    m_connectedVimMode = nullptr;
}

void MainWindow::showVimCommandPanel(const QString& prefix, const QString& buffer)
{
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

void MainWindow::hideVimCommandPanel()
{
    if (findReplacePanel && findReplacePanel->isVimCommandMode()) {
        findReplacePanel->setVimCommandMode(false);
        findReplacePanel->close();
    }
    m_vimCommandPanelActive = false;
}

void MainWindow::setTabWidthLabel(QString text)
{
    ui->tabWidth->setText(text);

    if (preferences)
        preferences->setTabWidthLabel(text);
}

void MainWindow::setLanguageHighlightLabel(QString text)
{
    ui->languageHighlight->setText(text);
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    Q_UNUSED(event);

    if (preferences) {
        preferences->close();
    }
}

void MainWindow::loadSettings()
{
    if (QFileInfo(settingsPath).exists())
        settings.loadSettings(settingsPath);

    else
        setTabWidth(defaultTabWidth);

    // Load global font settings from user home config
    SettingsManager& globalSettings = SettingsManager::instance();
    globalSettings.loadSettings();
    QString fontFamily = globalSettings.getValue("fontFamily", "Ubuntu Mono").toString();
    int fontSize = globalSettings.getValue("fontSize", defaultFontSize).toInt();
    int fontWeight = globalSettings.getValue("fontWeight", 50).toInt();
    bool fontItalic = globalSettings.getValue("fontItalic", false).toBool();
    settings.mainFont = QFont(fontFamily, fontSize, fontWeight, fontItalic);

    updateAllTextAreas(&TextArea::loadSettings, settings);
    setTheme(settings.theme);
    if (ui->actionToggle_Vim_Mode) {
        ui->actionToggle_Vim_Mode->setChecked(settings.vimModeEnabled);
    }
    
    // Restore last session: project path and open tabs
    QString lastProject = globalSettings.getValue("lastProjectPath", "").toString();
    if (!lastProject.isEmpty() && QDir(lastProject).exists()) {
        setProjectRootPath(lastProject);
        QDir::setCurrent(lastProject);
    }
    
    QJsonArray openTabs = globalSettings.getValue("openTabs").toJsonArray();
    for (const QJsonValue& val : openTabs) {
        QString filePath = val.toString();
        if (!filePath.isEmpty() && QFileInfo(filePath).exists()) {
            openFileAndAddToNewTab(filePath);
        }
    }
}

void MainWindow::saveSettings()
{
    LOG_DEBUG(QString("Saving settings, showLineNumberArea: %1").arg(settings.showLineNumberArea));
    settings.saveSettings(settingsPath);
    
    // Save session state: project path and open tabs
    SettingsManager& globalSettings = SettingsManager::instance();
    globalSettings.setValue("lastProjectPath", m_projectRootPath);
    
    QJsonArray openTabs;
    for (LightpadTabWidget* tabWidget : allTabWidgets()) {
        for (int i = 0; i < tabWidget->count(); i++) {
            QString filePath = tabWidget->getFilePath(i);
            if (!filePath.isEmpty()) {
                openTabs.append(filePath);
            }
        }
    }
    globalSettings.setValue("openTabs", openTabs);
    globalSettings.saveSettings();
}

void MainWindow::applyLanguageOverride(const QString& extension, const QString& displayName)
{
    auto textArea = getCurrentTextArea();
    if (!textArea) {
        return;
    }

    LightpadTabWidget* tabWidget = currentTabWidget();
    QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
    if (filePath.isEmpty()) {
        return;
    }

    QString languageId = detectLanguageIdForExtension(extension);
    if (languageId.isEmpty()) {
        LOG_WARNING(QString("No language mapping for extension: %1").arg(extension));
        return;
    }

    setHighlightOverrideForFile(filePath, languageId);
    textArea->updateSyntaxHighlightTags("", languageId);
    textArea->setLanguage(languageId);
    highlightLanguage = languageId;
    setLanguageHighlightLabel(displayName);
}

void MainWindow::applyHighlightForFile(const QString& filePath)
{
    auto textArea = getCurrentTextArea();
    if (!textArea || filePath.isEmpty()) {
        return;
    }

    QString overrideLang = highlightOverrideForFile(filePath);
    QString languageId = overrideLang;
    if (languageId.isEmpty()) {
        languageId = detectLanguageIdForFile(filePath);
    }

    if (languageId.isEmpty()) {
        setLanguageHighlightLabel("Choose lang.");
        highlightLanguage.clear();
        return;
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
        for (const auto& info : diffLines) {
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
}

QString MainWindow::detectLanguageIdForExtension(const QString& extension) const
{
    auto& registry = SyntaxPluginRegistry::instance();
    ISyntaxPlugin* plugin = registry.getPluginByExtension(extension);
    if (plugin) {
        return plugin->languageId();
    }
    return QString();
}

QString MainWindow::detectLanguageIdForFile(const QString& filePath) const
{
    return detectLanguageIdForExtension(QFileInfo(filePath).completeSuffix().toLower());
}

QString MainWindow::displayNameForLanguage(const QString& languageId, const QString& extension) const
{
    auto& registry = SyntaxPluginRegistry::instance();
    ISyntaxPlugin* plugin = registry.getPluginByLanguageId(languageId);
    if (plugin) {
        return plugin->languageName();
    }

    QMap<QString, QString> langToExt;
    loadLanguageExtensions(langToExt);
    if (!extension.isEmpty()) {
        for (auto it = langToExt.begin(); it != langToExt.end(); ++it) {
            if (it.value().toLower() == extension) {
                return it.key();
            }
        }
    }
    return languageId;
}

void MainWindow::loadHighlightOverridesForDir(const QString& dirPath)
{
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
        LOG_WARNING(QString("Failed to parse highlight config: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray assignments = root.value("assignments").toArray();
    for (const QJsonValue& value : assignments) {
        QJsonObject obj = value.toObject();
        QString fileName = obj.value("file").toString();
        QString languageId = obj.value("language").toString();
        if (fileName.isEmpty() || languageId.isEmpty()) {
            continue;
        }
        QString absolutePath = dirPath + "/" + fileName;
        m_highlightOverrides[absolutePath] = languageId;
    }

    m_loadedHighlightOverrideDirs.insert(configDir);
    LOG_INFO(QString("Loaded %1 highlight overrides from %2").arg(assignments.size()).arg(configFile));
}

bool MainWindow::saveHighlightOverridesForDir(const QString& dirPath) const
{
    if (dirPath.isEmpty()) {
        return false;
    }

    QString configDir = dirPath + "/.lightpad";
    QString configFile = configDir + "/highlight_config.json";

    QJsonArray assignments;
    for (auto it = m_highlightOverrides.begin(); it != m_highlightOverrides.end(); ++it) {
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
            LOG_ERROR(QString("Failed to create config directory: %1").arg(configDir));
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

    LOG_INFO(QString("Saved %1 highlight overrides to %2").arg(assignments.size()).arg(configFile));
    return true;
}

QString MainWindow::highlightOverrideForFile(const QString& filePath)
{
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

void MainWindow::setHighlightOverrideForFile(const QString& filePath, const QString& languageId)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return;
    }

    m_highlightOverrides[filePath] = languageId;
    saveHighlightOverridesForDir(fileInfo.absoluteDir().path());
}

template <typename... Args>
void MainWindow::updateAllTextAreas(void (TextArea::*f)(Args... args), Args... args)
{
    for (LightpadTabWidget* tabWidget : allTabWidgets()) {
        auto textAreas = tabWidget->findChildren<TextArea*>();
        for (auto& textArea : textAreas)
            (textArea->*f)(args...);
    }

    if (ui->actionToggle_Vim_Mode) {
        ui->actionToggle_Vim_Mode->setChecked(settings.vimModeEnabled);
    }
}

void MainWindow::updateAllTextAreas(void (TextArea::*f)(const Theme&), const Theme& theme)
{
    for (LightpadTabWidget* tabWidget : allTabWidgets()) {
        auto textAreas = tabWidget->findChildren<TextArea*>();
        for (auto& textArea : textAreas)
            (textArea->*f)(theme);
    }
}

void MainWindow::keyPressEvent(QKeyEvent* keyEvent)
{

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
    
    // Command Palette: Ctrl+Shift+P
    else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && 
             keyEvent->key() == Qt::Key_P) {
        showCommandPalette();
    }
    
    // Problems Panel: Ctrl+Shift+M
    else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && 
             keyEvent->key() == Qt::Key_M) {
        showProblemsPanel();
    }
    
    // Go to Line: Ctrl+G
    else if (keyEvent->modifiers() == Qt::ControlModifier && 
             keyEvent->key() == Qt::Key_G) {
        showGoToLineDialog();
    }
    
    // Go to Symbol: Ctrl+Shift+O
    else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && 
             keyEvent->key() == Qt::Key_O) {
        showGoToSymbolDialog();
    }
    
    // File Quick Open: Ctrl+P
    else if (keyEvent->modifiers() == Qt::ControlModifier && 
             keyEvent->key() == Qt::Key_P) {
        showFileQuickOpen();
    }
    
    // Recent Files: Ctrl+E
    else if (keyEvent->modifiers() == Qt::ControlModifier && 
             keyEvent->key() == Qt::Key_E) {
        showRecentFilesDialog();
    }
    
    // Toggle Whitespace: Ctrl+Shift+W
    else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && 
             keyEvent->key() == Qt::Key_W) {
        toggleShowWhitespace();
    }
    
    // Toggle Indent Guides: Ctrl+Shift+I
    else if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && 
             keyEvent->key() == Qt::Key_I) {
        toggleShowIndentGuides();
    }
    
    // Navigate Back: Alt+Left
    else if (keyEvent->modifiers() == Qt::AltModifier && 
             keyEvent->key() == Qt::Key_Left) {
        navigateBack();
    }
    
    // Navigate Forward: Alt+Right
    else if (keyEvent->modifiers() == Qt::AltModifier && 
             keyEvent->key() == Qt::Key_Right) {
        navigateForward();
    }
}

int MainWindow::getTabWidth()
{
    return settings.tabWidth;
}

int MainWindow::getFontSize()
{
    return settings.mainFont.pointSize();
}

//use current page if empty
//else add new tab
void MainWindow::openFileAndAddToNewTab(QString filePath)
{
    LightpadTabWidget* tabWidget = currentTabWidget();

    QFileInfo fileInfo(filePath);
    if (filePath.isEmpty() || !fileInfo.exists() || fileInfo.isDir())
        return;

    if (m_projectRootPath.isEmpty()) {
        updateGitIntegrationForPath(filePath);
    }

    //check if file not already open (including viewer tabs)
    for (int i = 0; i < tabWidget->count(); i++) {
        QString tabFilePath = tabWidget->getFilePath(i);
        if (tabFilePath == filePath) {
            tabWidget->setCurrentIndex(i);
            return;
        }
    }
    
    QString extension = fileInfo.suffix().toLower();
    
    // Check if it's an image file
    if (ImageViewer::isSupportedImageFormat(extension)) {
        ImageViewer* imageViewer = new ImageViewer(this);
        if (imageViewer->loadImage(filePath)) {
            tabWidget->addViewerTab(imageViewer, filePath, m_projectRootPath);
        } else {
            delete imageViewer;
        }
        return;
    }
    
#ifdef HAVE_PDF_SUPPORT
    // Check if it's a PDF file
    if (PdfViewer::isSupportedPdfFormat(extension)) {
        PdfViewer* pdfViewer = new PdfViewer(this);
        if (pdfViewer->loadPdf(filePath)) {
            tabWidget->addViewerTab(pdfViewer, filePath, m_projectRootPath);
        } else {
            delete pdfViewer;
        }
        return;
    }
#endif

    // Default handling for text files
    TextArea* currentTextArea = getCurrentTextArea();
    bool currentIsViewer = tabWidget->isViewerTab(tabWidget->currentIndex());
    if (tabWidget->count() == 0 || currentIsViewer || !currentTextArea || !currentTextArea->toPlainText().isEmpty()) {
        tabWidget->addNewTab();
        // Note: addNewTab() already sets the correct current index (count() - 2)
        // since the last tab is the add-button placeholder
    }

    open(filePath);
    setFilePathAsTabText(filePath);

    auto page = tabWidget->getCurrentPage();

    if (page) {
        // Use project root path if set, otherwise show treeview based on file
        if (!m_projectRootPath.isEmpty()) {
            page->setProjectRootPath(m_projectRootPath);
            page->setModelRootIndex(m_projectRootPath);
        }
        page->setTreeViewVisible(!m_projectRootPath.isEmpty());
        page->setFilePath(filePath);
    }

    if (getCurrentTextArea())
        applyHighlightForFile(filePath);
    
    // Track in recent files
    if (recentFilesManager) {
        recentFilesManager->addFile(filePath);
    }
    
    // Update breadcrumb
    updateBreadcrumb(filePath);

    tabWidget->currentChanged(tabWidget->currentIndex());
}

void MainWindow::closeTabPage(QString filePath)
{
    for (LightpadTabWidget* tabWidget : allTabWidgets()) {
        for (int i = 0; i < tabWidget->count(); i++) {
            if (tabWidget->getFilePath(i) == filePath) {
                tabWidget->removeTab(i);
            }
        }
    }
}

void MainWindow::on_actionToggle_Full_Screen_triggered()
{
    if (isMaximized())
        showNormal();

    else
        showMaximized();
}

void MainWindow::on_actionQuit_triggered()
{
    close();
}

void MainWindow::undo()
{
    if (getCurrentTextArea())
        getCurrentTextArea()->undo();
}

void MainWindow::redo()
{
    if (getCurrentTextArea())
        getCurrentTextArea()->redo();
}

TextArea* MainWindow::getCurrentTextArea()
{
    LightpadTabWidget* tabWidget = currentTabWidget();
    if (tabWidget->currentWidget()->findChild<LightpadPage*>("widget"))
        return tabWidget->currentWidget()->findChild<LightpadPage*>("widget")->getTextArea();

    else if (tabWidget->currentWidget()->findChild<TextArea*>(""))
        return tabWidget->currentWidget()->findChild<TextArea*>("");

    return nullptr;
}

Theme MainWindow::getTheme()
{
    return settings.theme;
}

QFont MainWindow::getFont()
{
    return settings.mainFont;
}

TextAreaSettings MainWindow::getSettings()
{
    return settings;
}

void MainWindow::setTabWidth(int width)
{
    updateAllTextAreas(&TextArea::setTabWidth, width);
    settings.tabWidth = width;
}

void MainWindow::on_actionToggle_Undo_triggered()
{
    undo();
}

void MainWindow::on_actionToggle_Redo_triggered()
{
    redo();
}

void MainWindow::on_actionIncrease_Font_Size_triggered()
{
    updateAllTextAreas(&TextArea::increaseFontSize);
    settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
    SettingsManager::instance().setValue("fontSize", settings.mainFont.pointSize());
    SettingsManager::instance().saveSettings();
}

void MainWindow::on_actionDecrease_Font_Size_triggered()
{
    updateAllTextAreas(&TextArea::decreaseFontSize);
    settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
    SettingsManager::instance().setValue("fontSize", settings.mainFont.pointSize());
    SettingsManager::instance().saveSettings();
}

void MainWindow::on_actionReset_Font_Size_triggered()
{
    updateAllTextAreas(&TextArea::setFontSize, defaultFontSize);
    settings.mainFont.setPointSize(getCurrentTextArea()->fontSize());
    SettingsManager::instance().setValue("fontSize", settings.mainFont.pointSize());
    SettingsManager::instance().saveSettings();
}

void MainWindow::on_actionCut_triggered()
{

    if (getCurrentTextArea())
        getCurrentTextArea()->cut();
}

void MainWindow::on_actionCopy_triggered()
{

    if (getCurrentTextArea())
        getCurrentTextArea()->copy();
}

void MainWindow::on_actionPaste_triggered()
{

    if (getCurrentTextArea())
        getCurrentTextArea()->paste();
}

void MainWindow::on_actionNew_Window_triggered()
{
    new MainWindow();
}

void MainWindow::on_actionClose_Tab_triggered()
{
    LightpadTabWidget* tabWidget = currentTabWidget();
    if (tabWidget->currentIndex() > -1)
        tabWidget->removeTab(tabWidget->currentIndex());
}

void MainWindow::on_actionClose_All_Tabs_triggered()
{
    currentTabWidget()->closeAllTabs();
}

void MainWindow::on_actionFind_in_file_triggered()
{
    showFindReplace(true);
}

void MainWindow::on_actionNew_File_triggered()
{
    currentTabWidget()->addNewTab();
}

void MainWindow::on_actionOpen_File_triggered()
{
    auto filePath = QFileDialog::getOpenFileName(this, tr("Open Document"), QDir::homePath());

    openFileAndAddToNewTab(filePath);
}

void MainWindow::on_actionOpen_Project_triggered()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Open Project"), QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (folderPath.isEmpty()) {
        return;
    }

    // Store the project root path for persistence across all tabs
    setProjectRootPath(folderPath);

    QDir::setCurrent(folderPath);
    setMainWindowTitle(QFileInfo(folderPath).fileName());
    if (fileQuickOpen) {
        fileQuickOpen->setRootDirectory(folderPath);
    }
}

void MainWindow::on_actionSave_triggered()
{
    LightpadTabWidget* tabWidget = currentTabWidget();
    auto tabIndex = tabWidget->currentIndex();
    auto filePath = tabWidget->getFilePath(tabIndex);

    if (filePath.isEmpty()) {
        on_actionSave_as_triggered();
        return;
    }

    save(filePath);
}

void MainWindow::on_actionSave_as_triggered()
{

    auto filePath = QFileDialog::getSaveFileName(this, tr("Save Document"), QDir::homePath());

    if (filePath.isEmpty())
        return;

    LightpadTabWidget* tabWidget = currentTabWidget();
    int tabIndex = tabWidget->currentIndex();
    tabWidget->setFilePath(tabIndex, filePath);

    save(filePath);
}

void MainWindow::open(const QString& filePath)
{

    QFile file(filePath);

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't open file."));
        return;
    }

    LightpadTabWidget* tabWidget = currentTabWidget();
    int tabIndex = tabWidget->currentIndex();
    tabWidget->setFilePath(tabIndex, filePath);

    if (getCurrentTextArea())
        getCurrentTextArea()->setPlainText(QString::fromUtf8(file.readAll()));
}

void MainWindow::save(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
        return;

    if (getCurrentTextArea()) {
        LightpadTabWidget* tabWidget = currentTabWidget();
        auto tabIndex = tabWidget->currentIndex();
        tabWidget->setFilePath(tabIndex, filePath);

        file.write(getCurrentTextArea()->toPlainText().toUtf8());
        getCurrentTextArea()->document()->setModified(false);
        getCurrentTextArea()->removeIconUnsaved();
        setFilePathAsTabText(filePath);
        
        // Notify problems panel for auto-refresh on save
        if (problemsPanel) {
            problemsPanel->onFileSaved(filePath);
        }
    }
}

void MainWindow::showFindReplace(bool onlyFind)
{
    if (!findReplacePanel) {
        findReplacePanel = new FindReplacePanel(onlyFind);

        auto layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());

        if (layout != 0)
            layout->insertWidget(layout->count() - 1, findReplacePanel, 0);
        
        // Connect navigation signal to open file at location
        connect(findReplacePanel, &FindReplacePanel::navigateToFile, 
                this, [this](const QString& filePath, int lineNumber, int columnNumber) {
            // For local search results, filePath is empty - use current text area
            if (!filePath.isEmpty()) {
                openFileAndAddToNewTab(filePath);
            }
            TextArea* textArea = getCurrentTextArea();
            if (textArea) {
                QTextCursor cursor = textArea->textCursor();
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineNumber - 1);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, columnNumber - 1);
                textArea->setTextCursor(cursor);
                textArea->setFocus();
            }
        });

        connect(findReplacePanel, &QObject::destroyed, this, [this]() {
            findReplacePanel = nullptr;
            m_vimCommandPanelActive = false;
        });
    }

    if (!m_vimCommandPanelActive) {
        findReplacePanel->setVisible(!findReplacePanel->isVisible() || findReplacePanel->isOnlyFind() != onlyFind);
        findReplacePanel->setOnlyFind(onlyFind);
    } else {
        findReplacePanel->setVisible(true);
        findReplacePanel->setOnlyFind(true);
    }

    if (findReplacePanel->isVisible() && getCurrentTextArea())
        findReplacePanel->setReplaceVisibility(!onlyFind);

    if (findReplacePanel->isVisible()) {
        findReplacePanel->setTextArea(getCurrentTextArea());
        findReplacePanel->setProjectPath(QDir::currentPath());
        findReplacePanel->setMainWindow(this);
        findReplacePanel->setFocusOnSearchBox();
    }
}

void MainWindow::openDialog(Dialog dialog)
{
    switch (dialog) {
    case Dialog::runConfiguration: {
        // Use new template selector
        auto page = currentTabWidget()->getCurrentPage();
        QString filePath = page ? page->getFilePath() : QString();
        
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Run Configuration", 
                "Please open a file first to configure run settings.");
            return;
        }
        
        if (findChildren<RunTemplateSelector*>().isEmpty()) {
            auto selector = new RunTemplateSelector(filePath, this);
            selector->setAttribute(Qt::WA_DeleteOnClose);
            selector->show();
        }
        break;
    }
    
    case Dialog::formatConfiguration: {
        // Use new format template selector
        auto page = currentTabWidget()->getCurrentPage();
        QString filePath = page ? page->getFilePath() : QString();
        
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "Format Configuration", 
                "Please open a file first to configure format settings.");
            return;
        }
        
        if (findChildren<FormatTemplateSelector*>().isEmpty()) {
            auto selector = new FormatTemplateSelector(filePath, this);
            selector->setAttribute(Qt::WA_DeleteOnClose);
            selector->show();
        }
        break;
    }

    case Dialog::shortcuts:
        if (findChildren<ShortcutsDialog*>().isEmpty())
            new ShortcutsDialog(this);
        break;

    default:
        return;
    }
}

void MainWindow::openConfigurationDialog()
{
    openDialog(Dialog::runConfiguration);
}

void MainWindow::openFormatConfigurationDialog()
{
    openDialog(Dialog::formatConfiguration);
}

void MainWindow::openShortcutsDialog()
{
    openDialog(Dialog::shortcuts);
}

void MainWindow::showTerminal()
{
    auto page = currentTabWidget()->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();

    if (filePath.isEmpty()) {
        noScriptAssignedWarning();
        return;
    }

    if (!terminalWidget) {
        terminalWidget = new TerminalTabWidget();
        terminalWidget->applyTheme(settings.theme);

        connect(terminalWidget, &TerminalTabWidget::closeRequested, this, [&]() {
            if (terminalWidget) {
                terminalWidget->hide();
            }
        });

        auto layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());

        if (layout != 0)
            layout->insertWidget(layout->count() - 1, terminalWidget, 0);
    }
    
    terminalWidget->show();
    
    // Run the file using the template system
    terminalWidget->runFile(filePath);
}

void MainWindow::showProblemsPanel()
{
    if (m_vimCommandPanelActive) {
        ensureStatusLabels();
        return;
    }
    if (!problemsPanel) {
        problemsPanel = new ProblemsPanel(this);
        
        connect(problemsPanel, &ProblemsPanel::problemClicked, this, [this](const QString& filePath, int line, int column) {
            openFileAndAddToNewTab(filePath);
            TextArea* textArea = getCurrentTextArea();
            if (textArea) {
                QTextCursor cursor = textArea->textCursor();
                cursor.movePosition(QTextCursor::Start);
                cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
                textArea->setTextCursor(cursor);
                textArea->setFocus();
            }
        });
        
        // Connect countsChanged to update status bar
        connect(problemsPanel, &ProblemsPanel::countsChanged, this, &MainWindow::updateProblemsStatusLabel);
        
        ensureStatusLabels();
        
        auto layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());
        if (layout != 0)
            layout->insertWidget(layout->count() - 1, problemsPanel, 0);
    }
    
    if (!m_vimCommandPanelActive) {
        problemsPanel->setVisible(!problemsPanel->isVisible());
    }
}

void MainWindow::ensureStatusLabels()
{
    if (!problemsStatusLabel) {
        problemsStatusLabel = new QLabel(this);
        problemsStatusLabel->setStyleSheet("color: #9aa4b2; padding: 0 8px;");
        problemsStatusLabel->setText("✓ No problems");
        problemsStatusLabel->setCursor(Qt::PointingHandCursor);
        
        // Make it clickable to toggle problems panel
        problemsStatusLabel->installEventFilter(this);
        
        auto layout = qobject_cast<QHBoxLayout*>(ui->backgroundBottom->layout());
        if (layout) {
            // Insert before the rowCol label (second to last widget)
            layout->insertWidget(layout->count() - 1, problemsStatusLabel);
        }
    }

    if (!vimStatusLabel) {
        vimStatusLabel = new QLabel(this);
        vimStatusLabel->setStyleSheet("color: #9aa4b2; padding: 0 8px;");
        vimStatusLabel->setText("");
        vimStatusLabel->setVisible(false);
        
        auto layout = qobject_cast<QHBoxLayout*>(ui->backgroundBottom->layout());
        if (layout) {
            int insertIndex = layout->count() - 1;
            if (problemsStatusLabel) {
                insertIndex = qMax(0, layout->indexOf(problemsStatusLabel) + 1);
            }
            layout->insertWidget(insertIndex, vimStatusLabel);
        }
    }
}

void MainWindow::ensureSourceControlPanel()
{
    if (sourceControlDock) {
        return;
    }

    sourceControlPanel = new SourceControlPanel(this);
    sourceControlPanel->setGitIntegration(m_gitIntegration);
    sourceControlPanel->setWorkingPath(m_projectRootPath.isEmpty() ? QDir::currentPath() : m_projectRootPath);

    connect(sourceControlPanel, &SourceControlPanel::fileOpenRequested, this,
            [this](const QString& filePath) { openFileAndAddToNewTab(filePath); });
    connect(sourceControlPanel, &SourceControlPanel::repositoryInitialized, this,
            [this](const QString& path) {
                setProjectRootPath(path);
                updateGitIntegrationForPath(path);
            });

    sourceControlDock = new QDockWidget(tr("Source Control"), this);
    sourceControlDock->setObjectName("sourceControlDock");
    sourceControlDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    sourceControlDock->setWidget(sourceControlPanel);
    addDockWidget(Qt::RightDockWidgetArea, sourceControlDock);
    sourceControlDock->hide();

    connect(sourceControlDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (ui->actionToggle_Source_Control) {
            ui->actionToggle_Source_Control->setChecked(visible);
        }
    });
}

void MainWindow::showCommandPalette()
{
    if (commandPalette) {
        commandPalette->showPalette();
    }
}

void MainWindow::setupCommandPalette()
{
    commandPalette = new CommandPalette(this);
    
    // Register all menus
    QMenuBar* menuBar = this->menuBar();
    for (QAction* action : menuBar->actions()) {
        if (action->menu()) {
            commandPalette->registerMenu(action->menu());
        }
    }
}

void MainWindow::showGoToLineDialog()
{
    if (!goToLineDialog) {
        setupGoToLineDialog();
    }
    
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        int maxLine = textArea->blockCount();
        goToLineDialog->setMaxLine(maxLine);
        goToLineDialog->showDialog();
    }
}

void MainWindow::setupGoToLineDialog()
{
    goToLineDialog = new GoToLineDialog(this);
    
    connect(goToLineDialog, &GoToLineDialog::lineSelected, this, [this](int lineNumber) {
        TextArea* textArea = getCurrentTextArea();
        if (textArea) {
            QTextCursor cursor = textArea->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineNumber - 1);
            textArea->setTextCursor(cursor);
            textArea->centerCursor();
            textArea->setFocus();
        }
    });
}

void MainWindow::showGoToSymbolDialog()
{
    if (!goToSymbolDialog) {
        setupGoToSymbolDialog();
    }
    
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        // For now, we'll create sample symbols based on simple parsing
        // In a real implementation, this would use LSP document symbols
        QList<LspDocumentSymbol> symbols;
        
        // Simple heuristic parsing for functions/classes
        QTextBlock block = textArea->document()->begin();
        while (block.isValid()) {
            QString text = block.text().trimmed();
            
            // Look for function-like patterns
            if (text.contains('(') && !text.startsWith("//") && !text.startsWith("/*")) {
                // Try to extract function name
                int parenPos = text.indexOf('(');
                QString beforeParen = text.left(parenPos).trimmed();
                
                // Get the last word before the parenthesis as the function name
                QStringList parts = beforeParen.split(QRegularExpression("\\s+"));
                if (!parts.isEmpty()) {
                    QString name = parts.last();
                    // Clean up any leading characters like * or &
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
                        
                        // Check for class/struct definitions
                        if (beforeParen.startsWith("class ") || beforeParen.startsWith("struct ")) {
                            sym.kind = LspSymbolKind::Class;
                        }
                        
                        symbols.append(sym);
                    }
                }
            }
            // Look for class/struct definitions without parentheses
            else if (text.startsWith("class ") || text.startsWith("struct ")) {
                QStringList parts = text.split(QRegularExpression("\\s+"));
                if (parts.size() >= 2) {
                    QString name = parts[1];
                    // Remove trailing characters like : or {
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
            // Look for Python def/class
            else if (text.startsWith("def ") || text.startsWith("class ")) {
                QString keyword = text.startsWith("def ") ? "def " : "class ";
                QString rest = text.mid(keyword.length());
                int endPos = rest.indexOf(QRegularExpression("[:(]"));
                if (endPos > 0) {
                    QString name = rest.left(endPos).trimmed();
                    
                    LspDocumentSymbol sym;
                    sym.name = name;
                    sym.kind = text.startsWith("def ") ? LspSymbolKind::Function : LspSymbolKind::Class;
                    sym.selectionRange.start.line = block.blockNumber();
                    sym.selectionRange.start.character = 0;
                    sym.range = sym.selectionRange;
                    symbols.append(sym);
                }
            }
            // Look for JavaScript function declarations
            else if (text.startsWith("function ")) {
                QString rest = text.mid(9);  // Length of "function "
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

void MainWindow::setupGoToSymbolDialog()
{
    goToSymbolDialog = new GoToSymbolDialog(this);
    
    connect(goToSymbolDialog, &GoToSymbolDialog::symbolSelected, this, [this](int line, int column) {
        TextArea* textArea = getCurrentTextArea();
        if (textArea) {
            QTextCursor cursor = textArea->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
            textArea->setTextCursor(cursor);
            textArea->centerCursor();
            textArea->setFocus();
        }
    });
}

void MainWindow::showFileQuickOpen()
{
    if (!fileQuickOpen) {
        setupFileQuickOpen();
    }
    
    // Set the current working directory as root
    QString rootPath = QDir::currentPath();
    
    // If we have a file open, use its directory
    LightpadTabWidget* tabWidget = currentTabWidget();
    auto tabIndex = tabWidget->currentIndex();
    auto filePath = tabWidget->getFilePath(tabIndex);
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        rootPath = fileInfo.absolutePath();
        // Try to find project root (look for .git, CMakeLists.txt, etc.)
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

void MainWindow::setupFileQuickOpen()
{
    fileQuickOpen = new FileQuickOpen(this);
    
    connect(fileQuickOpen, &FileQuickOpen::fileSelected, this, [this](const QString& filePath) {
        openFileAndAddToNewTab(filePath);
    });
}

void MainWindow::showRecentFilesDialog()
{
    if (!recentFilesDialog) {
        setupRecentFilesDialog();
    }
    
    recentFilesDialog->showDialog();
}

void MainWindow::setupRecentFilesDialog()
{
    recentFilesDialog = new RecentFilesDialog(recentFilesManager, this);
    
    connect(recentFilesDialog, &RecentFilesDialog::fileSelected, this, [this](const QString& filePath) {
        openFileAndAddToNewTab(filePath);
    });
}

void MainWindow::setupBreadcrumb()
{
    breadcrumbWidget = new BreadcrumbWidget(this);
    
    // Insert breadcrumb above the tab widget in the layout
    auto layout = qobject_cast<QVBoxLayout*>(ui->centralwidget->layout());
    if (layout) {
        // Insert at position 0 (top)
        layout->insertWidget(0, breadcrumbWidget);
    }
    
    connect(breadcrumbWidget, &BreadcrumbWidget::pathSegmentClicked, this, [this](const QString& path) {
        // Open folder or file when breadcrumb segment is clicked
        QFileInfo fileInfo(path);
        if (fileInfo.isDir()) {
            // Could open in file tree, for now just log
        } else if (fileInfo.isFile()) {
            openFileAndAddToNewTab(path);
        }
    });
}

void MainWindow::updateBreadcrumb(const QString& filePath)
{
    if (breadcrumbWidget) {
        breadcrumbWidget->setFilePath(filePath);
        if (!m_projectRootPath.isEmpty()) {
            breadcrumbWidget->setProjectRoot(m_projectRootPath);
        }
    }
}

void MainWindow::toggleShowWhitespace()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->setShowWhitespace(!textArea->showWhitespace());
    }
}

void MainWindow::toggleShowIndentGuides()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->setShowIndentGuides(!textArea->showIndentGuides());
    }
}

void MainWindow::navigateBack()
{
    if (!navigationHistory || !navigationHistory->canGoBack()) {
        return;
    }
    
    NavigationLocation loc = navigationHistory->goBack();
    if (loc.isValid()) {
        // Open file if different
        openFileAndAddToNewTab(loc.filePath);
        
        // Move cursor to the saved position
        TextArea* textArea = getCurrentTextArea();
        if (textArea) {
            QTextCursor cursor = textArea->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, loc.line - 1);
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, loc.column);
            textArea->setTextCursor(cursor);
            textArea->centerCursor();
        }
    }
}

void MainWindow::navigateForward()
{
    if (!navigationHistory || !navigationHistory->canGoForward()) {
        return;
    }
    
    NavigationLocation loc = navigationHistory->goForward();
    if (loc.isValid()) {
        // Open file if different
        openFileAndAddToNewTab(loc.filePath);
        
        // Move cursor to the saved position
        TextArea* textArea = getCurrentTextArea();
        if (textArea) {
            QTextCursor cursor = textArea->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, loc.line - 1);
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, loc.column);
            textArea->setTextCursor(cursor);
            textArea->centerCursor();
        }
    }
}

void MainWindow::recordNavigationLocation()
{
    if (!navigationHistory) {
        return;
    }
    
    TextArea* textArea = getCurrentTextArea();
    if (!textArea) {
        return;
    }
    
    // Get current file path
    LightpadTabWidget* tabWidget = currentTabWidget();
    auto tabIndex = tabWidget->currentIndex();
    QString filePath = tabWidget->getFilePath(tabIndex);
    if (filePath.isEmpty()) {
        return;
    }
    
    // Get cursor position
    QTextCursor cursor = textArea->textCursor();
    NavigationLocation loc;
    loc.filePath = filePath;
    loc.line = cursor.blockNumber() + 1;
    loc.column = cursor.positionInBlock();
    
    navigationHistory->recordLocationIfSignificant(loc);
}

void MainWindow::setupNavigationHistory()
{
    navigationHistory = new NavigationHistory(this);
}

void MainWindow::setupAutoSave()
{
    autoSaveManager = new AutoSaveManager(this, this);
    // Auto-save is disabled by default, can be enabled via settings
}

void MainWindow::setupGitIntegration()
{
    if (m_gitIntegration) {
        return;
    }

    m_gitIntegration = new GitIntegration(this);
    updateGitIntegrationForPath(QDir::currentPath());
}

void MainWindow::updateGitIntegrationForPath(const QString& path)
{
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
        sourceControlPanel->setWorkingPath(isRepo ? m_gitIntegration->repositoryPath() : path);
        sourceControlPanel->refresh();
    }

    auto textArea = getCurrentTextArea();
    LightpadTabWidget* tabWidget = currentTabWidget();
    QString currentFilePath = tabWidget->getFilePath(tabWidget->currentIndex());
    if (textArea && !currentFilePath.isEmpty()) {
        QList<GitDiffLineInfo> diffLines = m_gitIntegration->getDiffLines(currentFilePath);
        QList<QPair<int, int>> gutterLines;
        gutterLines.reserve(diffLines.size());
        for (const auto& info : diffLines) {
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
}

void MainWindow::applyGitIntegrationToAllPages()
{
    if (!m_gitIntegration) {
        return;
    }

    for (LightpadTabWidget* tabWidget : allTabWidgets()) {
        for (int i = 0; i < tabWidget->count(); i++) {
            auto page = tabWidget->getPage(i);
            if (page) {
                page->setGitIntegration(m_gitIntegration);
            }
        }
    }
}

void MainWindow::updateProblemsStatusLabel(int errors, int warnings, int infos)
{
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

void MainWindow::updateVimStatusLabel(const QString& text)
{
    if (vimStatusLabel) {
        vimStatusLabel->setText(text);
        vimStatusLabel->setVisible(!text.isEmpty());
    }
}

void MainWindow::showVimStatusMessage(const QString& message)
{
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

void MainWindow::setMainWindowTitle(QString title)
{
    setWindowTitle(title + " - Lightpad");
}

void MainWindow::setFont(QFont newFont)
{
    updateAllTextAreas(&TextArea::setFont, newFont);
    font = newFont;
}

void MainWindow::showLineNumbers(bool flag)
{
    updateAllTextAreas(&TextArea::showLineNumbers, flag);
    settings.showLineNumberArea = flag;
}

void MainWindow::highlihtCurrentLine(bool flag)
{
    updateAllTextAreas(&TextArea::highlihtCurrentLine, flag);
    settings.lineHighlighted = flag;
}

void MainWindow::highlihtMatchingBracket(bool flag)
{
    updateAllTextAreas(&TextArea::highlihtMatchingBracket, flag);
    settings.matchingBracketsHighlighted = flag;
}

void MainWindow::runCurrentScript()
{
    on_actionSave_triggered();
    auto textArea = getCurrentTextArea();

    if (textArea && !textArea->changesUnsaved())
        showTerminal();
}

void MainWindow::formatCurrentDocument()
{
    auto page = currentTabWidget()->getCurrentPage();
    QString filePath = page ? page->getFilePath() : QString();
    
    if (filePath.isEmpty()) {
        QMessageBox::information(this, "Format Document", 
            "Please save the file first before formatting.");
        return;
    }
    
    // Check for unsaved changes before formatting
    auto textArea = getCurrentTextArea();
    bool hadUnsavedChanges = textArea && textArea->changesUnsaved();
    
    // Save the file first
    on_actionSave_triggered();
    
    // Verify the file was saved (check if still has unsaved changes)
    if (textArea && textArea->changesUnsaved()) {
        QMessageBox::warning(this, "Format Document", 
            "Could not save the file. Formatting cancelled.");
        return;
    }
    
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    
    // Ensure templates are loaded
    if (manager.getAllTemplates().isEmpty()) {
        manager.loadTemplates();
    }
    
    QPair<QString, QStringList> command = manager.buildCommand(filePath);
    
    if (command.first.isEmpty()) {
        QMessageBox::information(this, "Format Document", 
            "No formatter found for this file type.\nUse Format > Edit Format Configurations to assign one.");
        return;
    }
    
    // Execute the formatter
    QProcess process;
    process.setWorkingDirectory(QFileInfo(filePath).absoluteDir().path());
    process.start(command.first, command.second);
    
    if (!process.waitForFinished(60000)) {
        QMessageBox::warning(this, "Format Document", 
            "Formatting timed out or failed to start.\nMake sure the formatter is installed and in PATH.");
        return;
    }
    
    if (process.exitCode() != 0) {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        QString stdOut = QString::fromUtf8(process.readAllStandardOutput());
        LOG_WARNING(QString("Formatter exited with code %1: %2").arg(process.exitCode()).arg(errorOutput));
        
        // Show error to user
        QString errorMsg = QString("Formatter exited with error code %1.").arg(process.exitCode());
        if (!errorOutput.isEmpty()) {
            errorMsg += QString("\n\nError output:\n%1").arg(errorOutput.left(500));
        }
        QMessageBox::warning(this, "Format Document", errorMsg);
    }
    
    // Reload the file if it was modified in place
    if (textArea) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString newContent = QString::fromUtf8(file.readAll());
            file.close();
            
            // Only update if content changed
            if (textArea->toPlainText() != newContent) {
                // Save cursor position
                int cursorPos = textArea->textCursor().position();
                
                textArea->setPlainText(newContent);
                // Mark as unmodified since we just formatted and the file matches the disk
                textArea->document()->setModified(false);
                
                // Restore cursor position as close as possible
                QTextCursor cursor = textArea->textCursor();
                cursor.setPosition(qMin(cursorPos, textArea->toPlainText().length()));
                textArea->setTextCursor(cursor);
            }
        } else {
            LOG_WARNING(QString("Failed to reload file after formatting: %1").arg(filePath));
        }
    }
}

void MainWindow::setFilePathAsTabText(QString filePath)
{

    auto fileName = QFileInfo(filePath).fileName();

    LightpadTabWidget* tabWidget = currentTabWidget();
    auto tabIndex = tabWidget->currentIndex();
    auto tabText = tabWidget->tabText(tabIndex);

    setMainWindowTitle(fileName);
    tabWidget->setTabText(tabIndex, fileName);
}

void MainWindow::closeCurrentTab()
{
    auto textArea = getCurrentTextArea();

    if (textArea && textArea->changesUnsaved())
        on_actionSave_triggered();

    currentTabWidget()->closeCurrentTab();
}

void MainWindow::setupTabWidgetConnections(LightpadTabWidget* tabWidget)
{
    QObject::connect(tabWidget, &QTabWidget::currentChanged, this, [this, tabWidget](int index) {
        updateTabWidgetContext(tabWidget, index);
    });
}

void MainWindow::updateTabWidgetContext(LightpadTabWidget* tabWidget, int index)
{
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
}

void MainWindow::applyTabWidgetTheme(LightpadTabWidget* tabWidget)
{
    if (!tabWidget) {
        return;
    }
    tabWidget->setTheme(settings.theme.backgroundColor.name(),
        settings.theme.foregroundColor.name(),
        settings.theme.surfaceColor.name(),
        settings.theme.hoverColor.name(),
        settings.theme.accentColor.name(),
        settings.theme.borderColor.name());
}

void MainWindow::setupTabWidget()
{
    applyTabWidgetTheme(ui->tabWidget);
    setupTabWidgetConnections(ui->tabWidget);
    updateTabWidgetContext(ui->tabWidget, 0);
}

void MainWindow::setupCompletionSystem()
{
    // Register completion providers
    auto& registry = CompletionProviderRegistry::instance();
    
    // Register keyword provider
    registry.registerProvider(std::make_shared<KeywordCompletionProvider>());
    
    // Register snippet provider
    registry.registerProvider(std::make_shared<SnippetCompletionProvider>());
    
    // Register plugin provider for syntax plugin keywords
    registry.registerProvider(std::make_shared<PluginCompletionProvider>());
    
    // Create the completion engine
    m_completionEngine = new CompletionEngine(this);
    
    QStringList providerIds = registry.allProviderIds();
    if (providerIds.isEmpty()) {
        LOG_WARNING("Completion system initialized but no providers registered");
    } else {
        LOG_INFO("Completion system initialized with providers: " + providerIds.join(", "));
    }
}

void MainWindow::setupTextArea()
{

    if (getCurrentTextArea()) {
        auto* textArea = getCurrentTextArea();
        textArea->setMainWindow(this);
        textArea->setFontSize(settings.mainFont.pointSize());
        textArea->setTabWidth(settings.tabWidth);
        textArea->setVimModeEnabled(settings.vimModeEnabled);
        ensureStatusLabels();
        connectVimMode(textArea);
        
        // Setup new completion system (preferred)
        if (m_completionEngine) {
            textArea->setCompletionEngine(m_completionEngine);
            LightpadTabWidget* tabWidget = currentTabWidget();
            QString filePath = tabWidget->getFilePath(tabWidget->currentIndex());
            applyHighlightForFile(filePath);
        }
        
        // Legacy: Setup old completer as fallback
        if (completer && !m_completionEngine)
            textArea->setCompleter(completer);
    }
}

void MainWindow::noScriptAssignedWarning()
{
    QMessageBox msgBox(this);
    msgBox.setText("No file is currently open.");
    msgBox.setInformativeText("Open a file first, then you can run it or configure a run template.");
    auto openButton = msgBox.addButton(tr("Open File"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();

    if (msgBox.clickedButton() == openButton)
        on_actionOpen_File_triggered();
}

void MainWindow::on_actionToggle_Menu_Bar_triggered()
{
    ui->menubar->setVisible(true);
}

void MainWindow::on_languageHighlight_clicked()
{

    if (!popupHighlightLanguage) {
        auto dir = QDir(":/resources/highlight").entryList(QStringList(), QDir::Dirs);
        auto popupHighlightLanguage = new PopupLanguageHighlight(dir, this);
        auto point = mapToGlobal(ui->languageHighlight->pos());
        popupHighlightLanguage->setGeometry(point.x(), point.y() - 2 * popupHighlightLanguage->height() + height(), popupHighlightLanguage->width(), popupHighlightLanguage->height());
    }

    else if (popupHighlightLanguage->isHidden())
        popupHighlightLanguage->show();

    else
        popupHighlightLanguage->hide();
}

void MainWindow::on_actionAbout_triggered()
{

    QFile TextFile(":/resources/messages/About.txt");

    if (TextFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&TextFile);
        QString license = in.readAll();
        QMessageBox::information(this, tr("About Lightpad"), license, QMessageBox::Close);
        TextFile.close();
    }
}

void MainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void MainWindow::on_tabWidth_clicked()
{

    if (!popupTabWidth) {
        auto popupTabWidth = new PopupTabWidth(QStringList({ "2", "4", "8" }), this);
        auto point = mapToGlobal(ui->tabWidth->pos());
        QRect rect(point.x(), point.y() - 2 * popupTabWidth->height() + height(), popupTabWidth->width(), popupTabWidth->height());
        popupTabWidth->setGeometry(rect);
    }

    else if (popupTabWidth->isHidden())
        popupTabWidth->show();

    else
        popupTabWidth->hide();
}

void MainWindow::on_actionReplace_in_file_triggered()
{
    showFindReplace(false);
}

void MainWindow::on_actionKeyboard_shortcuts_triggered()
{
    openShortcutsDialog();
}

void MainWindow::on_actionPreferences_triggered()
{
    if (!preferences) {
        preferences = new Preferences(this);

        connect(preferences, &QObject::destroyed, this, [&] {
            preferences = nullptr;
        });
    }
}

void MainWindow::on_runButton_clicked()
{
    runCurrentScript();
}

void MainWindow::on_actionRun_file_name_triggered()
{
    runCurrentScript();
}

void MainWindow::on_actionEdit_Configurations_triggered()
{
    openConfigurationDialog();
}

void MainWindow::on_magicButton_clicked()
{
    formatCurrentDocument();
}

void MainWindow::on_actionFormat_Document_triggered()
{
    formatCurrentDocument();
}

void MainWindow::on_actionEdit_Format_Configurations_triggered()
{
    openFormatConfigurationDialog();
}

void MainWindow::on_actionGo_to_Line_triggered()
{
    showGoToLineDialog();
}

void MainWindow::on_actionToggle_Minimap_triggered()
{
    LightpadTabWidget* tabWidget = currentTabWidget();
    LightpadPage* page = qobject_cast<LightpadPage*>(tabWidget->currentWidget());
    if (page) {
        bool visible = page->isMinimapVisible();
        // Toggle minimap on all tabs for consistency
        for (LightpadTabWidget* targetWidget : allTabWidgets()) {
            for (int i = 0; i < targetWidget->count(); i++) {
                LightpadPage* p = qobject_cast<LightpadPage*>(targetWidget->widget(i));
                if (p) {
                    p->setMinimapVisible(!visible);
                }
            }
        }
    }
}

void MainWindow::on_actionSplit_Horizontally_triggered()
{
    if (m_splitEditorContainer) {
        m_splitEditorContainer->splitHorizontal();
    }
}

void MainWindow::on_actionSplit_Vertically_triggered()
{
    if (m_splitEditorContainer) {
        m_splitEditorContainer->splitVertical();
    }
}

void MainWindow::on_actionClose_Editor_Group_triggered()
{
    if (m_splitEditorContainer) {
        m_splitEditorContainer->closeCurrentGroup();
    }
}

void MainWindow::on_actionFocus_Next_Group_triggered()
{
    if (m_splitEditorContainer) {
        m_splitEditorContainer->focusNextGroup();
    }
}

void MainWindow::on_actionFocus_Previous_Group_triggered()
{
    if (m_splitEditorContainer) {
        m_splitEditorContainer->focusPreviousGroup();
    }
}

void MainWindow::on_actionUnsplit_All_triggered()
{
    if (m_splitEditorContainer) {
        m_splitEditorContainer->unsplitAll();
    }
}

void MainWindow::on_actionToggle_Terminal_triggered()
{
    if (!terminalWidget) {
        terminalWidget = new TerminalTabWidget();
        terminalWidget->applyTheme(settings.theme);

        connect(terminalWidget, &TerminalTabWidget::closeRequested, this, [this]() {
            if (terminalWidget) {
                terminalWidget->hide();
                ui->actionToggle_Terminal->setChecked(false);
            }
        });

        auto layout = qobject_cast<QBoxLayout*>(ui->centralwidget->layout());

        if (layout != 0)
            layout->insertWidget(layout->count() - 1, terminalWidget, 0);
    }
    
    bool visible = terminalWidget->isVisible();
    terminalWidget->setVisible(!visible);
    ui->actionToggle_Terminal->setChecked(!visible);
}

void MainWindow::on_actionToggle_Source_Control_triggered()
{
    ensureSourceControlPanel();

    bool visible = sourceControlDock->isVisible();
    sourceControlDock->setVisible(!visible);
    if (ui->actionToggle_Source_Control) {
        ui->actionToggle_Source_Control->setChecked(!visible);
    }
}

// ============================================================================
// Text Transformation Actions
// ============================================================================

void MainWindow::on_actionTransform_Uppercase_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->transformToUppercase();
    }
}

void MainWindow::on_actionTransform_Lowercase_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->transformToLowercase();
    }
}

void MainWindow::on_actionTransform_Title_Case_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->transformToTitleCase();
    }
}

void MainWindow::on_actionSort_Lines_Ascending_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->sortLinesAscending();
    }
}

void MainWindow::on_actionSort_Lines_Descending_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->sortLinesDescending();
    }
}

// ============================================================================
// View Actions
// ============================================================================

void MainWindow::on_actionToggle_Word_Wrap_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        bool enabled = textArea->wordWrapEnabled();
        textArea->setWordWrapEnabled(!enabled);
        ui->actionToggle_Word_Wrap->setChecked(!enabled);
    }
}

void MainWindow::on_actionToggle_Vim_Mode_triggered()
{
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

void MainWindow::on_actionFold_Current_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->foldCurrentBlock();
    }
}

void MainWindow::on_actionUnfold_Current_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->unfoldCurrentBlock();
    }
}

void MainWindow::on_actionFold_All_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->foldAll();
    }
}

void MainWindow::on_actionUnfold_All_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->unfoldAll();
    }
}

void MainWindow::on_actionFold_Comments_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->foldComments();
    }
}

void MainWindow::on_actionUnfold_Comments_triggered()
{
    TextArea* textArea = getCurrentTextArea();
    if (textArea) {
        textArea->unfoldComments();
    }
}

void MainWindow::setTheme(Theme theme)
{
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
        // Base widget styling with smooth font rendering
        "QWidget { "
            "background-color: " + bgColor + "; "
            "color: " + fgColor + "; "
        "}"
        "QDialog { background-color: " + bgColor + "; }"

        // Modern menu styling with refined shadows
        "QMenu { "
            "color: " + fgColor + "; "
            "background-color: " + surfaceColor + "; "
            "selection-background-color: " + hoverColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 8px; "
            "padding: 4px; "
        "}"
        "QMenu::item { "
            "padding: 8px 32px 8px 12px; "
            "border-radius: 4px; "
            "margin: 2px 4px; "
        "}"
        "QMenu::item:selected { "
            "background-color: " + hoverColor + "; "
        "}"
        "QMenu::separator { "
            "height: 1px; "
            "background: " + borderColor + "; "
            "margin: 4px 8px; "
        "}"
        "QMenu::icon { "
            "padding-left: 8px; "
        "}"

        // Menu bar styling with better spacing
        "QMenuBar { "
            "background-color: " + surfaceColor + "; "
            "border-bottom: 1px solid " + borderColor + "; "
            "spacing: 4px; "
            "padding: 4px 6px; "
            "min-height: 28px; "
        "}"
        "QMenuBar::item { "
            "color: " + fgColor + "; "
            "padding: 6px 10px; "
            "margin: 0 2px; "
            "border-radius: 6px; "
        "}"
        "QMenuBar::item:selected { "
            "background-color: " + hoverColor + "; "
        "}"
        "QMenuBar::item:pressed { "
            "background-color: " + pressedColor + "; "
        "}"

        // Message box styling
        "QMessageBox { background-color: " + surfaceColor + "; }"
        "QMessageBox QLabel { color: " + fgColor + "; }"

        // Modern button styling with subtle transitions
        "QPushButton { "
            "color: " + fgColor + "; "
            "border: 1px solid " + borderColor + "; "
            "padding: 8px 16px; "
            "background-color: " + surfaceColor + "; "
            "border-radius: 6px; "
            "font-weight: 500; "
        "}"
        "QPushButton:hover { "
            "background-color: " + hoverColor + "; "
            "border-color: " + accentColor + "; "
        "}"
        "QPushButton:pressed { "
            "background-color: " + pressedColor + "; "
        "}"
        "QPushButton:focus { "
            "border: 1px solid " + accentColor + "; "
            "outline: none; "
        "}"
        "QPushButton:default { "
            "background-color: " + accentColor + "; "
            "border: 1px solid " + accentColor + "; "
            "color: " + bgColor + "; "
        "}"
        "QPushButton:default:hover { "
            "background-color: #6eb5ff; "
        "}"

        // Tool button styling
        "QToolButton { "
            "color: " + fgColor + "; "
            "border: 1px solid transparent; "
            "padding: 6px 10px; "
            "background-color: transparent; "
            "border-radius: 6px; "
        "}"
        "QToolButton:hover { "
            "background-color: " + hoverColor + "; "
            "border-color: " + borderColor + "; "
        "}"
        "QToolButton:pressed { "
            "background-color: " + pressedColor + "; "
        "}"
        "QToolButton:focus { "
            "border: 1px solid " + accentColor + "; "
        "}"
        "QToolButton#runButton, QToolButton#magicButton { "
            "background-color: " + surfaceAltColor + "; "
            "border: 1px solid " + borderColor + "; "
            "padding: 6px; "
            "border-radius: 6px; "
        "}"
        "QToolButton#runButton:hover, QToolButton#magicButton:hover { "
            "background-color: " + hoverColor + "; "
            "border-color: " + accentColor + "; "
        "}"
        "QToolButton#languageHighlight, QToolButton#tabWidth { "
            "background-color: " + surfaceAltColor + "; "
            "border: 1px solid " + borderColor + "; "
            "padding: 4px 10px; "
            "font-size: 12px; "
        "}"
        "QToolButton#languageHighlight:hover, QToolButton#tabWidth:hover { "
            "border: 1px solid " + accentColor + "; "
        "}"
        "QLabel#rowCol { "
            "color: " + mutedTextColor + "; "
            "font-size: 12px; "
            "padding: 0 4px; "
        "}"

        // Tree view and list styling with improved spacing
        "QAbstractItemView { "
            "color: " + fgColor + "; "
            "background-color: " + bgColor + "; "
            "outline: 0; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 6px; "
        "}"
        "QAbstractItemView::item { "
            "padding: 6px 8px; "
            "border-radius: 4px; "
            "margin: 1px 2px; "
        "}"
        "QAbstractItemView::item:hover { "
            "background-color: " + hoverColor + "; "
        "}"
        "QAbstractItemView::item:focus { "
            "outline: none; "
            "border: 1px solid " + accentColor + "; "
        "}"
        "QAbstractItemView::item:selected { "
            "background-color: " + accentSoftColor + "; "
            "color: " + fgColor + "; "
        "}"
        "QHeaderView::section { "
            "background-color: " + surfaceColor + "; "
            "color: " + mutedTextColor + "; "
            "padding: 8px 10px; "
            "border: none; "
            "border-bottom: 1px solid " + borderColor + "; "
            "font-weight: 600; "
            "text-transform: uppercase; "
            "font-size: 11px; "
        "}"

        // Text input styling with better focus states
        "QLineEdit { "
            "background-color: " + surfaceAltColor + "; "
            "color: " + fgColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 6px; "
            "padding: 8px 12px; "
            "selection-background-color: " + accentSoftColor + "; "
            "selection-color: " + fgColor + "; "
        "}"
        "QLineEdit:focus { "
            "border: 1px solid " + accentColor + "; "
        "}"
        "QLineEdit:disabled { "
            "background-color: " + surfaceColor + "; "
            "color: " + mutedTextColor + "; "
        "}"

        // Combo box styling
        "QComboBox { "
            "background-color: " + surfaceAltColor + "; "
            "color: " + fgColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 6px; "
            "padding: 6px 12px; "
            "min-height: 20px; "
        "}"
        "QComboBox::drop-down { "
            "border: none; "
            "width: 20px; "
        "}"
        "QComboBox:focus { "
            "border: 1px solid " + accentColor + "; "
        "}"
        "QComboBox::down-arrow { "
            "image: none; "
            "border: 4px solid transparent; "
            "border-top-color: " + mutedTextColor + "; "
            "margin-top: 4px; "
        "}"
        "QComboBox QAbstractItemView { "
            "background-color: " + surfaceColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 6px; "
            "padding: 4px; "
            "selection-background-color: " + hoverColor + "; "
        "}"

        // Text editor styling
        "QPlainTextEdit { "
            "color: " + fgColor + "; "
            "background-color: " + bgColor + "; "
            "border: none; "
        "}"
        "QTextEdit { "
            "color: " + fgColor + "; "
            "background-color: " + bgColor + "; "
            "border: none; "
        "}"

        // Group boxes with refined styling
        "QGroupBox { "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 8px; "
            "margin-top: 16px; "
            "padding: 12px; "
            "font-weight: 600; "
        "}"
        "QGroupBox::title { "
            "subcontrol-origin: margin; "
            "subcontrol-position: top left; "
            "padding: 0 8px; "
            "color: " + mutedTextColor + "; "
            "font-size: 12px; "
        "}"

        // Modern radio button styling
        "QRadioButton { "
            "color: " + fgColor + "; "
            "spacing: 8px; "
        "}"
        "QRadioButton::indicator { "
            "width: 16px; "
            "height: 16px; "
            "border-radius: 8px; "
        "}"
        "QRadioButton::indicator:checked { "
            "background-color: " + accentColor + "; "
            "border: 2px solid " + accentColor + "; "
        "}"
        "QRadioButton::indicator:unchecked { "
            "background-color: " + bgColor + "; "
            "border: 2px solid " + mutedTextColor + "; "
        "}"
        "QRadioButton::indicator:unchecked:hover { "
            "border: 2px solid " + accentColor + "; "
        "}"

        // Checkbox styling with modern look
        "QCheckBox { "
            "color: " + fgColor + "; "
            "spacing: 8px; "
        "}"
        "QCheckBox::indicator { "
            "width: 16px; "
            "height: 16px; "
            "border-radius: 4px; "
            "border: 2px solid " + mutedTextColor + "; "
            "background-color: " + bgColor + "; "
        "}"
        "QCheckBox::indicator:checked { "
            "background-color: " + accentColor + "; "
            "border: 2px solid " + accentColor + "; "
        "}"
        "QCheckBox::indicator:hover { "
            "border: 2px solid " + accentColor + "; "
        "}"

        // Minimal scrollbars with modern feel
        "QScrollBar:vertical { "
            "background-color: transparent; "
            "width: 12px; "
            "margin: 0; "
        "}"
        "QScrollBar::handle:vertical { "
            "background-color: " + borderColor + "; "
            "min-height: 32px; "
            "border-radius: 4px; "
            "margin: 2px 3px; "
        "}"
        "QScrollBar::handle:vertical:hover { "
            "background-color: " + mutedTextColor + "; "
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
            "background-color: " + borderColor + "; "
            "min-width: 32px; "
            "border-radius: 4px; "
            "margin: 3px 2px; "
        "}"
        "QScrollBar::handle:horizontal:hover { "
            "background-color: " + mutedTextColor + "; "
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
            "width: 0; "
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
            "background: none; "
        "}"

        // Tooltip styling
        "QToolTip { "
            "background-color: " + surfaceColor + "; "
            "color: " + fgColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 6px; "
            "padding: 6px 10px; "
        "}"

        // Splitter styling
        "QSplitter::handle { "
            "background-color: " + borderColor + "; "
        "}"
        "QSplitter::handle:hover { "
            "background-color: " + accentColor + "; "
        "}"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"

        // Status bar styling
        "QStatusBar { "
            "background-color: " + surfaceColor + "; "
            "color: " + fgColor + "; "
            "border-top: 1px solid " + borderColor + "; "
        "}"

        // Bottom panel styling with subtle elevation
        "QWidget#backgroundBottom { "
            "background-color: " + surfaceColor + "; "
            "border-top: 1px solid " + borderColor + "; "
        "}"
        "QWidget#FindReplacePanel { "
            "background-color: " + surfaceColor + "; "
            "border-top: 1px solid " + borderColor + "; "
        "}"
        "QWidget#Terminal { "
            "background-color: " + surfaceColor + "; "
            "border-top: 1px solid " + borderColor + "; "
        "}"
        "QWidget#TerminalTabWidget { "
            "background-color: " + surfaceColor + "; "
            "border-top: 1px solid " + borderColor + "; "
        "}"
        
        // Dialog button box
        "QDialogButtonBox QPushButton { "
            "min-width: 80px; "
        "}"
        
        // Line edit with icon
        "LineEditIcon { "
            "background-color: " + surfaceAltColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 6px; "
            "padding: 2px; "
        "}"
        "LineEditIcon:hover { "
            "border: 1px solid " + accentColor + "; "
        "}"
        "LineEditIcon QLineEdit { "
            "background: transparent; "
            "border: none; "
            "padding: 4px 6px; "
            "color: " + fgColor + "; "
        "}"
        "LineEditIcon QToolButton { "
            "background: transparent; "
            "border: none; "
            "padding: 4px; "
        "}"
        
        // Spin box styling
        "QSpinBox { "
            "background-color: " + surfaceAltColor + "; "
            "color: " + fgColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 6px; "
            "padding: 4px 8px; "
        "}"
        "QSpinBox:focus { "
            "border: 1px solid " + accentColor + "; "
        "}"
        "QSpinBox::up-button, QSpinBox::down-button { "
            "background-color: " + hoverColor + "; "
            "border: none; "
            "width: 16px; "
        "}"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover { "
            "background-color: " + pressedColor + "; "
        "}"
        
        // Progress bar styling
        "QProgressBar { "
            "background-color: " + surfaceAltColor + "; "
            "border: 1px solid " + borderColor + "; "
            "border-radius: 4px; "
            "text-align: center; "
            "color: " + fgColor + "; "
        "}"
        "QProgressBar::chunk { "
            "background-color: " + accentColor + "; "
            "border-radius: 3px; "
        "}";

    qApp->setStyleSheet(styleSheet);

    for (LightpadTabWidget* tabWidget : allTabWidgets()) {
        applyTabWidgetTheme(tabWidget);
    }
    if (terminalWidget) {
        terminalWidget->applyTheme(theme);
    }
    updateAllTextAreas(&TextArea::applySelectionPalette, settings.theme);
}

void MainWindow::setProjectRootPath(const QString& path)
{
    m_projectRootPath = path;
    
    // Update all existing tabs with the new project root
    for (LightpadTabWidget* tabWidget : allTabWidgets()) {
        for (int i = 0; i < tabWidget->count(); i++) {
            auto page = tabWidget->getPage(i);
            if (page) {
                page->setProjectRootPath(path);
                page->setTreeViewVisible(!path.isEmpty());
                if (!path.isEmpty()) {
                    page->setModelRootIndex(path);
                }
            }
        }
    }

    if (!path.isEmpty()) {
        updateGitIntegrationForPath(path);
    }
}

QString MainWindow::getProjectRootPath() const
{
    return m_projectRootPath;
}

GitIntegration* MainWindow::getGitIntegration() const
{
    return m_gitIntegration;
}
