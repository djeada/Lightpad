#include "uitesthelpers.h"

#include "core/lightpadpage.h"
#include "core/lightpadtabwidget.h"
#include "ui/mainwindow.h"
#include "ui/panels/spliteditorcontainer.h"

#include <QSignalSpy>
#include <QSplitter>
#include <QtTest>

LightpadPage::LightpadPage(QWidget *parent, bool treeViewHidden)
    : QWidget(parent), mainWindow(nullptr), treeView(nullptr),
      textArea(nullptr), minimap(nullptr), model(nullptr),
      m_ownsModel(!treeViewHidden),
      m_gitIntegration(nullptr) {}

QTreeView *LightpadPage::getTreeView() { return treeView; }

TextArea *LightpadPage::getTextArea() { return textArea; }

Minimap *LightpadPage::getMinimap() { return minimap; }

void LightpadPage::setTreeViewVisible(bool flag) { Q_UNUSED(flag); }

void LightpadPage::setModelRootIndex(QString path) { Q_UNUSED(path); }

void LightpadPage::setCustomContentWidget(QWidget *widget) {
  if (widget) {
    widget->setParent(this);
  }
}

void LightpadPage::setMainWindow(MainWindow *window) { mainWindow = window; }

void LightpadPage::setFilePath(QString path) { filePath = path; }

QString LightpadPage::getFilePath() { return filePath; }

void LightpadPage::setProjectRootPath(const QString &path) {
  projectRootPath = path;
}

QString LightpadPage::getProjectRootPath() const { return projectRootPath; }

void LightpadPage::setGitIntegration(GitIntegration *git) {
  m_gitIntegration = git;
}

MainWindow *LightpadPage::getMainWindow() const { return mainWindow; }

QString MainWindow::getProjectRootPath() const { return QString(); }

GitIntegration *MainWindow::getGitIntegration() const { return nullptr; }

class TestSplitEditorContainer : public QObject {
  Q_OBJECT

private slots:
  void testInitialState();
  void testSplitHorizontalAddsGroup();
  void testSplitVerticalCreatesNestedSplitter();
  void testFocusNavigationTracksCurrentGroup();
  void testCloseCurrentGroupRestoresSingleGroup();
  void testUnsplitAllRestoresSingleGroup();
  void testFocusEventUpdatesCurrentGroup();
  void testSnapshotNotEmpty();

private:
  QSplitter *findRootSplitter(SplitEditorContainer &container);
};

QSplitter *TestSplitEditorContainer::findRootSplitter(
    SplitEditorContainer &container) {
  return container.findChild<QSplitter *>("splitEditorRootSplitter");
}

void TestSplitEditorContainer::testInitialState() {
  SplitEditorContainer container;

  QCOMPARE(container.objectName(), QString("splitEditorContainer"));
  QCOMPARE(container.groupCount(), 1);
  QVERIFY(container.currentTabWidget() != nullptr);
  QVERIFY(findRootSplitter(container) != nullptr);
  QCOMPARE(findRootSplitter(container)->count(), 1);
}

void TestSplitEditorContainer::testSplitHorizontalAddsGroup() {
  SplitEditorContainer container;
  QSignalSpy splitSpy(&container, &SplitEditorContainer::splitCountChanged);
  QSignalSpy currentSpy(&container, &SplitEditorContainer::currentGroupChanged);

  LightpadTabWidget *newGroup = container.splitHorizontal();

  QVERIFY(newGroup != nullptr);
  QCOMPARE(container.groupCount(), 2);
  QCOMPARE(container.currentTabWidget(), newGroup);
  QCOMPARE(findRootSplitter(container)->count(), 2);
  QVERIFY(splitSpy.count() >= 1);
  QVERIFY(currentSpy.count() >= 1);
}

void TestSplitEditorContainer::testSplitVerticalCreatesNestedSplitter() {
  SplitEditorContainer container;
  container.splitVertical();

  QList<QSplitter *> splitters = container.findChildren<QSplitter *>();
  QVERIFY(splitters.size() >= 2);

  bool foundVerticalSplitter = false;
  for (QSplitter *splitter : splitters) {
    if (splitter != findRootSplitter(container) &&
        splitter->orientation() == Qt::Vertical) {
      foundVerticalSplitter = true;
      break;
    }
  }

  QVERIFY(foundVerticalSplitter);
  QCOMPARE(container.groupCount(), 2);
}

void TestSplitEditorContainer::testFocusNavigationTracksCurrentGroup() {
  SplitEditorContainer container;
  LightpadTabWidget *firstGroup = container.currentTabWidget();
  LightpadTabWidget *secondGroup = container.splitHorizontal();
  QVERIFY(firstGroup != nullptr);
  QVERIFY(secondGroup != nullptr);

  container.focusPreviousGroup();
  QCOMPARE(container.currentTabWidget(), firstGroup);

  container.focusNextGroup();
  QCOMPARE(container.currentTabWidget(), secondGroup);
}

void TestSplitEditorContainer::testCloseCurrentGroupRestoresSingleGroup() {
  SplitEditorContainer container;
  LightpadTabWidget *firstGroup = container.currentTabWidget();
  QVERIFY(container.splitHorizontal() != nullptr);

  QVERIFY(container.closeCurrentGroup());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  QCOMPARE(container.groupCount(), 1);
  QCOMPARE(container.currentTabWidget(), firstGroup);
  QCOMPARE(findRootSplitter(container)->count(), 1);
}

void TestSplitEditorContainer::testUnsplitAllRestoresSingleGroup() {
  SplitEditorContainer container;
  QVERIFY(container.splitHorizontal() != nullptr);
  QVERIFY(container.splitVertical() != nullptr);
  QVERIFY(container.groupCount() >= 2);

  container.unsplitAll();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  QCOMPARE(container.groupCount(), 1);
  QCOMPARE(findRootSplitter(container)->count(), 1);
  QVERIFY(!container.hasSplits());
}

void TestSplitEditorContainer::testFocusEventUpdatesCurrentGroup() {
  SplitEditorContainer container;
  UiTestHelpers::showWidget(container);
  LightpadTabWidget *firstGroup = container.currentTabWidget();
  LightpadTabWidget *secondGroup = container.splitHorizontal();
  QVERIFY(firstGroup != nullptr);
  QVERIFY(secondGroup != nullptr);

  firstGroup->setFocus();
  QApplication::processEvents();
  QTest::qWait(20);

  QCOMPARE(container.currentTabWidget(), firstGroup);
}

void TestSplitEditorContainer::testSnapshotNotEmpty() {
  SplitEditorContainer container;
  container.splitHorizontal();

  const QPixmap snapshot = UiTestHelpers::captureSnapshot(container);

  QVERIFY(!snapshot.isNull());
  QCOMPARE(snapshot.size(), container.size());
}

QTEST_MAIN(TestSplitEditorContainer)
#include "test_spliteditorcontainer.moc"
