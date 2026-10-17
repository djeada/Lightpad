#include "ui/dockutils.h"
#include "ui/widgets/elidingtoolbutton.h"

#include <QMainWindow>
#include <QMenu>
#include <QTimer>
#include <QtTest>

class TestDockUtils : public QObject {
  Q_OBJECT

private slots:
  void testToolPanelsAllowAllDockAreas();
  void testToolPanelsStayMovableClosableAndFloatable();
  void testMainWindowDockOptionsSupportDockRearrangement();
  void testDockStateRestoreKeepsHiddenDockPosition();
  void testFloatingDockHasResizeFrame();
  void testInitialBottomDockHeightKeepsEditorSpace();
  void testConstrainedTargetButtonKeepsLabelAndSplitMenu();
  void testTitleBarControlsCloseAndRedock();
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

void TestDockUtils::testFloatingDockHasResizeFrame() {
  QMainWindow mainWindow;
  mainWindow.setDockOptions(DockUtils::mainWindowDockOptions());

  QDockWidget dock("Test");
  DockUtils::configureToolPanelDock(&dock);
  dock.setWidget(new QWidget(&dock));
  mainWindow.addDockWidget(Qt::BottomDockWidgetArea, &dock);
  mainWindow.show();

  dock.setFloating(true);

  QVERIFY(dock.isFloating());
  QVERIFY(dock.windowFlags().testFlag(Qt::Window));
}

void TestDockUtils::testInitialBottomDockHeightKeepsEditorSpace() {
  QCOMPARE(DockUtils::preferredInitialBottomDockHeight(400), 160);
  QCOMPARE(DockUtils::preferredInitialBottomDockHeight(1000), 200);
  QCOMPARE(DockUtils::preferredInitialBottomDockHeight(2000), 240);
}

void TestDockUtils::testConstrainedTargetButtonKeepsLabelAndSplitMenu() {
  ElidingToolButton button;
  const QString label =
      "Run: a_very_long_target_name_that_must_not_expand_the_window";
  button.setText(label);
  button.setAccessibleName(label);
  button.setToolTip(label);
  button.setPopupMode(QToolButton::MenuButtonPopup);
  QMenu menu;
  auto *target = menu.addAction("Choose a different target");
  button.setMenu(&menu);
  button.resize(128, 32);
  button.show();
  QTest::qWait(10);
  QVERIFY(button.sizeHint().width() <= 200);
  QCOMPARE(button.text(), label);
  QCOMPARE(button.accessibleName(), label);
  QSignalSpy clicked(&button, &QToolButton::clicked);
  QSignalSpy changed(target, &QAction::triggered);
  QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(18, 16));
  QCOMPARE(clicked.count(), 1);
  QTimer::singleShot(0, &menu, [&]() {
    target->trigger();
    menu.close();
  });
  QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(124, 16));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(clicked.count(), 1);
  QCOMPARE(button.text(), label);
}

void TestDockUtils::testTitleBarControlsCloseAndRedock() {
  QMainWindow window;
  QDockWidget dock("Tests", &window);
  dock.setWidget(new QWidget(&dock));
  DockUtils::configureToolPanelDock(&dock);
  window.addDockWidget(Qt::BottomDockWidgetArea, &dock);
  window.show();
  auto *close = dock.findChild<QToolButton *>("toolPanelClose");
  auto *undock = dock.findChild<QToolButton *>("toolPanelFloat");
  QVERIFY(close);
  QVERIFY(undock);
  QVERIFY(close->isVisible());
  QVERIFY(undock->isVisible());
  undock->click();
  QVERIFY(dock.isFloating());
  undock->click();
  QVERIFY(!dock.isFloating());
  dock.setFeatures(QDockWidget::DockWidgetMovable);
  QVERIFY(close->isHidden());
  QVERIFY(undock->isHidden());
  dock.setFeatures(DockUtils::toolPanelFeatures());
  close->click();
  QVERIFY(dock.isHidden());
}

QTEST_MAIN(TestDockUtils)
#include "test_dockutils.moc"
