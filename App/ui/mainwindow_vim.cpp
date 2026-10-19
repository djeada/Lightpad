#include "mainwindow_internal.h"

#include <QSaveFile>
#include <QTimer>
#include <QUrl>

// Editor-level commands requested by Vim mode that the main window carries
// out: tabs/buffers, files, hover and per-editor display options.

QString MainWindow::resolveVimPath(const QString &path) const {
  if (path.isEmpty() || QDir::isAbsolutePath(path))
    return path;
  QString base = m_projectRootPath;
  if (base.isEmpty()) {
    if (LightpadTabWidget *tabWidget = currentTabWidget()) {
      const QString current = tabWidget->getFilePath(tabWidget->currentIndex());
      if (!current.isEmpty())
        base = QFileInfo(current).absolutePath();
    }
  }
  if (base.isEmpty())
    base = QDir::currentPath();
  return QDir::cleanPath(QDir(base).filePath(path));
}

void MainWindow::handleVimEditorCommand(TextArea *textArea,
                                        const QString &command) {
  auto later = [this](std::function<void()> action) {
    QTimer::singleShot(0, this, std::move(action));
  };
  auto selectTab = [this](LightpadTabWidget *tabWidget, int index) {
    if (!tabWidget || index < 0 || index >= tabWidget->count() - 1)
      return false;
    if (m_splitEditorContainer)
      m_splitEditorContainer->setCurrentTabWidget(tabWidget);
    tabWidget->setCurrentIndex(index);
    if (TextArea *area = getCurrentTextArea())
      area->setFocus();
    return true;
  };

  if (command == "hover") {
    if (!textArea)
      return;
    const QTextCursor cursor = textArea->textCursor();
    const QRect rect = textArea->cursorRect(cursor);
    const QPoint global = textArea->viewport()->mapToGlobal(rect.bottomLeft());
    if (!requestLspHover(textArea, cursor.blockNumber(),
                         cursor.positionInBlock(), global, textArea->viewport(),
                         rect))
      showVimStatusMessage(tr("No hover information available"));
  } else if (command.startsWith("tab:")) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    const int number = command.mid(4).toInt();
    if (!selectTab(tabWidget, number - 1))
      showVimStatusMessage(QString("E16: Invalid range"));
  } else if (command == "lastTab") {
    LightpadTabWidget *tabWidget = currentTabWidget();
    if (tabWidget)
      selectTab(tabWidget, tabWidget->count() - 2);
  } else if (command == "alternateTab") {
    QWidget *page = m_vimAlternatePage;
    if (!page) {
      showVimStatusMessage("E23: No alternate file");
      return;
    }
    for (LightpadTabWidget *tabWidget : allTabWidgets()) {
      const int index = tabWidget ? tabWidget->indexOf(page) : -1;
      if (index >= 0) {
        selectTab(tabWidget, index);
        return;
      }
    }
    showVimStatusMessage("E23: No alternate file");
  } else if (command == "closeOtherTabs") {
    LightpadTabWidget *tabWidget = currentTabWidget();
    if (!tabWidget)
      return;
    QWidget *keep = tabWidget->currentWidget();
    later([tabWidget = QPointer<LightpadTabWidget>(tabWidget), keep]() {
      if (!tabWidget)
        return;
      for (int i = tabWidget->count() - 2; i >= 0; --i) {
        if (tabWidget->widget(i) != keep && !tabWidget->requestCloseTab(i))
          break;
      }
    });
  } else if (command == "listBuffers" || command.startsWith("bufferName:")) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    if (!tabWidget)
      return;
    const QString wanted = command.section(':', 1);
    QStringList entries;
    QList<int> matches;
    for (int i = 0; i < tabWidget->count() - 1; ++i) {
      const QString path = tabWidget->getFilePath(i);
      const QString name =
          path.isEmpty() ? tabWidget->tabText(i) : QFileInfo(path).fileName();
      LightpadPage *page = tabWidget->getPage(i);
      const bool modified =
          page && page->getTextArea() && page->getTextArea()->changesUnsaved();
      QString flags = i == tabWidget->currentIndex() ? "%a" : "  ";
      if (tabWidget->widget(i) == m_vimAlternatePage)
        flags = "#";
      entries << QString("%1 %2 %3\"%4\"")
                     .arg(i + 1)
                     .arg(flags)
                     .arg(modified ? "+ " : "")
                     .arg(name);
      if (!wanted.isEmpty() && (name.contains(wanted, Qt::CaseInsensitive) ||
                                path.contains(wanted, Qt::CaseInsensitive)))
        matches << i;
    }
    if (wanted.isEmpty()) {
      showVimStatusMessage(entries.join("   "));
    } else if (matches.size() == 1) {
      selectTab(tabWidget, matches.first());
    } else {
      showVimStatusMessage(
          matches.isEmpty()
              ? QString("E94: No matching buffer for %1").arg(wanted)
              : QString("E93: More than one match for %1").arg(wanted));
    }
  } else if (command.startsWith("edit:")) {
    const QString path = resolveVimPath(command.mid(5));
    later([this, path]() {
      const QFileInfo info(path);
      if (info.exists()) {
        openFileAndAddToNewTab(path);
        return;
      }
      if (!info.absoluteDir().exists()) {
        showVimStatusMessage(
            QString("E212: Can't open file for writing: %1").arg(path));
        return;
      }
      // Like Vim, a new file gets a buffer now and is created on :w.
      on_actionNew_File_triggered();
      if (LightpadTabWidget *tabWidget = currentTabWidget()) {
        tabWidget->setFilePath(tabWidget->currentIndex(), path);
        tabWidget->setTabText(tabWidget->currentIndex(), info.fileName());
      }
      showVimStatusMessage(QString("\"%1\" [New]").arg(info.fileName()));
    });
  } else if (command.startsWith("saveAs:")) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    if (tabWidget)
      saveTabAs(tabWidget, tabWidget->currentIndex(),
                resolveVimPath(command.mid(7)));
  } else if (command.startsWith("writeAs:")) {
    LightpadTabWidget *tabWidget = currentTabWidget();
    if (!tabWidget || !textArea)
      return;
    const QString path = resolveVimPath(command.mid(8));
    const QString current = tabWidget->getFilePath(tabWidget->currentIndex());
    if (current.isEmpty()) {
      saveTabAs(tabWidget, tabWidget->currentIndex(), path);
      return;
    }
    if (QDir::cleanPath(current) == QDir::cleanPath(path)) {
      on_actionSave_triggered();
      return;
    }
    // ":w {file}" writes a copy and leaves the buffer's own file alone.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(textArea->toPlainText().toUtf8() + "\n") < 0 ||
        !file.commit()) {
      showVimStatusMessage(
          QString("E212: Can't open file for writing: %1").arg(path));
      return;
    }
    showVimStatusMessage(QString("\"%1\" %2L written")
                             .arg(QFileInfo(path).fileName())
                             .arg(textArea->document()->blockCount()));
  } else if (command.startsWith("openUrl:")) {
    const QString target = command.mid(8);
    const QUrl url =
        QUrl::fromUserInput(target, resolveVimPath("."), QUrl::AssumeLocalFile);
    if (!url.isValid() || !QDesktopServices::openUrl(url))
      showVimStatusMessage(QString("Cannot open %1").arg(target));
  } else if (command.startsWith("option:")) {
    if (!textArea)
      return;
    const QString name = command.section(':', 1, 1);
    const bool on = command.section(':', 2, 2) == "on";
    if (name == "number")
      textArea->showLineNumbers(on);
    else if (name == "wrap")
      textArea->setWordWrapEnabled(on);
    else if (name == "list")
      textArea->setShowWhitespace(on);
    else if (name == "cursorline")
      textArea->highlihtCurrentLine(on);
  }
}
