#include "ui/dockutils.h"

#include <QMainWindow>
#include <QtTest>

class TestDockUtils : public QObject {
  Q_OBJECT

private slots:
  void testToolPanelsAllowAllDockAreas();
  void testToolPanelsStayMovableClosableAndFloatable();
  void testMainWindowDockOptionsSupportDockRearrangement();
  void testDockStateRestoreKeepsHiddenDockPosition();
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

void TestDockUtils::testDockStateRestoreKeepsHiddenDockPosition() {
  QMainWindow firstWindow;
  firstWindow.setDockOptions(DockUtils::mainWindowDockOptions());

  QDockWidget firstTerminalDock("Terminal");
  firstTerminalDock.setObjectName("terminalDock");
  DockUtils::configureToolPanelDock(&firstTerminalDock);
  firstTerminalDock.setWidget(new QWidget(&firstTerminalDock));
  firstWindow.addDockWidget(Qt::BottomDockWidgetArea, &firstTerminalDock);

  QDockWidget firstSourceControlDock("Source Control");
  firstSourceControlDock.setObjectName("sourceControlDock");
  DockUtils::configureToolPanelDock(&firstSourceControlDock);
  firstSourceControlDock.setWidget(new QWidget(&firstSourceControlDock));
  firstWindow.addDockWidget(Qt::LeftDockWidgetArea, &firstSourceControlDock);
  firstSourceControlDock.hide();

  const QByteArray savedState = firstWindow.saveState();
  QVERIFY(!savedState.isEmpty());

  QMainWindow secondWindow;
  secondWindow.setDockOptions(DockUtils::mainWindowDockOptions());

  QDockWidget secondTerminalDock("Terminal");
  secondTerminalDock.setObjectName("terminalDock");
  DockUtils::configureToolPanelDock(&secondTerminalDock);
  secondTerminalDock.setWidget(new QWidget(&secondTerminalDock));
  secondWindow.addDockWidget(Qt::BottomDockWidgetArea, &secondTerminalDock);

  QDockWidget secondSourceControlDock("Source Control");
  secondSourceControlDock.setObjectName("sourceControlDock");
  DockUtils::configureToolPanelDock(&secondSourceControlDock);
  secondSourceControlDock.setWidget(new QWidget(&secondSourceControlDock));
  secondWindow.addDockWidget(Qt::RightDockWidgetArea, &secondSourceControlDock);
  secondSourceControlDock.hide();

  QVERIFY(secondWindow.restoreState(savedState));
  QCOMPARE(secondWindow.dockWidgetArea(&secondSourceControlDock),
           Qt::LeftDockWidgetArea);
}

QTEST_MAIN(TestDockUtils)
#include "test_dockutils.moc"
