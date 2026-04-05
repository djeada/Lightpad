#include "latex/latexpreviewpanel.h"
#include "lsp/lspclient.h"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QToolBar>

class TestLatexPreviewPanel : public QObject {
  Q_OBJECT

private slots:
  void testIsLatexFile();
  void testSetFilePath();
  void testWidgetStructure();
  void testEngineComboOptions();
  void testBuildSignals();
  void testAutoBuild();
  void testCleanWithNoFile();
  void testBuildWithNoFile();
};

void TestLatexPreviewPanel::testIsLatexFile() {
  QVERIFY(LatexPreviewPanel::isLatexFile("tex"));
  QVERIFY(LatexPreviewPanel::isLatexFile("TEX"));
  QVERIFY(LatexPreviewPanel::isLatexFile("sty"));
  QVERIFY(LatexPreviewPanel::isLatexFile("cls"));
  QVERIFY(LatexPreviewPanel::isLatexFile("bib"));
  QVERIFY(LatexPreviewPanel::isLatexFile("ltx"));

  QVERIFY(!LatexPreviewPanel::isLatexFile("txt"));
  QVERIFY(!LatexPreviewPanel::isLatexFile("md"));
  QVERIFY(!LatexPreviewPanel::isLatexFile("cpp"));
}

void TestLatexPreviewPanel::testSetFilePath() {
  LatexPreviewPanel panel;
  panel.setFilePath("/tmp/test/docs/main.tex");

  QCOMPARE(panel.filePath(), QString("/tmp/test/docs/main.tex"));
  QCOMPARE(panel.basePath(), QString("/tmp/test/docs"));
}

void TestLatexPreviewPanel::testWidgetStructure() {
  LatexPreviewPanel panel;

  auto *logBrowser = panel.findChild<QTextBrowser *>("latexPreviewLogBrowser");
  QVERIFY(logBrowser);
  QVERIFY(logBrowser->isReadOnly());

  auto *toolbar = panel.findChild<QToolBar *>("latexPreviewToolbar");
  QVERIFY(toolbar);
  QVERIFY(!toolbar->actions().isEmpty());

  auto *engineCombo = panel.findChild<QComboBox *>("latexEngineCombo");
  QVERIFY(engineCombo);

  auto *statusLabel = panel.findChild<QLabel *>("latexStatusLabel");
  QVERIFY(statusLabel);
  QCOMPARE(statusLabel->text(), QString("Ready"));
}

void TestLatexPreviewPanel::testEngineComboOptions() {
  LatexPreviewPanel panel;

  auto *engineCombo = panel.findChild<QComboBox *>("latexEngineCombo");
  QVERIFY(engineCombo);
  QCOMPARE(engineCombo->count(), 4);

  QStringList engines;
  for (int i = 0; i < engineCombo->count(); ++i) {
    engines << engineCombo->itemText(i);
  }

  QVERIFY(engines.contains("pdflatex"));
  QVERIFY(engines.contains("xelatex"));
  QVERIFY(engines.contains("lualatex"));
  QVERIFY(engines.contains("latexmk"));
}

void TestLatexPreviewPanel::testBuildSignals() {
  LatexPreviewPanel panel;

  QSignalSpy startSpy(&panel, &LatexPreviewPanel::buildStarted);
  QVERIFY(startSpy.isValid());

  QSignalSpy finishSpy(&panel, &LatexPreviewPanel::buildFinished);
  QVERIFY(finishSpy.isValid());

  QSignalSpy diagSpy(&panel, &LatexPreviewPanel::diagnosticsReady);
  QVERIFY(diagSpy.isValid());
}

void TestLatexPreviewPanel::testAutoBuild() {
  LatexPreviewPanel panel;

  QVERIFY(!panel.autoBuild());

  panel.setAutoBuild(true);
  QVERIFY(panel.autoBuild());

  panel.setAutoBuild(false);
  QVERIFY(!panel.autoBuild());
}

void TestLatexPreviewPanel::testCleanWithNoFile() {
  LatexPreviewPanel panel;

  panel.clean();

  auto *logBrowser = panel.findChild<QTextBrowser *>("latexPreviewLogBrowser");
  QVERIFY(logBrowser);
  QVERIFY(logBrowser->toPlainText().contains("No file"));
}

void TestLatexPreviewPanel::testBuildWithNoFile() {
  LatexPreviewPanel panel;

  panel.build();

  auto *logBrowser = panel.findChild<QTextBrowser *>("latexPreviewLogBrowser");
  QVERIFY(logBrowser);
  QVERIFY(logBrowser->toPlainText().contains("No file"));
}

QTEST_MAIN(TestLatexPreviewPanel)
#include "test_latexpreviewpanel.moc"
