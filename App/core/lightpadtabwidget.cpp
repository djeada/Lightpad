#include "lightpadtabwidget.h"
#include "../ui/mainwindow.h"
#include "lightpadpage.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMenu>
#include <QProcess>
#include <QSizePolicy>
#include <QStyle>
#include <QTabBar>
#include <QTextEdit>
#include <QUrl>

LightpadTabBar::LightpadTabBar(QWidget *parent) : QTabBar(parent) {}

void LightpadTabBar::contextMenuEvent(QContextMenuEvent *event) {
  int index = tabAt(event->pos());

  if (index < 0 || index == count() - 1) {
    return;
  }

  QMenu menu(this);

  QAction *closeAction = menu.addAction(tr("Close Tab"));
  QAction *closeOthersAction = menu.addAction(tr("Close Other Tabs"));
  QAction *closeToRightAction = menu.addAction(tr("Close Tabs to the Right"));
  menu.addSeparator();
  QAction *closeAllAction = menu.addAction(tr("Close All Tabs"));

  menu.addSeparator();
  QAction *copyAbsolutePathAction = menu.addAction(tr("Copy Absolute Path"));
  QAction *copyRelativePathAction = menu.addAction(tr("Copy Relative Path"));
  QAction *copyFileNameAction = menu.addAction(tr("Copy File Name"));
  menu.addSeparator();
  QAction *revealInExplorerAction =
      menu.addAction(tr("Reveal in File Explorer"));

  if (index >= count() - 2) {
    closeToRightAction->setEnabled(false);
  }

  if (count() <= 2) {
    closeOthersAction->setEnabled(false);
  }

  QAction *selectedAction = menu.exec(event->globalPos());

  if (selectedAction == closeAction) {
    emit closeTab(index);
  } else if (selectedAction == closeOthersAction) {
    emit closeOtherTabs(index);
  } else if (selectedAction == closeToRightAction) {
    emit closeTabsToTheRight(index);
  } else if (selectedAction == closeAllAction) {
    emit closeAllTabs();
  } else if (selectedAction == copyAbsolutePathAction) {
    emit copyAbsolutePath(index);
  } else if (selectedAction == copyRelativePathAction) {
    emit copyRelativePath(index);
  } else if (selectedAction == copyFileNameAction) {
    emit copyFileName(index);
  } else if (selectedAction == revealInExplorerAction) {
    emit revealInFileExplorer(index);
  }
}

LightpadTabWidget::LightpadTabWidget(QWidget *parent) : QTabWidget(parent) {
  setupTabBar();

  QWidget::connect(tabBar(), &QTabBar::tabCloseRequested, this,
                   [this](int index) { removeTab(index); });

  setTabsClosable(true);
  setMovable(true);

  newTabButton = new QToolButton(this);
  newTabButton->setObjectName("AddTabButton");
  newTabButton->setIcon(QIcon(":/resources/icons/add_dark.png"));
  newTabButton->setIconSize(QSize(25, 25));
  newTabButton->setFixedSize(newTabButton->iconSize());

  addTab(new QWidget(), QString());
  setTabEnabled(0, false);
  tabBar()->setTabButton(0, QTabBar::RightSide, newTabButton);

  QWidget::connect(newTabButton, &QToolButton::clicked, this,
                   [this] { addNewTab(); });

  QWidget::connect(tabBar(), &QTabBar::tabMoved, this,
                   [this](int from, int to) {
                     if (from == count() - 1)
                       tabBar()->moveTab(to, from);
                   });

  QWidget::connect(tabBar(), &QTabBar::currentChanged, this, [this](int index) {
    if (index == count() - 1)
      setCurrentIndex(0);
  });

  updateCloseButtons();
}

void LightpadTabWidget::resizeEvent(QResizeEvent *event) {

  QTabWidget::resizeEvent(event);
}

void LightpadTabWidget::tabRemoved(int index) {
  Q_UNUSED(index);

  if (count() <= 1)
    addNewTab();

  updateCloseButtons();
}

void LightpadTabWidget::tabInserted(int index) {
  QTabWidget::tabInserted(index);
  updateCloseButtons();
}

void LightpadTabWidget::updateCloseButtons() {
  for (int i = 0; i < count(); ++i) {
    if (i == count() - 1) {
      tabBar()->setTabButton(i, QTabBar::RightSide, newTabButton);
      continue;
    }

    QWidget *existingButton = tabBar()->tabButton(i, QTabBar::RightSide);
    if (existingButton && existingButton != newTabButton) {

      existingButton->setStyleSheet(
          QString("QToolButton {"
                  "  color: rgba(255, 255, 255, 0.4);"
                  "  background: transparent;"
                  "  border: none;"
                  "  border-radius: 4px;"
                  "  padding: 2px;"
                  "  font-size: 14px;"
                  "  font-weight: bold;"
                  "}"
                  "QToolButton:hover {"
                  "  color: %1;"
                  "  background: rgba(255, 255, 255, 0.15);"
                  "}"
                  "QToolButton:pressed {"
                  "  color: #ffffff;"
                  "  background: #e81123;"
                  "}")
              .arg(m_foregroundColor));
      continue;
    }

    QToolButton *closeButton = new QToolButton(tabBar());
    closeButton->setObjectName("TabCloseButton");
    closeButton->setText(QStringLiteral("\u00D7"));
    closeButton->setFixedSize(QSize(18, 18));
    closeButton->setAutoRaise(true);
    closeButton->setCursor(Qt::ArrowCursor);
    closeButton->setToolTip(tr("Close Tab"));
    closeButton->setStyleSheet(
        QString("QToolButton {"
                "  color: rgba(255, 255, 255, 0.4);"
                "  background: transparent;"
                "  border: none;"
                "  border-radius: 4px;"
                "  padding: 2px;"
                "  font-size: 14px;"
                "  font-weight: bold;"
                "}"
                "QToolButton:hover {"
                "  color: %1;"
                "  background: rgba(255, 255, 255, 0.15);"
                "}"
                "QToolButton:pressed {"
                "  color: #ffffff;"
                "  background: #e81123;"
                "}")
            .arg(m_foregroundColor));
    connect(closeButton, &QToolButton::clicked, this, [this, closeButton]() {
      for (int index = 0; index < count(); ++index) {
        if (tabBar()->tabButton(index, QTabBar::RightSide) == closeButton) {
          removeTab(index);
          break;
        }
      }
    });
    tabBar()->setTabButton(i, QTabBar::RightSide, closeButton);
  }
}

void LightpadTabWidget::addNewTab() {
  if (mainWindow) {
    LightpadPage *newPage = new LightpadPage(this);
    newPage->setMainWindow(mainWindow);
    if (mainWindow->getGitIntegration()) {
      newPage->setGitIntegration(mainWindow->getGitIntegration());
    }

    QString projectRoot = mainWindow->getProjectRootPath();
    if (!projectRoot.isEmpty()) {
      newPage->setProjectRootPath(projectRoot);
      newPage->setTreeViewVisible(true);
      newPage->setModelRootIndex(projectRoot);
    }

    insertTab(count() - 1, newPage, unsavedDocumentLabel);
    setCurrentIndex(count() - 2);
  }
}

void LightpadTabWidget::setMainWindow(MainWindow *window) {
  mainWindow = window;
  QList<LightpadPage *> pages = findChildren<LightpadPage *>();

  for (auto &page : pages) {
    page->setMainWindow(window);
    if (window && window->getGitIntegration()) {
      page->setGitIntegration(window->getGitIntegration());
    }
  }

  if (count() <= 1)
    addNewTab();
}

void LightpadTabWidget::setTheme(const QString &backgroundColor,
                                 const QString &foregroundColor,
                                 const QString &surfaceColor,
                                 const QString &hoverColor,
                                 const QString &accentColor,
                                 const QString &borderColor) {
  m_foregroundColor = foregroundColor;
  m_hoverColor = hoverColor;
  m_accentColor = accentColor;

  setStyleSheet(

      "QScrollBar:vertical { background: transparent; }"
      "QScrollBar:horizontal { background: transparent; }"

      "QTabBar { "
      "background: " +
      backgroundColor +
      "; "
      "qproperty-drawBase: 0; "
      "}"

      "QTabBar::tab { "
      "color: #8b949e; "
      "background-color: " +
      backgroundColor +
      "; "
      "padding: 10px 18px; "
      "margin: 4px 2px 0px 2px; "
      "border-top-left-radius: 8px; "
      "border-top-right-radius: 8px; "
      "border: 1px solid transparent; "
      "border-bottom: none; "
      "font-size: 13px; "
      "}"

      "QTabBar::tab:selected { "
      "color: " +
      foregroundColor +
      "; "
      "background-color: " +
      surfaceColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "border-bottom: 2px solid " +
      accentColor +
      "; "
      "}"

      "QTabBar::tab:hover:!selected { "
      "color: " +
      foregroundColor +
      "; "
      "background-color: " +
      hoverColor +
      "; "
      "}"

      "QToolButton#AddTabButton { "
      "background: " +
      backgroundColor +
      "; "
      "border-radius: 6px; "
      "padding: 4px; "
      "border: 1px solid transparent; "
      "}"
      "QToolButton#AddTabButton:hover { "
      "background: " +
      hoverColor +
      "; "
      "border: 1px solid " +
      borderColor +
      "; "
      "}"

      "QToolButton#TabCloseButton { "
      "color: rgba(255, 255, 255, 0.4); "
      "background: transparent; "
      "border: none; "
      "border-radius: 4px; "
      "padding: 2px; "
      "font-size: 14px; "
      "font-weight: bold; "
      "}"
      "QToolButton#TabCloseButton:hover { "
      "color: " +
      foregroundColor +
      "; "
      "background: rgba(255, 255, 255, 0.15); "
      "}"
      "QToolButton#TabCloseButton:pressed { "
      "color: #ffffff; "
      "background: #e81123; "
      "}"

      "QTabWidget::pane { "
      "border: none; "
      "background-color: " +
      backgroundColor +
      "; "
      "}"
      "QTabWidget#tabWidget { "
      "background-color: " +
      backgroundColor +
      "; "
      "}");

  updateCloseButtons();
}

void LightpadTabWidget::setFilePath(int index, QString filePath) {
  if (index >= 0 && index < count()) {
    LightpadPage *page = qobject_cast<LightpadPage *>(widget(index));

    if (page)
      page->setFilePath(filePath);
  }
}

void LightpadTabWidget::closeAllTabs() {
  if (count() == 1)
    return;

  for (int i = count() - 2; i >= 0; i--)
    removeTab(i);
}

void LightpadTabWidget::closeCurrentTab() {
  if (count() == 1)
    return;

  removeTab(currentIndex());
}

LightpadPage *LightpadTabWidget::getPage(int index) {
  if (index < 0 || index >= count())
    return nullptr;

  return qobject_cast<LightpadPage *>(widget(index));
}

LightpadPage *LightpadTabWidget::getCurrentPage() {
  LightpadPage *page = nullptr;

  if (qobject_cast<LightpadPage *>(currentWidget()) != 0)
    page = qobject_cast<LightpadPage *>(currentWidget());

  else if (findChild<LightpadPage *>("widget"))
    page = findChild<LightpadPage *>("widget");

  return page;
}

QString LightpadTabWidget::getFilePath(int index) {
  if (index >= 0 && index < count()) {
    LightpadPage *page = qobject_cast<LightpadPage *>(widget(index));

    if (page)
      return page->getFilePath();

    QWidget *w = widget(index);
    if (m_viewerFilePaths.contains(w))
      return m_viewerFilePaths[w];
  }

  return "";
}

void LightpadTabWidget::addViewerTab(QWidget *viewer, const QString &filePath) {
  addViewerTab(viewer, filePath, QString());
}

void LightpadTabWidget::addViewerTab(QWidget *viewer, const QString &filePath,
                                     const QString &projectRootPath) {
  if (!viewer)
    return;

  auto *page = new LightpadPage(this, false);
  page->setMainWindow(mainWindow);
  if (!projectRootPath.isEmpty()) {
    page->setProjectRootPath(projectRootPath);
    page->setTreeViewVisible(true);
    page->setModelRootIndex(projectRootPath);
  }
  page->setCustomContentWidget(viewer);
  page->setFilePath(filePath);

  QFileInfo fileInfo(filePath);
  QString tabTitle = fileInfo.fileName();

  m_viewerFilePaths[page] = filePath;
  insertTab(count() - 1, page, tabTitle);
  setCurrentIndex(count() - 2);
}

bool LightpadTabWidget::isViewerTab(int index) const {
  if (index < 0 || index >= count())
    return false;

  QWidget *w = widget(index);
  return m_viewerFilePaths.contains(w);
}

void LightpadTabWidget::setupTabBar() {

  LightpadTabBar *customTabBar = new LightpadTabBar(this);
  setTabBar(customTabBar);

  connect(customTabBar, &LightpadTabBar::closeTab, this,
          &LightpadTabWidget::onCloseTab);
  connect(customTabBar, &LightpadTabBar::closeOtherTabs, this,
          &LightpadTabWidget::onCloseOtherTabs);
  connect(customTabBar, &LightpadTabBar::closeTabsToTheRight, this,
          &LightpadTabWidget::onCloseTabsToTheRight);
  connect(customTabBar, &LightpadTabBar::closeAllTabs, this,
          &LightpadTabWidget::onCloseAllTabs);
  connect(customTabBar, &LightpadTabBar::copyAbsolutePath, this,
          &LightpadTabWidget::onCopyAbsolutePath);
  connect(customTabBar, &LightpadTabBar::copyRelativePath, this,
          &LightpadTabWidget::onCopyRelativePath);
  connect(customTabBar, &LightpadTabBar::copyFileName, this,
          &LightpadTabWidget::onCopyFileName);
  connect(customTabBar, &LightpadTabBar::revealInFileExplorer, this,
          &LightpadTabWidget::onRevealInFileExplorer);
}

void LightpadTabWidget::onCloseTab(int index) {
  if (index >= 0 && index < count() - 1) {
    removeTab(index);
  }
}

void LightpadTabWidget::onCloseOtherTabs(int index) {
  if (index < 0 || index >= count() - 1) {
    return;
  }

  for (int i = count() - 2; i > index; --i) {
    removeTab(i);
  }

  for (int i = index - 1; i >= 0; --i) {
    removeTab(i);
  }
}

void LightpadTabWidget::onCloseTabsToTheRight(int index) {
  if (index < 0 || index >= count() - 1) {
    return;
  }

  for (int i = count() - 2; i > index; --i) {
    removeTab(i);
  }
}

void LightpadTabWidget::onCloseAllTabs() { closeAllTabs(); }

void LightpadTabWidget::onCopyAbsolutePath(int index) {
  QString filePath = getFilePath(index);
  if (!filePath.isEmpty()) {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(filePath);
  }
}

void LightpadTabWidget::onCopyRelativePath(int index) {
  if (!mainWindow) {
    return;
  }

  QString filePath = getFilePath(index);
  if (filePath.isEmpty()) {
    return;
  }

  QString projectRoot = mainWindow->getProjectRootPath();
  QClipboard *clipboard = QApplication::clipboard();

  if (!projectRoot.isEmpty()) {
    QDir projectDir(projectRoot);
    QString relativePath = projectDir.relativeFilePath(filePath);
    clipboard->setText(relativePath);
  } else {

    clipboard->setText(filePath);
  }
}

void LightpadTabWidget::onCopyFileName(int index) {
  QString filePath = getFilePath(index);
  if (!filePath.isEmpty()) {
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(fileName);
  }
}

void LightpadTabWidget::onRevealInFileExplorer(int index) {
  QString filePath = getFilePath(index);
  if (!filePath.isEmpty()) {
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {

      QString dirPath = fileInfo.absolutePath();

#ifdef Q_OS_WIN

      QProcess::startDetached(
          "explorer", QStringList()
                          << "/select," + QDir::toNativeSeparators(filePath));
#elif defined(Q_OS_MAC)

      QProcess::startDetached("open", QStringList() << "-R" << filePath);
#else

      QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
#endif
    }
  }
}
