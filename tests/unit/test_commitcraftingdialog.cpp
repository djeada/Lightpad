#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/dialogs/commitcraftingdialog.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTreeWidget>
#include <QtTest/QtTest>

class TestCommitCraftingDialog : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testListsEveryHunkAsUnassigned();
  void testNewBucketStartsEmpty();
  void testAssignMovesChangeToBucket();
  void testReturnPutsChangeBack();
  void testCommitButtonNeedsChangesAndMessage();
  void testCommitOnlyIncludesTheBucket();
  void testCommitLeavesOtherChangesInWorkingTree();
  void testUntrackedFileCanBeCommitted();
  void testPromptsAreShownNotApplied();
  void testKeyboardAssignAndReturn();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;

  bool git(const QStringList &args);
  QString gitOut(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  QTreeWidgetItem *firstChange(QTreeWidget *tree);
};

void TestCommitCraftingDialog::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "dev@example.com"}));
  QVERIFY(git({"config", "user.name", "Dev"}));

  writeFile("src.cpp", "l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                       "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  QVERIFY(git({"add", "src.cpp"}));
  QVERIFY(git({"commit", "-m", "base"}));

  m_git = new GitIntegration;
  QVERIFY(m_git->setRepositoryPath(m_repoPath));
}

void TestCommitCraftingDialog::cleanup() {
  delete m_git;
  m_git = nullptr;
}

bool TestCommitCraftingDialog::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    return false;
  }
  return process.exitCode() == 0;
}

QString TestCommitCraftingDialog::gitOut(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start("git", args);
  process.waitForFinished(GIT_COMMAND_TIMEOUT_MS);
  return QString::fromUtf8(process.readAllStandardOutput());
}

void TestCommitCraftingDialog::writeFile(const QString &name,
                                         const QString &content) {
  QFile file(m_repoPath + "/" + name);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(content.toUtf8());
  file.close();
}

QTreeWidgetItem *TestCommitCraftingDialog::firstChange(QTreeWidget *tree) {
  for (QTreeWidgetItemIterator it(tree); *it; ++it) {
    if ((*it)->data(0, Qt::UserRole + 8).toBool()) {
      return *it;
    }
  }
  return nullptr;
}

void TestCommitCraftingDialog::testListsEveryHunkAsUnassigned() {
  writeFile("src.cpp",
            "CHANGED1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
            "l11\nl12\nl13\nl14\nl15\nl16\nl17\nCHANGED18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  QCOMPARE(dialog.plan().available().size(), 2);
  QCOMPARE(dialog.plan().unassigned().size(), 2);

  QLabel *label = dialog.findChild<QLabel *>("craftUnassignedLabel");
  QVERIFY2(label->text().contains("(2)"), qPrintable(label->text()));
}

void TestCommitCraftingDialog::testNewBucketStartsEmpty() {
  writeFile("src.cpp", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                       "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  dialog.findChild<QPushButton *>("craftAddBucketButton")->click();

  QCOMPARE(dialog.plan().buckets().size(), 1);
  QVERIFY(dialog.plan().buckets().first().changes.isEmpty());

  QVERIFY(gitOut({"diff", "--cached"}).trimmed().isEmpty());
}

void TestCommitCraftingDialog::testAssignMovesChangeToBucket() {
  writeFile("src.cpp",
            "CHANGED1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
            "l11\nl12\nl13\nl14\nl15\nl16\nl17\nCHANGED18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  dialog.findChild<QPushButton *>("craftAddBucketButton")->click();

  QTreeWidget *unassigned =
      dialog.findChild<QTreeWidget *>("craftUnassignedTree");
  firstChange(unassigned)->setSelected(true);
  dialog.findChild<QPushButton *>("craftAssignButton")->click();

  QCOMPARE(dialog.plan().buckets().first().changes.size(), 1);
  QCOMPARE(dialog.plan().unassigned().size(), 1);
}

void TestCommitCraftingDialog::testReturnPutsChangeBack() {
  writeFile("src.cpp", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                       "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  dialog.findChild<QPushButton *>("craftAddBucketButton")->click();

  QTreeWidget *unassigned =
      dialog.findChild<QTreeWidget *>("craftUnassignedTree");
  firstChange(unassigned)->setSelected(true);
  dialog.findChild<QPushButton *>("craftAssignButton")->click();
  QCOMPARE(dialog.plan().unassigned().size(), 0);

  QTreeWidget *buckets = dialog.findChild<QTreeWidget *>("craftBucketTree");
  firstChange(buckets)->setSelected(true);
  dialog.findChild<QPushButton *>("craftReturnButton")->click();

  QCOMPARE(dialog.plan().unassigned().size(), 1);
}

void TestCommitCraftingDialog::testCommitButtonNeedsChangesAndMessage() {
  writeFile("src.cpp", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                       "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  QPushButton *commit = dialog.findChild<QPushButton *>("craftCommitButton");
  QVERIFY(!commit->isEnabled());

  dialog.findChild<QPushButton *>("craftAddBucketButton")->click();
  QVERIFY(!commit->isEnabled());

  QTreeWidget *unassigned =
      dialog.findChild<QTreeWidget *>("craftUnassignedTree");
  firstChange(unassigned)->setSelected(true);
  dialog.findChild<QPushButton *>("craftAssignButton")->click();

  QVERIFY(!commit->isEnabled());

  dialog.findChild<QTextEdit *>("craftMessageEdit")->setPlainText("Fix it");
  QVERIFY(commit->isEnabled());
}

void TestCommitCraftingDialog::testCommitOnlyIncludesTheBucket() {
  writeFile("src.cpp",
            "CHANGED1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
            "l11\nl12\nl13\nl14\nl15\nl16\nl17\nCHANGED18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  dialog.findChild<QPushButton *>("craftAddBucketButton")->click();

  QTreeWidget *unassigned =
      dialog.findChild<QTreeWidget *>("craftUnassignedTree");
  firstChange(unassigned)->setSelected(true);
  dialog.findChild<QPushButton *>("craftAssignButton")->click();
  dialog.findChild<QTextEdit *>("craftMessageEdit")
      ->setPlainText("First region only");

  QTimer::singleShot(0, [] {
    for (QWidget *widget : QApplication::topLevelWidgets()) {
      if (auto *box = qobject_cast<QDialog *>(widget)) {
        if (box->isVisible() && box->objectName() != "CommitCraftingDialog") {
          QList<QPushButton *> buttons = box->findChildren<QPushButton *>();
          for (QPushButton *button : buttons) {
            if (button->text().contains("Yes")) {
              button->click();
              return;
            }
          }
        }
      }
    }
  });
  dialog.findChild<QPushButton *>("craftCommitButton")->click();

  const QString committed = gitOut({"show", "--format=%s", "HEAD"});
  QVERIFY2(committed.contains("First region only"), qPrintable(committed));
  QVERIFY2(committed.contains("CHANGED1"), qPrintable(committed));
  QVERIFY2(!committed.contains("CHANGED18"), qPrintable(committed));
}

void TestCommitCraftingDialog::testCommitLeavesOtherChangesInWorkingTree() {
  writeFile("src.cpp",
            "CHANGED1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
            "l11\nl12\nl13\nl14\nl15\nl16\nl17\nCHANGED18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  dialog.findChild<QPushButton *>("craftAddBucketButton")->click();
  QTreeWidget *unassigned =
      dialog.findChild<QTreeWidget *>("craftUnassignedTree");
  firstChange(unassigned)->setSelected(true);
  dialog.findChild<QPushButton *>("craftAssignButton")->click();
  dialog.findChild<QTextEdit *>("craftMessageEdit")->setPlainText("Region one");

  QTimer::singleShot(0, [] {
    for (QWidget *widget : QApplication::topLevelWidgets()) {
      if (auto *box = qobject_cast<QDialog *>(widget)) {
        if (box->isVisible()) {
          for (QPushButton *button : box->findChildren<QPushButton *>()) {
            if (button->text().contains("Yes")) {
              button->click();
              return;
            }
          }
        }
      }
    }
  });
  dialog.findChild<QPushButton *>("craftCommitButton")->click();

  QFile file(m_repoPath + "/src.cpp");
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QString content = QString::fromUtf8(file.readAll());

  QVERIFY(content.contains("CHANGED1"));
  QVERIFY(content.contains("CHANGED18"));
  QVERIFY2(gitOut({"diff"}).contains("CHANGED18"),
           qPrintable(gitOut({"diff"})));
}

void TestCommitCraftingDialog::testUntrackedFileCanBeCommitted() {
  writeFile("brand-new.txt", "hello\n");

  CommitCraftingDialog dialog(m_git, Theme());
  QCOMPARE(dialog.plan().available().size(), 1);
  QCOMPARE(dialog.plan().available().first().kind,
           CommitChangeRef::Kind::UntrackedFile);
}

void TestCommitCraftingDialog::testPromptsAreShownNotApplied() {
  writeFile("src.cpp", "  l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                       "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");
  writeFile("package-lock.json", "{}\n");

  CommitCraftingDialog dialog(m_git, Theme());

  QStringList prompts;
  for (const ReviewPrompt &prompt : dialog.reviewPrompts()) {
    prompts << prompt.text;
  }
  QVERIFY2(prompts.join("\n").contains("generated"),
           qPrintable(prompts.join("\n")));

  QVERIFY(dialog.plan().buckets().isEmpty());
  QVERIFY(gitOut({"diff", "--cached"}).trimmed().isEmpty());
}

void TestCommitCraftingDialog::testKeyboardAssignAndReturn() {
  writeFile("src.cpp", "CHANGED\nl2\nl3\nl4\nl5\nl6\nl7\nl8\nl9\nl10\n"
                       "l11\nl12\nl13\nl14\nl15\nl16\nl17\nl18\nl19\nl20\n");

  CommitCraftingDialog dialog(m_git, Theme());
  dialog.show();
  dialog.findChild<QPushButton *>("craftAddBucketButton")->click();

  QTreeWidget *unassigned =
      dialog.findChild<QTreeWidget *>("craftUnassignedTree");
  firstChange(unassigned)->setSelected(true);

  QTest::keyClick(&dialog, Qt::Key_Right, Qt::ControlModifier);
  QCOMPARE(dialog.plan().buckets().first().changes.size(), 1);

  QTreeWidget *buckets = dialog.findChild<QTreeWidget *>("craftBucketTree");
  firstChange(buckets)->setSelected(true);
  QTest::keyClick(&dialog, Qt::Key_Left, Qt::ControlModifier);
  QCOMPARE(dialog.plan().unassigned().size(), 1);
}

QTEST_MAIN(TestCommitCraftingDialog)
#include "test_commitcraftingdialog.moc"
