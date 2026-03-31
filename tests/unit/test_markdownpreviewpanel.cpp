#include "markdown/markdownpreviewpanel.h"
#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>

#ifdef HAVE_WEBENGINE
#include <QWebEngineView>
#else
#include <QTextBrowser>
#endif

class TestMarkdownPreviewPanel : public QObject {
  Q_OBJECT

private slots:
  void testIsMarkdownFile();
  void testSetMarkdown();
  void testSetFilePath();
  void testUpdatePreview();
  void testLinkClickedSignal();
  void testWidgetStructure();
  void testWordCountLabel();
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

#ifdef HAVE_WEBENGINE
  auto *webView = panel.findChild<QWebEngineView *>("markdownPreviewWebView");
  QVERIFY(webView);
#else
  auto *browser = panel.findChild<QTextBrowser *>("markdownPreviewBrowser");
  QVERIFY(browser);
  QVERIFY(!browser->toPlainText().isEmpty());
#endif
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

#ifdef HAVE_WEBENGINE
  auto *webView = panel.findChild<QWebEngineView *>("markdownPreviewWebView");
  QVERIFY(webView);
#else
  auto *browser = panel.findChild<QTextBrowser *>("markdownPreviewBrowser");
  QVERIFY(browser);
  QString html = browser->toHtml();
  QVERIFY(!html.isEmpty());
#endif
}

void TestMarkdownPreviewPanel::testLinkClickedSignal() {
  MarkdownPreviewPanel panel;
  QSignalSpy spy(&panel, &MarkdownPreviewPanel::linkClicked);
  QVERIFY(spy.isValid());
}

void TestMarkdownPreviewPanel::testWidgetStructure() {
  MarkdownPreviewPanel panel;

#ifdef HAVE_WEBENGINE
  auto *webView = panel.findChild<QWebEngineView *>("markdownPreviewWebView");
  QVERIFY(webView);
#else
  auto *browser = panel.findChild<QTextBrowser *>("markdownPreviewBrowser");
  QVERIFY(browser);
  QVERIFY(browser->isReadOnly());
#endif

  auto *toolbar = panel.findChild<QToolBar *>("markdownPreviewToolbar");
  QVERIFY(toolbar);
  QVERIFY(!toolbar->actions().isEmpty());
}

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

void TestMarkdownPreviewPanel::testExportToHtml() {
  MarkdownPreviewPanel panel;

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
