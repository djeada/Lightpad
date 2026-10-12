#pragma once

// Private helpers shared by the MainWindow implementation files.
// Not part of the public interface; include only from mainwindow*.cpp.

#include <QAbstractItemView>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QCompleter>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringListModel>
#include <QTabBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QThread>
#include <QToolButton>
#include <QVBoxLayout>
#include <cstdio>
#include <cstring>
#include <functional>

#include "../completion/completionengine.h"
#include "../completion/completionproviderregistry.h"
#include "../completion/providers/keywordcompletionprovider.h"
#include "../completion/providers/lspcompletionprovider.h"
#include "../completion/providers/plugincompletionprovider.h"
#include "../completion/providers/snippetcompletionprovider.h"
#include "../core/autosavemanager.h"
#include "../core/io/filemanager.h"
#include "../core/lightpadpage.h"
#include "../core/logging/logger.h"
#include "../core/navigationhistory.h"
#include "../core/recentfilesmanager.h"
#include "../core/textarea.h"
#include "../dap/debugsettings.h"
#include "../definition/idefinitionprovider.h"
#include "../definition/languagelspdefinitionprovider.h"
#include "../definition/symbolnavigationservice.h"
#include "../diagnostics/diagnosticsmanager.h"
#include "../diagnostics/diagnosticutils.h"
#include "../filetree/gitfilesystemmodel.h"
#include "../format_templates/formattemplatemanager.h"
#include "../language/languagefeaturemanager.h"
#include "../latex/latexpreviewpanel.h"
#include "../latex/latextools.h"
#include "../markdown/markdownpreviewpanel.h"
#include "../markdown/markdowntools.h"
#include "../run_templates/runtemplatemanager.h"
#include "../syntax/syntaxpluginregistry.h"
#include "../test_templates/testconfiguration.h"
#include "../test_templates/testfileclassifier.h"
#include "../theme/colorcontrast.h"
#include "../theme/themeengine.h"
#include "../theme/themepalette.h"
#include "dialogs/cmaketargetpickerdialog.h"
#include "dialogs/commandpalette.h"
#include "dialogs/compareanythingdialog.h"
#include "dialogs/debugconfigurationdialog.h"
#include "dialogs/filequickopen.h"
#include "dialogs/filetimelinedialog.h"
#include "dialogs/formattemplateselector.h"
#include "dialogs/gitdiffdialog.h"
#include "dialogs/gitfilehistorydialog.h"
#include "dialogs/gitlogdialog.h"
#include "dialogs/gitrebasedialog.h"
#include "dialogs/gitworkbenchdialog.h"
#include "dialogs/processpickerdialog.h"
#include "dialogs/provenancelensdialog.h"
#include "dialogs/rebasetimelinedialog.h"
#include "dialogs/styleddialog.h"
#include "uistylehelper.h"

#include "../build/cmakeproject.h"
#include "../diagnostics/compilerdiagnosticparser.h"
#include "../filetree/filedirtreemodel.h"
#include "../run_templates/runtargetresolver.h"
#include "core/io/fileopenguard.h"
#include "dialogs/gotolinedialog.h"
#include "dialogs/gotosymboldialog.h"
#include "dialogs/languageserverstatusdialog.h"
#include "dialogs/preferences.h"
#include "dialogs/pythonenvironmentdialog.h"
#include "dialogs/recentfilesdialog.h"
#include "dialogs/runconfigurations.h"
#include "dialogs/runtemplateselector.h"
#include "dialogs/shortcuts.h"
#include "dialogs/testconfigurationdialog.h"
#include "dialogs/themedmessagebox.h"
#include "dialogs/themegallerydialog.h"
#include "dockutils.h"
#include "mainwindow.h"
#include "panels/breadcrumbwidget.h"
#include "panels/conflictcenterpanel.h"
#include "panels/conflictresolverview.h"
#include "panels/databasepanel.h"
#include "panels/debugpanel.h"
#include "panels/findreplacepanel.h"
#include "panels/problemspanel.h"
#include "panels/sourcecontrolpanel.h"
#include "panels/spliteditorcontainer.h"
#include "panels/terminal.h"
#include "panels/terminaltabwidget.h"
#include "panels/testpanel.h"
#include "popup.h"
#include "ui_mainwindow.h"
#include "viewers/imageviewer.h"
#include "widgets/hacker/hackerscanlineoverlay.h"
#include "widgets/notificationwidget.h"
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

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif


namespace {
QString statusBarTextButtonStyle() {
  const ThemeColors &tc = ThemeEngine::instance().activeTheme().colors;
  return QString("QToolButton {"
                 "  color: %1;"
                 "  padding: 0 8px;"
                 "  font-size: 12px;"
                 "  border: none;"
                 "  background: transparent;"
                 "}"
                 "QToolButton:hover {"
                 "  color: %2;"
                 "}")
      .arg(tc.textSecondary.name(), tc.textPrimary.name());
}

constexpr auto kCompoundDebugTargetPrefix = "compound:";

struct ExternalTerminalLaunch {
  QString program;
  QStringList arguments;
};

ExternalTerminalLaunch buildExternalTerminalLaunch(const QStringList &command) {
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  struct Candidate {
    const char *program;
    const char *separator;
  };
  static const Candidate candidates[] = {{"x-terminal-emulator", "-e"},
                                         {"gnome-terminal", "--"},
                                         {"konsole", "-e"},
                                         {"xfce4-terminal", "-x"},
                                         {"kitty", ""},
                                         {"alacritty", "-e"},
                                         {"xterm", "-e"}};
  for (const Candidate &candidate : candidates) {
    const QString resolved = QStandardPaths::findExecutable(candidate.program);
    if (resolved.isEmpty()) {
      continue;
    }
    ExternalTerminalLaunch launch;
    launch.program = resolved;
    if (qstrlen(candidate.separator) > 0) {
      launch.arguments << QString::fromLatin1(candidate.separator);
    }
    launch.arguments << command;
    return launch;
  }
#elif defined(Q_OS_MACOS)
  Q_UNUSED(command);
#else
  Q_UNUSED(command);
#endif
  return {};
}

constexpr auto kBuildDiagnosticsSource = "build";
constexpr auto kCurrentFileLspErrorNotificationKey = "current-file-lsp-error";
constexpr auto kSessionTabPathKey = "path";
constexpr auto kSessionTabCursorKey = "cursorPosition";
constexpr auto kSessionTabVerticalScrollKey = "verticalScroll";
constexpr auto kSessionTabHorizontalScrollKey = "horizontalScroll";

template <typename T> bool hasVisibleChildWidget(const QObject *parent) {
  if (!parent) {
    return false;
  }

  const QList<T *> widgets = parent->findChildren<T *>();
  for (T *widget : widgets) {
    if (widget && widget->isVisible()) {
      return true;
    }
  }
  return false;
}

QJsonArray settingsArray(const SettingsManager &settings, const QString &key) {
  const QJsonValue value = settings.getSettingsObject().value(key);
  return value.isArray() ? value.toArray() : QJsonArray();
}

QJsonObject settingsObject(const SettingsManager &settings,
                           const QString &key) {
  const QJsonValue value = settings.getSettingsObject().value(key);
  return value.isObject() ? value.toObject() : QJsonObject();
}

QString persistedThemeName(const ThemeDefinition &theme) {
  return (theme.name == "Custom" || theme.name == "Active") ? QString()
                                                            : theme.name;
}

QColor blendColor(const QColor &a, const QColor &b, qreal t) {
  t = qBound(0.0, t, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QColor glowShift(const QColor &base, const QColor &glow, qreal glowLevel,
                 qreal strength) {
  return blendColor(base, glow, qBound(0.0, glowLevel * strength, 1.0));
}

QString normalizeEditorTextForSave(const QString &filePath,
                                   const QString &languageId,
                                   const QString &content, int tabWidth) {
  if (LanguageCatalog::normalize(languageId) == "py") {
    return FileManager::expandTabsToSpaces(content, tabWidth);
  }
  return FileManager::normalizeContentForSave(filePath, content, tabWidth);
}

QTextCursor cursorAtLineColumn(QTextDocument *document, int line, int column) {
  QTextCursor cursor(document);
  if (!document || document->blockCount() == 0) {
    return cursor;
  }
  const QTextBlock block =
      document->findBlockByNumber(qBound(0, line, document->blockCount() - 1));
  if (!block.isValid()) {
    return cursor;
  }
  cursor.setPosition(block.position() +
                     qBound(0, column, qMax(0, block.length() - 1)));
  return cursor;
}
} // namespace

// Member template; defined here so every mainwindow*.cpp can instantiate it.
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
