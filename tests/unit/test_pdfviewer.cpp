#include "ui/viewers/pdfviewer.h"
#include <QApplication>
#include <QLabel>
#include <QPdfView>
#include <QSpinBox>
#include <QTest>

namespace {
QLabel *findLabelByPrefix(PdfViewer &viewer, const QString &prefix) {
  const auto labels = viewer.findChildren<QLabel *>();
  for (QLabel *label : labels) {
    if (label && label->text().startsWith(prefix)) {
      return label;
    }
  }
  return nullptr;
}

QLabel *findLabelBySuffix(PdfViewer &viewer, const QString &suffix) {
  const auto labels = viewer.findChildren<QLabel *>();
  for (QLabel *label : labels) {
    if (label && label->text().endsWith(suffix)) {
      return label;
    }
  }
  return nullptr;
}
} // namespace

class TestPdfViewer : public QObject {
  Q_OBJECT

private slots:
  void testSupportedFormats();
  void testZoomFunctions();
  void testNavigationFunctions();
};

void TestPdfViewer::testSupportedFormats() {

  QVERIFY(PdfViewer::isSupportedPdfFormat("pdf"));
  QVERIFY(PdfViewer::isSupportedPdfFormat("PDF"));
  QVERIFY(PdfViewer::isSupportedPdfFormat("Pdf"));

  QVERIFY(!PdfViewer::isSupportedPdfFormat("txt"));
  QVERIFY(!PdfViewer::isSupportedPdfFormat("png"));
  QVERIFY(!PdfViewer::isSupportedPdfFormat("doc"));
  QVERIFY(!PdfViewer::isSupportedPdfFormat("docx"));
  QVERIFY(!PdfViewer::isSupportedPdfFormat("html"));
}

void TestPdfViewer::testZoomFunctions() {
  PdfViewer viewer;
  auto *pdfView = viewer.findChild<QPdfView *>();
  QVERIFY(pdfView != nullptr);

  viewer.zoomIn();
  QCOMPARE(pdfView->zoomMode(), QPdfView::ZoomMode::Custom);
  QVERIFY(qFuzzyCompare(pdfView->zoomFactor(), 1.25));

  viewer.zoomOut();
  QVERIFY(qFuzzyCompare(pdfView->zoomFactor(), 1.0));
  QLabel *zoomLabel = findLabelBySuffix(viewer, "%");
  QVERIFY(zoomLabel != nullptr);
  QCOMPARE(zoomLabel->text(), QString("100%"));

  viewer.fitPage();
  QCOMPARE(pdfView->zoomMode(), QPdfView::ZoomMode::FitInView);

  viewer.fitWidth();
  QCOMPARE(pdfView->zoomMode(), QPdfView::ZoomMode::FitToWidth);
}

void TestPdfViewer::testNavigationFunctions() {
  PdfViewer viewer;
  auto *pageSpinBox = viewer.findChild<QSpinBox *>();
  QVERIFY(pageSpinBox != nullptr);

  viewer.goToPage(1);
  QCOMPARE(pageSpinBox->value(), 1);

  viewer.previousPage();
  QCOMPARE(pageSpinBox->value(), 1);

  viewer.nextPage();
  QCOMPARE(pageSpinBox->value(), 1);

  QLabel *pageLabel = findLabelByPrefix(viewer, "Page ");
  QVERIFY(pageLabel != nullptr);
  QCOMPARE(pageLabel->text(), QString("Page 1 of 0"));
}

QTEST_MAIN(TestPdfViewer)

#include "test_pdfviewer.moc"
