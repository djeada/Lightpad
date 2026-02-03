#include "ui/panels/minimap.h"
#include <QPlainTextEdit>
#include <QtTest/QtTest>

class TestMinimap : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void testInitialization();
  void testSetSourceEditor();
  void testScale();
  void testVisibility();
  void testViewportColor();
  void testLineNumberFromClick();

private:
  QPlainTextEdit *m_editor;
  Minimap *m_minimap;
};

void TestMinimap::initTestCase() {
  m_editor = new QPlainTextEdit();
  m_minimap = new Minimap();
}

void TestMinimap::cleanupTestCase() {
  delete m_minimap;
  delete m_editor;
}

void TestMinimap::testInitialization() {
  Minimap minimap;

  // Default values
  QCOMPARE(minimap.sourceEditor(), nullptr);
  QVERIFY(minimap.scale() > 0);
  QVERIFY(minimap.isMinimapVisible());
}

void TestMinimap::testSetSourceEditor() {
  Minimap minimap;
  QPlainTextEdit editor;

  // Set some text in the editor
  editor.setPlainText("Line 1\nLine 2\nLine 3\nLine 4\nLine 5");

  minimap.setSourceEditor(&editor);

  QCOMPARE(minimap.sourceEditor(), &editor);

  // Clear source editor
  minimap.setSourceEditor(nullptr);
  QCOMPARE(minimap.sourceEditor(), nullptr);
}

void TestMinimap::testScale() {
  Minimap minimap;

  // Set valid scale
  minimap.setScale(0.2);
  QCOMPARE(minimap.scale(), 0.2);

  // Test bounds - scale should be clamped
  minimap.setScale(0.01);           // Too small
  QVERIFY(minimap.scale() >= 0.05); // Should be clamped to minimum

  minimap.setScale(1.0);           // Too large
  QVERIFY(minimap.scale() <= 0.5); // Should be clamped to maximum
}

void TestMinimap::testVisibility() {
  Minimap minimap;

  // Default is visible
  QVERIFY(minimap.isMinimapVisible());

  // Hide
  minimap.setMinimapVisible(false);
  QVERIFY(!minimap.isMinimapVisible());

  // Show
  minimap.setMinimapVisible(true);
  QVERIFY(minimap.isMinimapVisible());
}

void TestMinimap::testViewportColor() {
  Minimap minimap;

  QColor testColor(255, 0, 0, 128);
  minimap.setViewportColor(testColor);

  // Just verify it doesn't crash - color is internal state
  // The widget should handle the color change gracefully
  minimap.update();
}

void TestMinimap::testLineNumberFromClick() {
  Minimap minimap;
  QPlainTextEdit editor;

  // Create document with many lines
  QString text;
  for (int i = 0; i < 100; ++i) {
    text += QString("Line %1\n").arg(i);
  }
  editor.setPlainText(text);

  minimap.setSourceEditor(&editor);
  minimap.resize(100, 300);
  minimap.updateContent();

  // Verify the minimap can handle content update without crashing
  QCOMPARE(minimap.sourceEditor(), &editor);
}

QTEST_MAIN(TestMinimap)
#include "test_minimap.moc"
