#include "markdown/markdownpreviewpanel.h"
#include <QApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QToolBar>

class TestMarkdownPreviewPanel : public QObject {
  Q_OBJECT

private slots:
  void testIsMarkdownFile();
  void testSetMarkdown();
  void testSetFilePath();
  void testUpdatePreview();
  void testLinkClickedSignal();
  void testWidgetStructure();
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

QTEST_MAIN(TestMarkdownPreviewPanel)
#include "test_markdownpreviewpanel.moc"
