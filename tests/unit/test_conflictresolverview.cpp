#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include "git/gitintegration.h"
#include "settings/theme.h"
#include "ui/panels/conflictresolverview.h"

class TestConflictResolverView : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testOneCardPerDisagreement();
  void testCardNamesBothBranches();
  void testHeaderCountsTheWholeFile();
  void testKeepingOneSideDecidesThatCardOnly();
  void testDecidedCardShowsTheOutcomeAndCanBeChanged();
  void testKeepBothOrdersAreOffered();
  void testKeepAllMineDecidesEverything();
  void testUndoAndRedoAreWiredToTheToolbar();
  void testFinishButtonWaitsForEveryDecision();
  void testMarkingDoneWritesAndStagesTheFile();
  void testSaveProgressKeepsTheFileUnresolved();
  void testLongContextIsFoldedAway();
  void testRawFileRequestIsForwarded();

private:
  QTemporaryDir m_tempDir;
  QString m_repoPath;
  GitIntegration *m_git = nullptr;
  Theme m_theme;

  bool git(const QStringList &args);
  void writeFile(const QString &name, const QString &content);
  void makeConflict(const QString &mainBody, const QString &featureBody);

  static QString body(const QString &first, const QString &second,
                      int filler = 10);
  QPushButton *buttonFor(ConflictResolverView &view, const QString &name,
                         int regionId) const;
};

QString TestConflictResolverView::body(const QString &first,
                                       const QString &second, int filler) {
  QStringList lines;
  lines << QStringLiteral("start") << first << QStringLiteral("end");
  for (int i = 0; i < filler; ++i) {
    lines << QStringLiteral("gap %1").arg(i);
  }
  lines << QStringLiteral("X") << second << QStringLiteral("Z");
  return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

bool TestConflictResolverView::git(const QStringList &args) {
  QProcess process;
  process.setWorkingDirectory(m_repoPath);
  process.start(QStringLiteral("git"), args);
  return process.waitForFinished(15000) && process.exitCode() == 0;
}

void TestConflictResolverView::writeFile(const QString &name,
                                         const QString &content) {
  QFile file(QDir(m_repoPath).filePath(name));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(content.toUtf8());
  file.close();
}

void TestConflictResolverView::init() {
  static int counter = 0;
  m_repoPath = m_tempDir.path() + "/repo" + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repoPath));
  QVERIFY(git({"init", "--initial-branch=main"}));
  QVERIFY(git({"config", "user.email", "ada@example.com"}));
  QVERIFY(git({"config", "user.name", "Ada"}));

  m_git = new GitIntegration;
}

void TestConflictResolverView::cleanup() {
  delete m_git;
  m_git = nullptr;
}

void TestConflictResolverView::makeConflict(const QString &mainBody,
                                            const QString &featureBody) {
  writeFile("f.txt", body(QStringLiteral("MIDDLE"), QStringLiteral("MIDDLE2")));
  QVERIFY(git({"add", "f.txt"}));
  QVERIFY(git({"commit", "-m", "base"}));

  QVERIFY(git({"checkout", "-b", "feature"}));
  writeFile("f.txt", featureBody);
  QVERIFY(git({"commit", "-am", "feature edit"}));

  QVERIFY(git({"checkout", "main"}));
  writeFile("f.txt", mainBody);
  QVERIFY(git({"commit", "-am", "main edit"}));

  git({"merge", "feature"});

  QVERIFY(m_git->setRepositoryPath(m_repoPath));
  QVERIFY(m_git->hasMergeConflicts());
}

QPushButton *TestConflictResolverView::buttonFor(ConflictResolverView &view,
                                                 const QString &name,
                                                 int regionId) const {
  for (QPushButton *button : view.findChildren<QPushButton *>(name)) {
    if (button->property("regionId").toInt() == regionId) {
      return button;
    }
  }
  return nullptr;
}

void TestConflictResolverView::testOneCardPerDisagreement() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MINE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("THEIRS2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QCOMPARE(view.totalConflicts(), 2);
  QCOMPARE(view.remainingConflicts(), 2);
  QCOMPARE(view.findChildren<QWidget *>(QStringLiteral("conflictCard")).size(),
           2);
}

void TestConflictResolverView::testCardNamesBothBranches() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MIDDLE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("MIDDLE2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QStringList allText;
  for (QLabel *label : view.findChildren<QLabel *>()) {
    allText << label->text();
  }
  const QString joined = allText.join(QLatin1Char('\n'));

  QVERIFY(joined.contains(QStringLiteral("YOUR VERSION")));
  QVERIFY(joined.contains(QStringLiteral("THEIR VERSION")));
  QVERIFY(joined.contains(QStringLiteral("feature")));

  QVERIFY(joined.contains(QStringLiteral("main")));
  QVERIFY(!joined.contains(QStringLiteral("HEAD")));

  QLabel *lineLabel =
      view.findChild<QLabel *>(QStringLiteral("conflictCardLineLabel"));
  QVERIFY(lineLabel);
  QVERIFY(lineLabel->text().contains(QStringLiteral("line")));
}

void TestConflictResolverView::testHeaderCountsTheWholeFile() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MINE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("THEIRS2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QLabel *count =
      view.findChild<QLabel *>(QStringLiteral("conflictCountLabel"));
  QProgressBar *progress =
      view.findChild<QProgressBar *>(QStringLiteral("conflictProgressBar"));
  QVERIFY(count);
  QVERIFY(progress);
  QCOMPARE(progress->maximum(), 2);
  QCOMPARE(progress->value(), 0);
  QVERIFY(count->text().contains(QStringLiteral("0 of 2")));
}

void TestConflictResolverView::testKeepingOneSideDecidesThatCardOnly() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MINE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("THEIRS2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QPushButton *keepOurs =
      buttonFor(view, QStringLiteral("conflictKeepOursButton"), 1);
  QVERIFY(keepOurs);
  keepOurs->click();

  QCOMPARE(view.decidedConflicts(), 1);
  QCOMPARE(view.remainingConflicts(), 1);
  QCOMPARE(view.resolver().region(1)->choice, ConflictChoice::Ours);
  QCOMPARE(view.resolver().region(2)->choice, ConflictChoice::Unresolved);
  QVERIFY(view.resolver().text().contains(QStringLiteral("MINE\n")));
  QVERIFY(!view.resolver().text().contains(QStringLiteral("THEIRS\n")));
}

void TestConflictResolverView::testDecidedCardShowsTheOutcomeAndCanBeChanged() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MIDDLE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("MIDDLE2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  buttonFor(view, QStringLiteral("conflictKeepTheirsButton"), 1)->click();

  QLabel *outcome =
      view.findChild<QLabel *>(QStringLiteral("conflictCardOutcomeLabel"));
  QVERIFY(outcome);
  QVERIFY(
      outcome->text().contains(conflictChoiceOutcome(ConflictChoice::Theirs)));

  QPushButton *reopen =
      view.findChild<QPushButton *>(QStringLiteral("conflictReopenButton"));
  QVERIFY(reopen);
  reopen->click();
  QCOMPARE(view.remainingConflicts(), 1);
  QCOMPARE(view.resolver().region(1)->choice, ConflictChoice::Unresolved);
}

void TestConflictResolverView::testKeepBothOrdersAreOffered() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MIDDLE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("MIDDLE2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QPushButton *bothMineFirst =
      buttonFor(view, QStringLiteral("conflictKeepBothOursFirstButton"), 1);
  QVERIFY(bothMineFirst);
  QVERIFY(
      buttonFor(view, QStringLiteral("conflictKeepBothTheirsFirstButton"), 1));
  QVERIFY(buttonFor(view, QStringLiteral("conflictKeepNeitherButton"), 1));
  QVERIFY(buttonFor(view, QStringLiteral("conflictWriteOwnButton"), 1));

  bothMineFirst->click();
  const QString text = view.resolver().text();
  QVERIFY(text.indexOf(QStringLiteral("MINE")) <
          text.indexOf(QStringLiteral("THEIRS")));
}

void TestConflictResolverView::testKeepAllMineDecidesEverything() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MINE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("THEIRS2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QPushButton *keepAll = view.findChild<QPushButton *>(
      QStringLiteral("conflictKeepAllOursButton"));
  QVERIFY(keepAll);
  keepAll->click();

  QCOMPARE(view.remainingConflicts(), 0);
  QVERIFY(!keepAll->isEnabled());
}

void TestConflictResolverView::testUndoAndRedoAreWiredToTheToolbar() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MIDDLE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("MIDDLE2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QPushButton *undo =
      view.findChild<QPushButton *>(QStringLiteral("conflictUndoButton"));
  QPushButton *redo =
      view.findChild<QPushButton *>(QStringLiteral("conflictRedoButton"));
  QVERIFY(undo);
  QVERIFY(redo);
  QVERIFY(!undo->isEnabled());
  QVERIFY(!redo->isEnabled());

  buttonFor(view, QStringLiteral("conflictKeepOursButton"), 1)->click();
  QVERIFY(undo->isEnabled());
  QVERIFY(
      undo->toolTip().contains(conflictChoiceOutcome(ConflictChoice::Ours)));

  undo->click();
  QCOMPARE(view.remainingConflicts(), 1);
  QVERIFY(redo->isEnabled());

  redo->click();
  QCOMPARE(view.remainingConflicts(), 0);
}

void TestConflictResolverView::testFinishButtonWaitsForEveryDecision() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MINE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("THEIRS2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QPushButton *done =
      view.findChild<QPushButton *>(QStringLiteral("conflictDoneButton"));
  QVERIFY(done);
  QVERIFY(!done->isEnabled());
  QVERIFY(done->toolTip().contains(QStringLiteral("still need")));

  buttonFor(view, QStringLiteral("conflictKeepOursButton"), 1)->click();
  QVERIFY(!done->isEnabled());

  buttonFor(view, QStringLiteral("conflictKeepOursButton"), 2)->click();
  QVERIFY(done->isEnabled());
}

void TestConflictResolverView::testMarkingDoneWritesAndStagesTheFile() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MIDDLE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("MIDDLE2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QSignalSpy spy(&view, &ConflictResolverView::fileResolved);
  buttonFor(view, QStringLiteral("conflictKeepTheirsButton"), 1)->click();
  QVERIFY(view.markFileDone());
  QCOMPARE(spy.count(), 1);

  QVERIFY(!m_git->hasMergeConflicts());

  QFile file(QDir(m_repoPath).filePath("f.txt"));
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QString content = QString::fromUtf8(file.readAll());
  QVERIFY(content.startsWith(QStringLiteral("start\nTHEIRS\nend\n")));
  QVERIFY(!content.contains(QStringLiteral("MINE")));
  QVERIFY(!content.contains(QStringLiteral("<<<<<<<")));
  QVERIFY(!content.contains(QStringLiteral(">>>>>>>")));
}

void TestConflictResolverView::testSaveProgressKeepsTheFileUnresolved() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MINE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("THEIRS2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  buttonFor(view, QStringLiteral("conflictKeepOursButton"), 1)->click();
  QVERIFY(view.saveProgress());

  QVERIFY(m_git->hasMergeConflicts());
  QVERIFY(!view.markFileDone());

  QFile file(QDir(m_repoPath).filePath("f.txt"));
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QString content = QString::fromUtf8(file.readAll());
  QVERIFY(content.contains(QStringLiteral("MINE\n")));
  QVERIFY(content.contains(QStringLiteral("<<<<<<<")));
}

void TestConflictResolverView::testLongContextIsFoldedAway() {

  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MINE2"), 40),
               body(QStringLiteral("THEIRS"), QStringLiteral("THEIRS2"), 40));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QPushButton *toggle =
      view.findChild<QPushButton *>(QStringLiteral("conflictContextToggle"));
  QVERIFY(toggle);
  QVERIFY(toggle->text().contains(QStringLiteral("unchanged")));

  const int before =
      view.findChildren<QPlainTextEdit *>(QStringLiteral("conflictCodeBlock"))
          .size();
  toggle->click();
  const int after =
      view.findChildren<QPlainTextEdit *>(QStringLiteral("conflictCodeBlock"))
          .size();
  QVERIFY(after > before);
}

void TestConflictResolverView::testRawFileRequestIsForwarded() {
  makeConflict(body(QStringLiteral("MINE"), QStringLiteral("MIDDLE2")),
               body(QStringLiteral("THEIRS"), QStringLiteral("MIDDLE2")));

  ConflictResolverView view(m_git, QStringLiteral("f.txt"));
  view.applyTheme(m_theme);

  QSignalSpy spy(&view, &ConflictResolverView::rawFileRequested);
  view.findChild<QPushButton *>(QStringLiteral("conflictRawButton"))->click();
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.first().first().toString(), view.absolutePath());
}

QTEST_MAIN(TestConflictResolverView)
#include "test_conflictresolverview.moc"
