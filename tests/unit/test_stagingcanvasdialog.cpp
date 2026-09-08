#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/dialogs/stagingcanvasdialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QtTest/QtTest>

class TestStagingCanvasDialog : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testListsChangedFiles();
  void testFileStateMarkers();
  void testColumnsShowTheirOwnLayer();
  void testHeadColumnIsReadOnly();
  void testContextLinesAreNotSelectable();
  void testStageButtonDescribesWholeFile();
  void testStageButtonDescribesSelectedLines();
  void testStageSelectedLine();
  void testStageHunkViaHeaderRow();
  void testUnstageDoesNotTouchWorkingTree();
  void testKeyboardStageAndUnstage();
  void testWorkingHeadPairIsReadOnly();
  void testComparePairSwitchesWorkingColumn();
  void testEmptyRepositoryState();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  QString gitOut(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  QString readFile(const QString &name);
  QListWidget *column(StagingCanvasDialog &dialog, const char *which);
};

void TestStagingCanvasDialog::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "test@test.com"}));
  QVERIFY(git({"config", "user.name", "Test User"}));

  writeFile("f.txt", "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestStagingCanvasDialog::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestStagingCanvasDialog::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

QString TestStagingCanvasDialog::gitOut(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  process.waitForFinished(GIT_COMMAND_TIMEOUT_MS);
  return QString::fromUtf8(process.readAllStandardOutput());
}

void TestStagingCanvasDialog::writeFile(const QString &name,
                                        const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

QString TestStagingCanvasDialog::readFile(const QString &name) {
  QFile file(m_repoPath + "/" + name);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

QListWidget *TestStagingCanvasDialog::column(StagingCanvasDialog &dialog,
                                             const char *which) {
  const QString name =
      QStringLiteral("staging%1Column").arg(QString::fromLatin1(which));
  QListWidget *list = dialog.findChild<QListWidget *>(name);
  Q_ASSERT(list);
  return list;
}

void TestStagingCanvasDialog::testListsChangedFiles() {
  writeFile("f.txt", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  writeFile("other.txt", "new file\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QTreeWidget *files = dialog.findChild<QTreeWidget *>("stagingFileTree");
  QVERIFY(files);
  QCOMPARE(files->topLevelItemCount(), 2);
}

void TestStagingCanvasDialog::testFileStateMarkers() {
  writeFile("f.txt", "STAGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  QVERIFY(git({"add", "f.txt"}));
  writeFile("f.txt", "STAGED\nDIRTY\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QTreeWidget *files = dialog.findChild<QTreeWidget *>("stagingFileTree");
  QCOMPARE(files->topLevelItemCount(), 1);

  const QString state = files->topLevelItem(0)->text(1);
  QVERIFY2(state.contains(QString::fromUtf8("▲")), qPrintable(state));
  QVERIFY2(state.contains(QString::fromUtf8("●")), qPrintable(state));
}

void TestStagingCanvasDialog::testColumnsShowTheirOwnLayer() {
  writeFile("f.txt", "STAGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  QVERIFY(git({"add", "f.txt"}));
  writeFile("f.txt", "STAGED\nDIRTY\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());

  QStringList workingText;
  QListWidget *working = column(dialog, "Working");
  for (int i = 0; i < working->count(); ++i) {
    workingText << working->item(i)->text();
  }
  QStringList indexText;
  QListWidget *index = column(dialog, "Index");
  for (int i = 0; i < index->count(); ++i) {
    indexText << index->item(i)->text();
  }

  QVERIFY2(workingText.join("\n").contains("+DIRTY"),
           qPrintable(workingText.join("\n")));
  QVERIFY2(!workingText.join("\n").contains("+STAGED"),
           qPrintable(workingText.join("\n")));
  QVERIFY2(indexText.join("\n").contains("+STAGED"),
           qPrintable(indexText.join("\n")));
  QVERIFY2(!indexText.join("\n").contains("+DIRTY"),
           qPrintable(indexText.join("\n")));
}

void TestStagingCanvasDialog::testHeadColumnIsReadOnly() {
  writeFile("f.txt", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QListWidget *head = column(dialog, "Head");
  QCOMPARE(head->selectionMode(), QAbstractItemView::NoSelection);
  QCOMPARE(head->focusPolicy(), Qt::NoFocus);
  QVERIFY(head->count() > 0);

  QVERIFY(head->item(0)->text().contains("l1"));
}

void TestStagingCanvasDialog::testContextLinesAreNotSelectable() {
  writeFile("f.txt", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QListWidget *working = column(dialog, "Working");

  bool sawContext = false;
  bool sawChange = false;
  for (int i = 0; i < working->count(); ++i) {
    QListWidgetItem *item = working->item(i);
    const QString text = item->text();
    if (text.startsWith("+") || text.startsWith("-")) {
      sawChange = true;
      QVERIFY(item->flags().testFlag(Qt::ItemIsSelectable));
    } else if (text.startsWith(" ")) {
      sawContext = true;
      QVERIFY(!item->flags().testFlag(Qt::ItemIsSelectable));
    }
  }
  QVERIFY(sawChange);
  QVERIFY(sawContext);
}

void TestStagingCanvasDialog::testStageButtonDescribesWholeFile() {
  writeFile("f.txt", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  const QString stageText =
      dialog.findChild<QPushButton *>("stagingStageButton")->text();

  QVERIFY2(stageText.contains("hunk") || stageText.contains("file"),
           qPrintable(stageText));
}

void TestStagingCanvasDialog::testStageButtonDescribesSelectedLines() {
  writeFile("f.txt", "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n"
                     "new1\nnew2\nnew3\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QListWidget *working = column(dialog, "Working");

  int selected = 0;
  for (int i = 0; i < working->count() && selected < 2; ++i) {
    if (working->item(i)->text().startsWith("+")) {
      working->item(i)->setSelected(true);
      ++selected;
    }
  }
  QCOMPARE(selected, 2);

  const QString stageText =
      dialog.findChild<QPushButton *>("stagingStageButton")->text();
  QVERIFY2(stageText.contains("2 lines"), qPrintable(stageText));
}

void TestStagingCanvasDialog::testStageSelectedLine() {
  writeFile("f.txt", "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n"
                     "new1\nnew2\nnew3\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QListWidget *working = column(dialog, "Working");

  for (int i = 0; i < working->count(); ++i) {
    if (working->item(i)->text() == "+new1") {
      working->item(i)->setSelected(true);
      break;
    }
  }

  dialog.findChild<QPushButton *>("stagingStageButton")->click();

  const QString staged = gitOut({"diff", "--cached"});
  QVERIFY2(staged.contains("+new1"), qPrintable(staged));
  QVERIFY2(!staged.contains("+new2"), qPrintable(staged));

  QVERIFY(readFile("f.txt").contains("new3"));
}

void TestStagingCanvasDialog::testStageHunkViaHeaderRow() {
  writeFile("f.txt",
            "CHANGED1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
            "l11\nl12\nl13\nl14\nl15\nl16\nl17\nCHANGED18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QListWidget *working = column(dialog, "Working");

  for (int i = 0; i < working->count(); ++i) {
    if (working->item(i)->text().contains("hunk 1 of 2")) {
      working->setCurrentRow(i);
      working->item(i)->setSelected(true);
      break;
    }
  }

  dialog.findChild<QPushButton *>("stagingStageButton")->click();

  const QString staged = gitOut({"diff", "--cached"});
  QVERIFY2(staged.contains("CHANGED1"), qPrintable(staged));
  QVERIFY2(!staged.contains("CHANGED18"), qPrintable(staged));
}

void TestStagingCanvasDialog::testUnstageDoesNotTouchWorkingTree() {
  writeFile("f.txt", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  QVERIFY(git({"add", "f.txt"}));

  StagingCanvasDialog dialog(m_git, Theme());

  dialog.findChild<QPushButton *>("stagingUnstageButton")->click();

  QVERIFY2(gitOut({"diff", "--cached"}).trimmed().isEmpty(),
           qPrintable(gitOut({"diff", "--cached"})));
  QVERIFY(readFile("f.txt").startsWith("CHANGED"));
}

void TestStagingCanvasDialog::testKeyboardStageAndUnstage() {
  writeFile("f.txt", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  dialog.show();

  QTest::keyClick(&dialog, Qt::Key_Right, Qt::ControlModifier);
  QVERIFY2(gitOut({"diff", "--cached"}).contains("CHANGED"),
           qPrintable(gitOut({"diff", "--cached"})));

  QTest::keyClick(&dialog, Qt::Key_Left, Qt::ControlModifier);
  QVERIFY2(gitOut({"diff", "--cached"}).trimmed().isEmpty(),
           qPrintable(gitOut({"diff", "--cached"})));

  QVERIFY(readFile("f.txt").startsWith("CHANGED"));
}

void TestStagingCanvasDialog::testWorkingHeadPairIsReadOnly() {
  writeFile("f.txt", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QComboBox *compare = dialog.findChild<QComboBox *>("stagingCompareSelector");
  QVERIFY(compare);
  compare->setCurrentIndex(2);

  QVERIFY(!dialog.findChild<QPushButton *>("stagingStageButton")->isEnabled());
  QVERIFY(
      !dialog.findChild<QPushButton *>("stagingDiscardButton")->isEnabled());
  QVERIFY(gitOut({"diff", "--cached"}).trimmed().isEmpty());
}

void TestStagingCanvasDialog::testComparePairSwitchesWorkingColumn() {
  writeFile("f.txt", "STAGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  QVERIFY(git({"add", "f.txt"}));
  writeFile("f.txt", "STAGED\nDIRTY\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                     "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  StagingCanvasDialog dialog(m_git, Theme());
  QComboBox *compare = dialog.findChild<QComboBox *>("stagingCompareSelector");
  compare->setCurrentIndex(2);

  QListWidget *working = column(dialog, "Working");
  QStringList text;
  for (int i = 0; i < working->count(); ++i) {
    text << working->item(i)->text();
  }

  QVERIFY2(text.join("\n").contains("+STAGED"), qPrintable(text.join("\n")));
  QVERIFY2(text.join("\n").contains("+DIRTY"), qPrintable(text.join("\n")));
}

void TestStagingCanvasDialog::testEmptyRepositoryState() {
  StagingCanvasDialog dialog(m_git, Theme());
  QTreeWidget *files = dialog.findChild<QTreeWidget *>("stagingFileTree");
  QCOMPARE(files->topLevelItemCount(), 0);

  QVERIFY(!dialog.findChild<QPushButton *>("stagingStageButton")->isEnabled());
  QVERIFY(
      !dialog.findChild<QPushButton *>("stagingUnstageButton")->isEnabled());
}

QTEST_MAIN(TestStagingCanvasDialog)
#include "test_stagingcanvasdialog.moc"
