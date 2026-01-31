#include <QTest>
#include <QApplication>
#include "ui/viewers/pdfviewer.h"

class TestPdfViewer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testSupportedFormats();
    void testZoomFunctions();
    void testNavigationFunctions();
    void cleanupTestCase();

private:
    QApplication* app;
};

void TestPdfViewer::initTestCase()
{
    static int argc = 1;
    static char* argv[] = { const_cast<char*>("test_pdfviewer") };
    app = new QApplication(argc, argv);
}

void TestPdfViewer::cleanupTestCase()
{
    delete app;
}

void TestPdfViewer::testSupportedFormats()
{
    // Test supported format
    QVERIFY(PdfViewer::isSupportedPdfFormat("pdf"));
    QVERIFY(PdfViewer::isSupportedPdfFormat("PDF"));
    QVERIFY(PdfViewer::isSupportedPdfFormat("Pdf"));
    
    // Test unsupported formats
    QVERIFY(!PdfViewer::isSupportedPdfFormat("txt"));
    QVERIFY(!PdfViewer::isSupportedPdfFormat("png"));
    QVERIFY(!PdfViewer::isSupportedPdfFormat("doc"));
    QVERIFY(!PdfViewer::isSupportedPdfFormat("docx"));
    QVERIFY(!PdfViewer::isSupportedPdfFormat("html"));
}

void TestPdfViewer::testZoomFunctions()
{
    PdfViewer viewer;
    
    // Test that zoom functions don't crash even without a loaded PDF
    viewer.zoomIn();
    viewer.zoomOut();
    viewer.fitWidth();
    viewer.fitPage();
    
    // All operations should complete without crashing
    QVERIFY(true);
}

void TestPdfViewer::testNavigationFunctions()
{
    PdfViewer viewer;
    
    // Test that navigation functions don't crash even without a loaded PDF
    viewer.goToPage(1);
    viewer.previousPage();
    viewer.nextPage();
    
    // All operations should complete without crashing
    QVERIFY(true);
}

QTEST_APPLESS_MAIN(TestPdfViewer)

#include "test_pdfviewer.moc"
