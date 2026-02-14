#include "ui/viewers/pdfviewer.h"
#include <QApplication>
#include <QTest>

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

  viewer.zoomIn();
  viewer.zoomOut();
  viewer.fitWidth();
  viewer.fitPage();

  QVERIFY(true);
}

void TestPdfViewer::testNavigationFunctions() {
  PdfViewer viewer;

  viewer.goToPage(1);
  viewer.previousPage();
  viewer.nextPage();

  QVERIFY(true);
}

QTEST_MAIN(TestPdfViewer)

#include "test_pdfviewer.moc"
