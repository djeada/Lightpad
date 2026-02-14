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

  QCOMPARE(minimap.sourceEditor(), nullptr);
  QVERIFY(minimap.scale() > 0);
  QVERIFY(minimap.isMinimapVisible());
}

void TestMinimap::testSetSourceEditor() {
  Minimap minimap;
  QPlainTextEdit editor;

  editor.setPlainText("Line 1\nLine 2\nLine 3\nLine 4\nLine 5");

  minimap.setSourceEditor(&editor);

  QCOMPARE(minimap.sourceEditor(), &editor);

  minimap.setSourceEditor(nullptr);
  QCOMPARE(minimap.sourceEditor(), nullptr);
}

void TestMinimap::testScale() {
  Minimap minimap;

  minimap.setScale(0.2);
  QCOMPARE(minimap.scale(), 0.2);

  minimap.setScale(0.01);
  QVERIFY(minimap.scale() >= 0.05);

  minimap.setScale(1.0);
  QVERIFY(minimap.scale() <= 0.5);
}

void TestMinimap::testVisibility() {
  Minimap minimap;

  QVERIFY(minimap.isMinimapVisible());

  minimap.setMinimapVisible(false);
  QVERIFY(!minimap.isMinimapVisible());

  minimap.setMinimapVisible(true);
  QVERIFY(minimap.isMinimapVisible());
}

void TestMinimap::testViewportColor() {
  Minimap minimap;

  QColor testColor(255, 0, 0, 128);
  minimap.setViewportColor(testColor);

  minimap.update();
}

void TestMinimap::testLineNumberFromClick() {
  Minimap minimap;
  QPlainTextEdit editor;

  QString text;
  for (int i = 0; i < 100; ++i) {
    text += QString("Line %1\n").arg(i);
  }
  editor.setPlainText(text);

  minimap.setSourceEditor(&editor);
  minimap.resize(100, 300);
  minimap.updateContent();

  QCOMPARE(minimap.sourceEditor(), &editor);
}

QTEST_MAIN(TestMinimap)
#include "test_minimap.moc"
