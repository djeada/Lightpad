#include "ui/dialogs/commandpalette.h"

#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class TestCommandPalette : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void testDestroyedActionsCanBeSearchedSafely();
  void testDestroyedSelectionDoesNotCloseThePalette();
  void testUnavailableSelectionIsNotExecutedOrRecorded();
  void testOnlyAvailableActionsAreOffered();

private:
  QTemporaryDir m_settings;
};

void TestCommandPalette::initTestCase() {
  QVERIFY(m_settings.isValid());
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                    m_settings.path());
}

void TestCommandPalette::testDestroyedActionsCanBeSearchedSafely() {
  CommandPalette palette;
  auto action = std::make_unique<QAction>("Run");
  palette.registerAction(action.get());
  action.reset();
  palette.showPalette();
  QCOMPARE(palette.findChild<QListWidget *>("commandPaletteResults")->count(), 0);
  QVERIFY(palette.findChild<QLabel *>("commandPaletteEmptyState")->isVisible());
}

void TestCommandPalette::testDestroyedSelectionDoesNotCloseThePalette() {
  CommandPalette palette;
  auto action = std::make_unique<QAction>("Build");
  palette.registerAction(action.get());
  palette.showPalette();
  QCOMPARE(palette.findChild<QListWidget *>("commandPaletteResults")->count(), 1);
  action.reset();
  QTest::keyClick(palette.findChild<QLineEdit *>("commandPaletteSearch"),
                  Qt::Key_Return);
  QVERIFY(palette.isVisible());
  QCOMPARE(palette.findChild<QListWidget *>("commandPaletteResults")->count(), 0);
}

void TestCommandPalette::testUnavailableSelectionIsNotExecutedOrRecorded() {
  QSettings settings("Lightpad", "Lightpad");
  settings.remove("commandPalette/recentCommands");
  CommandPalette palette;
  QAction action("Save");
  action.setObjectName("saveUnavailable");
  QSignalSpy triggered(&action, &QAction::triggered);
  palette.registerAction(&action);
  palette.showPalette();
  action.setEnabled(false);
  QTest::keyClick(palette.findChild<QLineEdit *>("commandPaletteSearch"),
                  Qt::Key_Return);
  QCOMPARE(triggered.count(), 0);
  QVERIFY(palette.isVisible());
  QVERIFY(!settings.value("commandPalette/recentCommands").toStringList()
               .contains("saveUnavailable"));
}

void TestCommandPalette::testOnlyAvailableActionsAreOffered() {
  CommandPalette palette;
  QAction disabled("Disabled");
  disabled.setEnabled(false);
  QAction hidden("Hidden");
  hidden.setVisible(false);
  QAction available("Open");
  palette.registerAction(&disabled);
  palette.registerAction(&hidden);
  palette.registerAction(&available);
  palette.showPalette();
  auto *results = palette.findChild<QListWidget *>("commandPaletteResults");
  QCOMPARE(results->count(), 1);
  QSignalSpy triggered(&available, &QAction::triggered);
  QTest::keyClick(palette.findChild<QLineEdit *>("commandPaletteSearch"),
                  Qt::Key_Return);
  QCOMPARE(triggered.count(), 1);
  QVERIFY(!palette.isVisible());
}

QTEST_MAIN(TestCommandPalette)
#include "test_commandpalette.moc"
