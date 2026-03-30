#include "markdownpreviewpanel.h"
#include "markdowntools.h"
#include "../core/logging/logger.h"
#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QScrollBar>
#include <QSplitter>

MarkdownPreviewPanel::MarkdownPreviewPanel(QWidget *parent)
    : QWidget(parent), m_wordCountLabel(nullptr), m_outlineTree(nullptr),
      m_outlinePanel(nullptr), m_syncScrollEnabled(false), m_zoomLevel(100) {
  setupUi();

  m_updateTimer.setSingleShot(true);
  m_updateTimer.setInterval(300);
  connect(&m_updateTimer, &QTimer::timeout, this,
          &MarkdownPreviewPanel::updatePreview);
}

void MarkdownPreviewPanel::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  setupToolbar();
  layout->addWidget(m_toolbar);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setObjectName("markdownPreviewSplitter");

  setupOutlinePanel();
  splitter->addWidget(m_outlinePanel);

  m_browser = new QTextBrowser(this);
  m_browser->setObjectName("markdownPreviewBrowser");
  m_browser->setOpenLinks(false);
  m_browser->setOpenExternalLinks(false);
  m_browser->setReadOnly(true);

  connect(m_browser, &QTextBrowser::anchorClicked, this,
          &MarkdownPreviewPanel::onAnchorClicked);

  splitter->addWidget(m_browser);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({180, 600});

  layout->addWidget(splitter, 1);

  m_wordCountLabel = new QLabel(this);
  m_wordCountLabel->setObjectName("markdownWordCountLabel");
  m_wordCountLabel->setStyleSheet(
      "QLabel { padding: 2px 8px; color: #8b949e; font-size: 11px; }");
  layout->addWidget(m_wordCountLabel);

  setLayout(layout);
}

void MarkdownPreviewPanel::setupToolbar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setObjectName("markdownPreviewToolbar");
  m_toolbar->setMovable(false);

  QAction *refreshAction = m_toolbar->addAction("⟳ Refresh");
  refreshAction->setToolTip("Refresh Preview");
  connect(refreshAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::updatePreview);

  m_toolbar->addSeparator();

  QAction *zoomInAction = m_toolbar->addAction("🔍+ Zoom In");
  zoomInAction->setToolTip("Zoom In (Ctrl+=)");
  zoomInAction->setShortcut(QKeySequence("Ctrl+="));
  connect(zoomInAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::onZoomIn);

  QAction *zoomOutAction = m_toolbar->addAction("🔍- Zoom Out");
  zoomOutAction->setToolTip("Zoom Out (Ctrl+-)");
  zoomOutAction->setShortcut(QKeySequence("Ctrl+-"));
  connect(zoomOutAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::onZoomOut);

  QAction *zoomResetAction = m_toolbar->addAction("1:1 Reset");
  zoomResetAction->setToolTip("Reset Zoom (Ctrl+0)");
  zoomResetAction->setShortcut(QKeySequence("Ctrl+0"));
  connect(zoomResetAction, &QAction::triggered, this,
          &MarkdownPreviewPanel::onZoomReset);

  m_toolbar->addSeparator();

  QAction *exportHtmlAction = m_toolbar->addAction("📄 Export HTML");
  exportHtmlAction->setToolTip("Export to HTML file");
  connect(exportHtmlAction, &QAction::triggered, this, [this]() {
    QString html = exportToHtml();
    if (html.isEmpty())
      return;

    QString suggestedName;
    if (!m_filePath.isEmpty()) {
      QFileInfo fi(m_filePath);
      suggestedName = fi.completeBaseName() + ".html";
    } else {
      suggestedName = "preview.html";
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Export HTML"),
                                                suggestedName,
                                                tr("HTML Files (*.html)"));
    if (path.isEmpty())
      return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      file.write(html.toUtf8());
      file.close();
    }
  });

  QAction *syncScrollAction = m_toolbar->addAction("⇅ Sync Scroll");
  syncScrollAction->setToolTip("Toggle synchronized scrolling");
  syncScrollAction->setCheckable(true);
  syncScrollAction->setChecked(m_syncScrollEnabled);
  connect(syncScrollAction, &QAction::toggled, this,
          &MarkdownPreviewPanel::setSyncScrollEnabled);
}

void MarkdownPreviewPanel::setupOutlinePanel() {
  m_outlinePanel = new QWidget(this);
  m_outlinePanel->setObjectName("markdownOutlinePanel");
  auto *outlineLayout = new QVBoxLayout(m_outlinePanel);
  outlineLayout->setContentsMargins(0, 0, 0, 0);
  outlineLayout->setSpacing(0);

  auto *outlineHeader = new QLabel("Outline", m_outlinePanel);
  outlineHeader->setObjectName("markdownOutlineHeader");
  outlineHeader->setStyleSheet(
      "QLabel { padding: 4px 8px; font-weight: bold; color: #e6edf3; "
      "background: #161b22; border-bottom: 1px solid #30363d; }");
  outlineLayout->addWidget(outlineHeader);

  m_outlineTree = new QTreeWidget(m_outlinePanel);
  m_outlineTree->setObjectName("markdownOutlineTree");
  m_outlineTree->setHeaderHidden(true);
  m_outlineTree->setRootIsDecorated(true);
  m_outlineTree->setIndentation(12);
  m_outlineTree->setStyleSheet(
      "QTreeWidget { background: #0d1117; color: #c9d1d9; border: none; }"
      "QTreeWidget::item:hover { background: #161b22; }"
      "QTreeWidget::item:selected { background: #1f6feb; }");

  connect(m_outlineTree, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem *item) {
            int lineNumber = item->data(0, Qt::UserRole).toInt();
            QString anchor = item->data(0, Qt::UserRole + 1).toString();
            if (!anchor.isEmpty()) {
              m_browser->scrollToAnchor(anchor);
            }
            emit outlineEntryClicked(lineNumber);
          });

  outlineLayout->addWidget(m_outlineTree, 1);
  m_outlinePanel->setLayout(outlineLayout);
}

void MarkdownPreviewPanel::setMarkdown(const QString &markdown) {
  m_markdown = markdown;
  m_updateTimer.start();
}

void MarkdownPreviewPanel::setBasePath(const QString &basePath) {
  m_basePath = basePath;
}

void MarkdownPreviewPanel::setFilePath(const QString &filePath) {
  m_filePath = filePath;
  QFileInfo fi(filePath);
  m_basePath = fi.absolutePath();
}

bool MarkdownPreviewPanel::isMarkdownFile(const QString &extension) {
  static const QStringList markdownExtensions = {"md",   "markdown", "mdown",
                                                  "mkd",  "mkdn",    "mdx"};
  return markdownExtensions.contains(extension.toLower());
}

void MarkdownPreviewPanel::updatePreview() {
  if (m_markdown.isEmpty()) {
    m_browser->clear();
    updateWordCountLabel();
    updateOutline();
    return;
  }

  int scrollPos = m_browser->verticalScrollBar()->value();

  QString html = MarkdownTools::toHtml(m_markdown, m_basePath);

  if (m_zoomLevel != 100) {
    QString zoomStyle =
        QString("<style>body { zoom: %1%; }</style>").arg(m_zoomLevel);
    html.replace("</head>", zoomStyle + "</head>");
  }

  if (!m_basePath.isEmpty()) {
    m_browser->setSearchPaths({m_basePath});
  }

  m_browser->setHtml(html);

  m_browser->verticalScrollBar()->setValue(scrollPos);

  updateWordCountLabel();
  updateOutline();
}

void MarkdownPreviewPanel::onAnchorClicked(const QUrl &url) {
  QString urlStr = url.toString();

  if (url.scheme() == "file") {
    QString localPath = url.toLocalFile();
    if (!localPath.isEmpty()) {
      emit linkClicked(localPath);
      return;
    }
  }

  if (urlStr.startsWith('#')) {
    m_browser->scrollToAnchor(urlStr.mid(1));
    return;
  }

  if (url.scheme() == "http" || url.scheme() == "https") {
    QDesktopServices::openUrl(url);
    return;
  }

  if (!m_basePath.isEmpty()) {
    QString resolved = QDir(m_basePath).absoluteFilePath(urlStr);
    if (QFileInfo::exists(resolved)) {
      emit linkClicked(resolved);
      return;
    }
  }

  emit linkClicked(urlStr);
}

void MarkdownPreviewPanel::updateWordCountLabel() {
  if (!m_wordCountLabel)
    return;

  if (m_markdown.isEmpty()) {
    m_wordCountLabel->setText("");
    return;
  }

  int words = MarkdownTools::wordCount(m_markdown);
  int readingTime = MarkdownTools::readingTimeMinutes(m_markdown);
  int completed = 0;
  int tasks = MarkdownTools::countTasks(m_markdown, &completed);

  QString text = QString("%1 words · %2 min read").arg(words).arg(readingTime);
  if (tasks > 0) {
    text += QString(" · %1/%2 tasks").arg(completed).arg(tasks);
  }
  m_wordCountLabel->setText(text);
}

void MarkdownPreviewPanel::updateOutline() {
  if (!m_outlineTree)
    return;

  m_outlineTree->clear();

  QList<MarkdownHeading> headings = MarkdownTools::extractHeadings(m_markdown);

  QList<QTreeWidgetItem *> stack;
  QList<int> levelStack;

  for (const MarkdownHeading &h : headings) {
    auto *item = new QTreeWidgetItem();
    item->setText(0, h.text);
    item->setData(0, Qt::UserRole, h.lineNumber);
    item->setData(0, Qt::UserRole + 1, h.anchor);
    item->setToolTip(0, QString("Line %1").arg(h.lineNumber + 1));

    while (!levelStack.isEmpty() && levelStack.last() >= h.level) {
      stack.removeLast();
      levelStack.removeLast();
    }

    if (stack.isEmpty()) {
      m_outlineTree->addTopLevelItem(item);
    } else {
      stack.last()->addChild(item);
    }

    stack.append(item);
    levelStack.append(h.level);
  }

  m_outlineTree->expandAll();
}

void MarkdownPreviewPanel::setSyncScrollEnabled(bool enabled) {
  m_syncScrollEnabled = enabled;
}

void MarkdownPreviewPanel::setSourceScrollRatio(double ratio) {
  if (!m_syncScrollEnabled || !m_browser)
    return;

  QScrollBar *scrollBar = m_browser->verticalScrollBar();
  if (scrollBar) {
    int maxScroll = scrollBar->maximum();
    scrollBar->setValue(static_cast<int>(ratio * maxScroll));
  }
}

QString MarkdownPreviewPanel::exportToHtml() const {
  if (m_markdown.isEmpty())
    return QString();
  return MarkdownTools::toHtml(m_markdown, m_basePath);
}

void MarkdownPreviewPanel::onZoomIn() {
  m_zoomLevel = qMin(m_zoomLevel + 10, 300);
  updatePreview();
}

void MarkdownPreviewPanel::onZoomOut() {
  m_zoomLevel = qMax(m_zoomLevel - 10, 50);
  updatePreview();
}

void MarkdownPreviewPanel::onZoomReset() {
  m_zoomLevel = 100;
  updatePreview();
}
