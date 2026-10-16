#include "../../core/lightpadpage.h"
#include "../../core/lightpadtabwidget.h"
#include "../../core/textarea.h"
#include "../../editor/vimmode.h"
#include "../mainwindow.h"
#include "findreplacepanel.h"
#include "spliteditorcontainer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTimer>
#include <algorithm>

// Keep application integration separate from the panel: the actual widget is
// also exercised with ordinary QPlainTextEdit documents in its tests.
void FindReplacePanel::setTextArea(TextArea *area) {
  QString path;
  if (area) {
    for (QObject *parent = area->parent(); parent; parent = parent->parent()) {
      if (auto *page = qobject_cast<LightpadPage *>(parent)) {
        path = page->getFilePath();
        break;
      }
    }
  }
  setEditor(area, path);
}

void FindReplacePanel::setMainWindow(MainWindow *window) {
  if (m_window == window)
    return;
  m_window = window;
  setDocumentProvider([this]() {
    QVector<OpenDocument> result;
    if (!m_window)
      return result;
    for (auto *tabs : m_window->allTabWidgets()) {
      if (!tabs)
        continue;
      for (int i = 0; i < tabs->count(); ++i) {
        if (tabs->isViewerTab(i))
          continue;
        auto *page = tabs->getPage(i);
        auto *area = page ? page->getTextArea() : nullptr;
        if (!area)
          continue;
        const QString path = tabs->getFilePath(i);
        result.append(
            {{},
             path,
             path.isEmpty() ? tabs->tabText(i) : QFileInfo(path).fileName(),
             area});
      }
    }
    return result;
  });
  m_activateEditor = [this](QPlainTextEdit *editor) {
    if (!m_window)
      return;
    for (auto *tabs : m_window->allTabWidgets()) {
      for (int i = 0; i < tabs->count(); ++i) {
        auto *page = tabs->getPage(i);
        if (page && page->getTextArea() == editor) {
          if (m_window->m_splitEditorContainer)
            m_window->m_splitEditorContainer->setCurrentTabWidget(tabs);
          tabs->setCurrentIndex(i);
          return;
        }
      }
    }
  };
  m_focusEditor = [this]() {
    if (m_editor)
      m_editor->setFocus();
  };
  m_highlight = [this](const QRegularExpression &pattern) {
    if (!m_window)
      return;
    for (const auto &doc : documents()) {
      auto *area = qobject_cast<TextArea *>(doc.editor.data());
      if (!area)
        continue;
      const bool active =
          isVisible() && !m_vim && scope() != SearchReplace::Scope::Selection &&
          (scope() != SearchReplace::Scope::CurrentFile || area == m_editor) &&
          (scope() != SearchReplace::Scope::Project ||
           std::any_of(m_results.files.cbegin(), m_results.files.cend(),
                       [&doc](const SearchReplace::FileResult &file) {
                         return file.source.key == doc.key;
                       }));
      area->setSearchPattern(active ? pattern : QRegularExpression());
      area->updateSyntaxHighlightTags(
          active && !pattern.pattern().isEmpty() ? m_find->text() : QString());
    }
  };
  m_vimKey = [this](QKeyEvent *event) {
    auto *area = qobject_cast<TextArea *>(m_editor.data());
    if (!area || !area->vimMode() || !area->isVimModeEnabled())
      return;
    auto *vim = area->vimMode();
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        vim->mode() == VimEditMode::Command &&
        vim->commandText() != m_find->text())
      vim->setCommandText(m_find->text());
    vim->processKeyEvent(event);
  };
  m_diskGuard = [this](const QStringList &paths, bool begin) {
    if (!m_window)
      return;
    for (const auto &path : paths) {
      const QString clean = QDir::cleanPath(path);
      if (begin)
        m_window->m_searchReplaceWrites.insert(clean);
      else {
        bool consistent = true;
        for (const auto &doc : documents()) {
          if (!doc.editor || doc.key != SearchReplace::fileKey(clean))
            continue;
          QFile file(clean);
          if (!file.open(QIODevice::ReadOnly)) {
            consistent = false;
            continue;
          }
          QString content =
              SearchReplace::normalizedText(QString::fromUtf8(file.readAll()));
          if (content.startsWith(QChar(0xfeff)))
            content.remove(0, 1);
          if (content != doc.editor->toPlainText())
            consistent = false;
          else if (!doc.editor->document()->isModified()) {
            if (auto *area = qobject_cast<TextArea *>(doc.editor.data()))
              area->removeIconUnsaved();
          }
        }
        if (consistent)
          m_window->recordFileTimestamp(clean);
        if (m_window->isFileOpenInEditor(clean))
          m_window->watchOpenFile(clean);
        m_window->m_searchReplaceWrites.remove(clean);
      }
    }
    if (!begin)
      QTimer::singleShot(0, m_window, [window = m_window]() {
        if (window)
          window->recheckOpenFilesForExternalChanges();
      });
  };
}
