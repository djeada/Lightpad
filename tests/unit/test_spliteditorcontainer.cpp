#include "uitesthelpers.h"

#include "core/lightpadpage.h"
#include "core/lightpadtabwidget.h"
#include "ui/mainwindow.h"
#include "ui/panels/spliteditorcontainer.h"

#include <QSignalSpy>
#include <QSplitter>
#include <QtTest>

Theme::Theme() = default;

TextAreaSettings::TextAreaSettings()
    : autoIndent(false), showLineNumberArea(true), lineHighlighted(true),
      matchingBracketsHighlighted(true), vimModeEnabled(false), tabWidth(2) {}

LightpadTreeView::LightpadTreeView(LightpadPage *parent)
    : QTreeView(parent), parentPage(parent), fileModel(nullptr),
      fileController(nullptr) {}

LightpadTreeView::~LightpadTreeView() = default;

void LightpadTreeView::renameFile(QString oldFilePath, QString newFilePath) {
  Q_UNUSED(oldFilePath);
  Q_UNUSED(newFilePath);
}

void LightpadTreeView::keyPressEvent(QKeyEvent *event) {
  QTreeView::keyPressEvent(event);
}

void LightpadTreeView::mouseReleaseEvent(QMouseEvent *event) {
  QTreeView::mouseReleaseEvent(event);
}

void LightpadTreeView::dragEnterEvent(QDragEnterEvent *event) {
  QTreeView::dragEnterEvent(event);
}

void LightpadTreeView::dragMoveEvent(QDragMoveEvent *event) {
  QTreeView::dragMoveEvent(event);
}

void LightpadTreeView::dropEvent(QDropEvent *event) {
  QTreeView::dropEvent(event);
}

LightpadPage::LightpadPage(QWidget *parent, bool treeViewHidden)
    : QWidget(parent), mainWindow(nullptr), treeView(nullptr),
      textArea(nullptr), minimap(nullptr), model(nullptr),
      m_ownsModel(!treeViewHidden), m_gitIntegration(nullptr) {}

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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(nullptr) {}

MainWindow::~MainWindow() = default;

void MainWindow::keyPressEvent(QKeyEvent *event) {
  QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
  QMainWindow::closeEvent(event);
}

#define DEFINE_MAINWINDOW_SLOT(name)                                           \
  void MainWindow::name() {}
#define DEFINE_MAINWINDOW_BOOL_SLOT(name)                                      \
  void MainWindow::name(bool) {}

DEFINE_MAINWINDOW_SLOT(on_actionQuit_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Full_Screen_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Undo_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Redo_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionIncrease_Font_Size_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionDecrease_Font_Size_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionReset_Font_Size_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionCut_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionCopy_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionPaste_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionNew_Window_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionClose_Tab_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionClose_All_Tabs_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionFind_in_file_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionFind_in_project_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionNew_File_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionOpen_File_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionOpen_Project_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionSave_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionSave_as_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionReplace_in_file_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionReplace_in_project_triggered)
DEFINE_MAINWINDOW_SLOT(on_languageHighlight_clicked)
DEFINE_MAINWINDOW_SLOT(on_actionAbout_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionAbout_Qt_triggered)
DEFINE_MAINWINDOW_SLOT(on_tabWidth_clicked)
DEFINE_MAINWINDOW_SLOT(on_actionKeyboard_shortcuts_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionPreferences_triggered)
DEFINE_MAINWINDOW_SLOT(showThemeGallery)
DEFINE_MAINWINDOW_SLOT(on_runButton_clicked)
DEFINE_MAINWINDOW_SLOT(on_debugButton_clicked)
DEFINE_MAINWINDOW_SLOT(on_actionRun_file_name_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionDebug_file_name_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionStart_Debug_Configuration_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionEdit_Configurations_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionEdit_Debug_Configurations_triggered)
DEFINE_MAINWINDOW_SLOT(on_magicButton_clicked)
DEFINE_MAINWINDOW_SLOT(on_actionFormat_Document_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionEdit_Format_Configurations_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionGo_to_Line_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Minimap_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionSplit_Horizontally_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionSplit_Vertically_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionClose_Editor_Group_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionFocus_Next_Group_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionFocus_Previous_Group_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionUnsplit_All_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Terminal_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Source_Control_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Test_Panel_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Problems_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionPreview_Markdown_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionPreview_LaTeX_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionOpen_To_Side_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionGit_Log_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionGit_File_History_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionGit_Rebase_triggered)
DEFINE_MAINWINDOW_BOOL_SLOT(on_actionToggle_Heatmap_triggered)
DEFINE_MAINWINDOW_BOOL_SLOT(on_actionToggle_CodeLens_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionTransform_Uppercase_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionTransform_Lowercase_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionTransform_Title_Case_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionSort_Lines_Ascending_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionSort_Lines_Descending_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Word_Wrap_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionToggle_Vim_Mode_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionFold_Current_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionUnfold_Current_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionFold_All_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionUnfold_All_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionFold_Comments_triggered)
DEFINE_MAINWINDOW_SLOT(on_actionUnfold_Comments_triggered)

#undef DEFINE_MAINWINDOW_SLOT
#undef DEFINE_MAINWINDOW_BOOL_SLOT

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
  void testTabThemeUsesThemeDrivenColors();

private:
  QSplitter *findRootSplitter(SplitEditorContainer &container);
};

QSplitter *
TestSplitEditorContainer::findRootSplitter(SplitEditorContainer &container) {
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

void TestSplitEditorContainer::testTabThemeUsesThemeDrivenColors() {
  LightpadTabWidget tabWidget;
  tabWidget.addTab(new QWidget(&tabWidget), QStringLiteral("main.cpp"));
  tabWidget.setTheme("#101820", "#f2f5f7", "#17232d", "#23435b", "#61dafb",
                     "#314657");

  const QString style = tabWidget.styleSheet();
  QVERIFY(style.contains("#23435b"));
  QVERIFY(style.contains("#61dafb"));
  QVERIFY(style.contains("border-radius: 0px;"));
  QVERIFY(style.contains("border-right: 1px solid #314657;"));
  QVERIFY(style.contains("QToolButton#AddTabButton"));

  auto *closeButton = qobject_cast<QToolButton *>(
      tabWidget.tabBar()->tabButton(0, QTabBar::RightSide));
  QVERIFY(closeButton != nullptr);
  QVERIFY(closeButton->styleSheet().contains("#23435b"));
  QVERIFY(closeButton->styleSheet().contains("#61dafb"));
  QVERIFY(!closeButton->styleSheet().contains("#e81123"));
}

QTEST_MAIN(TestSplitEditorContainer)
#include "test_spliteditorcontainer.moc"
