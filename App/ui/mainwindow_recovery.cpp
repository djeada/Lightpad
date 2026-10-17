#include "../core/documentrecovery.h"
#include "dialogs/documentrecoverydialog.h"
#include "mainwindow_internal.h"

namespace {
FileOpenGuard::TextFormat recoveryFormat(TextArea *area) {
  const auto data = area->property("recoveryFormat").toMap();
  if (data.isEmpty())
    return {};
  return {FileOpenGuard::TextEncoding(data.value("encoding").toInt()),
          data.value("bom").toBool(), data.value("lineEnding").toString()};
}
} // namespace
void MainWindow::setupDocumentRecovery() {
  m_documentRecovery = new DocumentRecovery(this);
  connect(m_documentRecovery, &DocumentRecovery::recoveryError, this,
          [this](const QString &error) {
            statusBar()->showMessage(
                tr("Recovery copy could not be updated: %1").arg(error), 15000);
          });
  auto *action = new QAction(tr("Recover Unsaved Documents…"), this);
  action->setObjectName("actionRecoverUnsavedDocuments");
  ui->menuFile->insertAction(ui->actionQuit, action);
  connect(action, &QAction::triggered, this,
          [this]() { showDocumentRecovery(); });
  for (auto *tabs : allTabWidgets())
    for (int i = 0; i < tabs->count(); ++i)
      if (!tabs->isViewerTab(i))
        if (auto *page = tabs->getPage(i))
          trackRecoveryEditor(page->getTextArea());
  QTimer::singleShot(200, this, [this]() {
    if (!m_documentRecovery->initializationError().isEmpty()) {
      statusBar()->showMessage(
          tr("Document recovery is unavailable: %1")
              .arg(m_documentRecovery->initializationError()),
          15000);
      return;
    }
    const auto scan = m_documentRecovery->recoverable();
    if (!scan.drafts.isEmpty() || !scan.warnings.isEmpty())
      showDocumentRecovery();
  });
}
void MainWindow::trackRecoveryEditor(TextArea *area) {
  if (!m_documentRecovery || !area)
    return;
  for (auto *tabs : allTabWidgets()) {
    for (int i = 0; i < tabs->count(); ++i) {
      auto *page = tabs->getPage(i);
      if (!page || page->getTextArea() != area || tabs->isViewerTab(i))
        continue;
      const QString path = tabs->getFilePath(i);
      if (!path.isEmpty()) {
        m_documentRecovery->track(area, path, QFileInfo(path).fileName(),
                                  m_fileFormats.value(QDir::cleanPath(path)));
      } else {
        const QString original =
            area->property("recoveryOriginalPath").toString();
        const QString title = area->property("recoveryTitle").toString();
        m_documentRecovery->track(area, original,
                                  title.isEmpty() ? tabs->tabText(i) : title,
                                  recoveryFormat(area));
      }
      return;
    }
  }
}
void MainWindow::showDocumentRecovery() {
  if (!m_documentRecovery)
    return;
  if (!m_documentRecoveryDialog) {
    auto *dialog = new DocumentRecoveryDialog(this);
    m_documentRecoveryDialog = dialog;
    dialog->setActions(
        [this]() { return m_documentRecovery->recoverable(); },
        [this](const Recovery::Draft &draft, QString *error) {
          auto *tabs = currentTabWidget();
          if (!tabs)
            return false;
          tabs->addNewTab();
          auto *area = getCurrentTextArea();
          if (!area)
            return false;
          const QString name =
              draft.title.isEmpty() ? tr("Untitled") : draft.title;
          area->setProperty("recoveryOriginalPath", draft.originalPath);
          area->setProperty("recoveryTitle", name);
          area->setProperty(
              "recoveryFormat",
              QVariantMap{{"encoding", int(draft.format.encoding)},
                          {"bom", draft.format.hasBom},
                          {"lineEnding", draft.format.lineEnding}});
          area->setPlainText(draft.text);
          area->document()->setModified(true);
          tabs->setTabText(tabs->currentIndex(), tr("Recovered: %1").arg(name));
          tabs->setTabToolTip(tabs->currentIndex(), draft.originalPath);
          trackRecoveryEditor(area);
          if (!m_documentRecovery->adopt(draft, area, error)) {
            tabs->forceCloseTab(tabs->currentIndex());
            return false;
          }
          if (!draft.originalPath.isEmpty())
            applyHighlightForFile(draft.originalPath);
          QTextCursor cursor(area->document());
          cursor.setPosition(
              qBound(0, draft.cursor, area->document()->characterCount() - 1));
          area->setTextCursor(cursor);
          area->ensureCursorVisible();
          area->setFocus();
          saveSettings();
          return true;
        },
        [this](const Recovery::Draft &draft, QString *error) {
          return m_documentRecovery->discard(draft, error);
        });
    connect(&ThemeEngine::instance(), &ThemeEngine::themeChanged, dialog,
            [dialog](const ThemeDefinition &theme) {
              static_cast<StyledDialog *>(dialog)->applyTheme(theme);
            });
  }
  static_cast<StyledDialog *>(m_documentRecoveryDialog.data())
      ->applyTheme(ThemeEngine::instance().activeTheme());
  m_documentRecoveryDialog->refresh();
  m_documentRecoveryDialog->show();
  m_documentRecoveryDialog->raise();
  m_documentRecoveryDialog->activateWindow();
}
