#include "ui/dockutils.h"

#include <QMainWindow>
#include <QtTest>

class TestDockUtils : public QObject {
  Q_OBJECT

private slots:
  void testToolPanelsAllowAllDockAreas();
  void testToolPanelsStayMovableClosableAndFloatable();
  void testMainWindowDockOptionsSupportDockRearrangement();
};

void TestDockUtils::testToolPanelsAllowAllDockAreas() {
  QDockWidget dock;

  DockUtils::configureToolPanelDock(&dock);

  QCOMPARE(dock.allowedAreas(), Qt::AllDockWidgetAreas);
}

void TestDockUtils::testToolPanelsStayMovableClosableAndFloatable() {
  QDockWidget dock;

  DockUtils::configureToolPanelDock(&dock);

  QCOMPARE(dock.features(), DockUtils::toolPanelFeatures());
}

void TestDockUtils::testMainWindowDockOptionsSupportDockRearrangement() {
  const auto options = DockUtils::mainWindowDockOptions();

  QVERIFY(options.testFlag(QMainWindow::AllowTabbedDocks));
  QVERIFY(options.testFlag(QMainWindow::AllowNestedDocks));
  QVERIFY(options.testFlag(QMainWindow::AnimatedDocks));
  QVERIFY(options.testFlag(QMainWindow::GroupedDragging));
}

QTEST_MAIN(TestDockUtils)
#include "test_dockutils.moc"
