#include "markdown/markdownpreviewpanel.h"
#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QToolBar>
#include <QTreeWidget>

class TestMarkdownPreviewPanel : public QObject {
  Q_OBJECT

private slots:
  void testIsMarkdownFile();
  void testSetMarkdown();
  void testSetFilePath();
  void testUpdatePreview();
  void testLinkClickedSignal();
  void testWidgetStructure();

  // New tests for expanded features
  void testWordCountLabel();
  void testOutlinePanel();
  void testOutlinePanelUpdates();
  void testOutlineEntryClickedSignal();
  void testExportToHtml();
  void testSyncScrollEnabled();
  void testZoomControls();
};

void TestMarkdownPreviewPanel::testIsMarkdownFile() {
  QVERIFY(MarkdownPreviewPanel::isMarkdownFile("md"));
  QVERIFY(MarkdownPreviewPanel::isMarkdownFile("markdown"));
  QVERIFY(MarkdownPreviewPanel::isMarkdownFile("mdown"));
  QVERIFY(MarkdownPreviewPanel::isMarkdownFile("mkd"));
  QVERIFY(MarkdownPreviewPanel::isMarkdownFile("mkdn"));
  QVERIFY(MarkdownPreviewPanel::isMarkdownFile("mdx"));
  QVERIFY(MarkdownPreviewPanel::isMarkdownFile("MD"));

  QVERIFY(!MarkdownPreviewPanel::isMarkdownFile("txt"));
  QVERIFY(!MarkdownPreviewPanel::isMarkdownFile("html"));
  QVERIFY(!MarkdownPreviewPanel::isMarkdownFile("cpp"));
}

void TestMarkdownPreviewPanel::testSetMarkdown() {
  MarkdownPreviewPanel panel;
  panel.setMarkdown("# Hello World");

  QTest::qWait(500);

  auto *browser = panel.findChild<QTextBrowser *>("markdownPreviewBrowser");
  QVERIFY(browser);
  QVERIFY(!browser->toPlainText().isEmpty());
}

void TestMarkdownPreviewPanel::testSetFilePath() {
  MarkdownPreviewPanel panel;
  panel.setFilePath("/tmp/test/docs/readme.md");

  QCOMPARE(panel.filePath(), QString("/tmp/test/docs/readme.md"));
  QCOMPARE(panel.basePath(), QString("/tmp/test/docs"));
}

void TestMarkdownPreviewPanel::testUpdatePreview() {
  MarkdownPreviewPanel panel;
  panel.setMarkdown("**Bold text** and *italic*");
  panel.updatePreview();

  auto *browser = panel.findChild<QTextBrowser *>("markdownPreviewBrowser");
  QVERIFY(browser);
  QString html = browser->toHtml();
  QVERIFY(!html.isEmpty());
}

void TestMarkdownPreviewPanel::testLinkClickedSignal() {
  MarkdownPreviewPanel panel;
  QSignalSpy spy(&panel, &MarkdownPreviewPanel::linkClicked);
  QVERIFY(spy.isValid());
}

void TestMarkdownPreviewPanel::testWidgetStructure() {
  MarkdownPreviewPanel panel;

  auto *browser = panel.findChild<QTextBrowser *>("markdownPreviewBrowser");
  QVERIFY(browser);
  QVERIFY(browser->isReadOnly());

  auto *toolbar = panel.findChild<QToolBar *>("markdownPreviewToolbar");
  QVERIFY(toolbar);
  QVERIFY(!toolbar->actions().isEmpty());
}

// ── New expanded feature tests ──────────────────────────────────────

void TestMarkdownPreviewPanel::testWordCountLabel() {
  MarkdownPreviewPanel panel;

  auto *label = panel.findChild<QLabel *>("markdownWordCountLabel");
  QVERIFY(label);

  panel.setMarkdown("# Title\n\nThis is a test paragraph with several words.");
  panel.updatePreview();

  QVERIFY(!label->text().isEmpty());
  QVERIFY(label->text().contains("words"));
  QVERIFY(label->text().contains("min read"));
}

void TestMarkdownPreviewPanel::testOutlinePanel() {
  MarkdownPreviewPanel panel;

  auto *outlinePanel = panel.findChild<QWidget *>("markdownOutlinePanel");
  QVERIFY(outlinePanel);

  auto *outlineTree = panel.findChild<QTreeWidget *>("markdownOutlineTree");
  QVERIFY(outlineTree);
  QVERIFY(outlineTree->isHeaderHidden());
}

void TestMarkdownPreviewPanel::testOutlinePanelUpdates() {
  MarkdownPreviewPanel panel;
  panel.setMarkdown("# Title\n\n## Section 1\n\n## Section 2\n\n### Sub");
  panel.updatePreview();

  auto *outlineTree = panel.findChild<QTreeWidget *>("markdownOutlineTree");
  QVERIFY(outlineTree);
  QVERIFY(outlineTree->topLevelItemCount() > 0);
}

void TestMarkdownPreviewPanel::testOutlineEntryClickedSignal() {
  MarkdownPreviewPanel panel;
  QSignalSpy spy(&panel, &MarkdownPreviewPanel::outlineEntryClicked);
  QVERIFY(spy.isValid());
}

void TestMarkdownPreviewPanel::testExportToHtml() {
  MarkdownPreviewPanel panel;

  // Empty markdown should return empty
  QVERIFY(panel.exportToHtml().isEmpty());

  panel.setMarkdown("# Hello\n\nWorld");
  QString html = panel.exportToHtml();

  QVERIFY(!html.isEmpty());
  QVERIFY(html.contains("<h1"));
  QVERIFY(html.contains("Hello"));
  QVERIFY(html.contains("World"));
}

void TestMarkdownPreviewPanel::testSyncScrollEnabled() {
  MarkdownPreviewPanel panel;

  QVERIFY(!panel.isSyncScrollEnabled());

  panel.setSyncScrollEnabled(true);
  QVERIFY(panel.isSyncScrollEnabled());

  panel.setSyncScrollEnabled(false);
  QVERIFY(!panel.isSyncScrollEnabled());
}

void TestMarkdownPreviewPanel::testZoomControls() {
  MarkdownPreviewPanel panel;
  panel.setMarkdown("# Test content");
  panel.updatePreview();

  auto *toolbar = panel.findChild<QToolBar *>("markdownPreviewToolbar");
  QVERIFY(toolbar);

  // Verify zoom actions exist in toolbar
  bool hasZoomIn = false, hasZoomOut = false, hasZoomReset = false;
  for (QAction *action : toolbar->actions()) {
    if (action->text().contains("Zoom In"))
      hasZoomIn = true;
    if (action->text().contains("Zoom Out"))
      hasZoomOut = true;
    if (action->text().contains("Reset"))
      hasZoomReset = true;
  }
  QVERIFY(hasZoomIn);
  QVERIFY(hasZoomOut);
  QVERIFY(hasZoomReset);
}

QTEST_MAIN(TestMarkdownPreviewPanel)
#include "test_markdownpreviewpanel.moc"
