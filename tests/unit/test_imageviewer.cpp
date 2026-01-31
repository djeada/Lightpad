#include <QTest>
#include <QApplication>
#include <QTemporaryFile>
#include <QImage>
#include "ui/viewers/imageviewer.h"

class TestImageViewer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testSupportedFormats();
    void testLoadValidImage();
    void testZoomFunctions();
    void cleanupTestCase();

private:
    QString testImagePath;
};

void TestImageViewer::initTestCase()
{
    // Create a test image
    QTemporaryFile tempFile(QDir::tempPath() + "/test_image_XXXXXX.png");
    tempFile.setAutoRemove(false);
    if (tempFile.open()) {
        testImagePath = tempFile.fileName();
        QImage testImage(100, 100, QImage::Format_RGB32);
        testImage.fill(Qt::red);
        testImage.save(testImagePath, "PNG");
        tempFile.close();
    }
}

void TestImageViewer::cleanupTestCase()
{
    // Clean up test image
    if (!testImagePath.isEmpty() && QFile::exists(testImagePath)) {
        QFile::remove(testImagePath);
    }
}

void TestImageViewer::testSupportedFormats()
{
    // Test supported formats
    QVERIFY(ImageViewer::isSupportedImageFormat("png"));
    QVERIFY(ImageViewer::isSupportedImageFormat("PNG"));
    QVERIFY(ImageViewer::isSupportedImageFormat("jpg"));
    QVERIFY(ImageViewer::isSupportedImageFormat("jpeg"));
    QVERIFY(ImageViewer::isSupportedImageFormat("gif"));
    QVERIFY(ImageViewer::isSupportedImageFormat("bmp"));
    QVERIFY(ImageViewer::isSupportedImageFormat("webp"));
    QVERIFY(ImageViewer::isSupportedImageFormat("svg"));
    QVERIFY(ImageViewer::isSupportedImageFormat("ico"));
    QVERIFY(ImageViewer::isSupportedImageFormat("tiff"));
    
    // Test unsupported formats
    QVERIFY(!ImageViewer::isSupportedImageFormat("txt"));
    QVERIFY(!ImageViewer::isSupportedImageFormat("cpp"));
    QVERIFY(!ImageViewer::isSupportedImageFormat("pdf"));
    QVERIFY(!ImageViewer::isSupportedImageFormat("html"));
}

void TestImageViewer::testLoadValidImage()
{
    ImageViewer viewer;
    
    // Load the test image
    bool result = viewer.loadImage(testImagePath);
    QVERIFY(result);
    QCOMPARE(viewer.getFilePath(), testImagePath);
}

void TestImageViewer::testZoomFunctions()
{
    ImageViewer viewer;
    
    // Load a valid image first
    viewer.loadImage(testImagePath);
    
    // Test that zoom functions don't crash
    viewer.zoomIn();
    viewer.zoomOut();
    viewer.fitToWindow();
    viewer.actualSize();
    
    // All operations should complete without crashing
    QVERIFY(true);
}

QTEST_MAIN(TestImageViewer)

#include "test_imageviewer.moc"
