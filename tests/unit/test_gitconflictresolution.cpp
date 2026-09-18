#include "git/gitconflictresolution.h"
#include <QtTest/QtTest>

class TestGitConflictResolution : public QObject {
  Q_OBJECT

private slots:
  void testCleanTextHasNoConflicts();
  void testParsesASingleConflict();
  void testUntouchedResolverRoundTrips();
  void testUntouchedResolverRoundTripsWithoutTrailingNewline();
  void testParsesDiff3Base();
  void testParsesSeveralConflicts();
  void testEqualsSignsInProseAreNotSeparators();
  void testUnterminatedMarkerIsLeftAlone();
  void testKeepingOursDropsTheirs();
  void testKeepingTheirsDropsOurs();
  void testKeepingBothOrders();
  void testKeepingNeitherRemovesTheRegion();
  void testCustomTextReplacesTheRegion();
  void testBaseIsOnlyOfferedWhenItExists();
  void testResolveAllSettlesEveryRegion();
  void testCountsTrackProgress();
  void testUndoTakesBackOneDecision();
  void testRedoReappliesADecision();
  void testNewDecisionClearsRedo();
  void testUndoDescriptionNamesTheConflict();
  void testReopeningAConflictBringsMarkersBack();
  void testLineNumbersFollowResolutions();
  void testDocumentItemsFoldLongContext();
  void testDocumentItemsKeepShortContextVisible();
  void testWholeSideReconstruction();
  void testLabelsAreExposed();

private:
  static QString twoWayConflict();
  static QString diff3Conflict();
};

QString TestGitConflictResolution::twoWayConflict() {
  return QStringLiteral("before\n"
                        "<<<<<<< HEAD\n"
                        "mine one\n"
                        "mine two\n"
                        "=======\n"
                        "theirs one\n"
                        ">>>>>>> feature/login\n"
                        "after\n");
}

QString TestGitConflictResolution::diff3Conflict() {
  return QStringLiteral("head\n"
                        "<<<<<<< HEAD\n"
                        "mine\n"
                        "||||||| merged common ancestors\n"
                        "original\n"
                        "=======\n"
                        "theirs\n"
                        ">>>>>>> other\n"
                        "tail\n");
}

void TestGitConflictResolution::testCleanTextHasNoConflicts() {
  ConflictFileResolver resolver;
  QVERIFY(!resolver.load(QStringLiteral("just\nplain\ntext\n")));
  QVERIFY(!resolver.hasConflicts());
  QCOMPARE(resolver.totalConflicts(), 0);
  QCOMPARE(resolver.text(), QStringLiteral("just\nplain\ntext\n"));
  QVERIFY(!textHasConflictMarkers(QStringLiteral("just\nplain\ntext\n")));
}

void TestGitConflictResolution::testParsesASingleConflict() {
  ConflictFileResolver resolver;
  QVERIFY(resolver.load(twoWayConflict()));
  QCOMPARE(resolver.totalConflicts(), 1);

  const ConflictRegion *region = resolver.region(1);
  QVERIFY(region);
  QCOMPARE(region->oursLines, QStringList() << "mine one" << "mine two");
  QCOMPARE(region->theirsLines, QStringList() << "theirs one");
  QCOMPARE(region->oursLabel, QStringLiteral("HEAD"));
  QCOMPARE(region->theirsLabel, QStringLiteral("feature/login"));
  QVERIFY(!region->hasBase);
  QVERIFY(!region->resolved());
}

void TestGitConflictResolution::testUntouchedResolverRoundTrips() {
  ConflictFileResolver resolver;
  QVERIFY(resolver.load(twoWayConflict()));
  QCOMPARE(resolver.text(), twoWayConflict());
}

void TestGitConflictResolution::
    testUntouchedResolverRoundTripsWithoutTrailingNewline() {
  QString content = twoWayConflict();
  content.chop(1);
  ConflictFileResolver resolver;
  QVERIFY(resolver.load(content));
  QCOMPARE(resolver.text(), content);
}

void TestGitConflictResolution::testParsesDiff3Base() {
  ConflictFileResolver resolver;
  QVERIFY(resolver.load(diff3Conflict()));

  const ConflictRegion *region = resolver.region(1);
  QVERIFY(region);
  QVERIFY(region->hasBase);
  QCOMPARE(region->baseLines, QStringList() << "original");
  QCOMPARE(region->oursLines, QStringList() << "mine");
  QCOMPARE(region->theirsLines, QStringList() << "theirs");
  QCOMPARE(resolver.text(), diff3Conflict());
}

void TestGitConflictResolution::testParsesSeveralConflicts() {
  const QString content = QStringLiteral("a\n"
                                         "<<<<<<< HEAD\n"
                                         "one mine\n"
                                         "=======\n"
                                         "one theirs\n"
                                         ">>>>>>> other\n"
                                         "b\n"
                                         "<<<<<<< HEAD\n"
                                         "two mine\n"
                                         "=======\n"
                                         "two theirs\n"
                                         ">>>>>>> other\n"
                                         "c\n");
  ConflictFileResolver resolver;
  QVERIFY(resolver.load(content));
  QCOMPARE(resolver.totalConflicts(), 2);
  QCOMPARE(resolver.region(1)->oursLines, QStringList() << "one mine");
  QCOMPARE(resolver.region(2)->theirsLines, QStringList() << "two theirs");
  QCOMPARE(resolver.text(), content);
}

void TestGitConflictResolution::testEqualsSignsInProseAreNotSeparators() {
  const QString content = QStringLiteral("Title\n=====\nbody\n");
  ConflictFileResolver resolver;
  QVERIFY(!resolver.load(content));
  QCOMPARE(resolver.text(), content);
}

void TestGitConflictResolution::testUnterminatedMarkerIsLeftAlone() {
  const QString content = QStringLiteral("a\n<<<<<<< HEAD\nmine\nb\n");
  ConflictFileResolver resolver;
  QVERIFY(!resolver.load(content));
  QCOMPARE(resolver.totalConflicts(), 0);
  QCOMPARE(resolver.text(), content);
}

void TestGitConflictResolution::testKeepingOursDropsTheirs() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  QVERIFY(resolver.resolve(1, ConflictChoice::Ours));
  QCOMPARE(resolver.text(),
           QStringLiteral("before\nmine one\nmine two\nafter\n"));
}

void TestGitConflictResolution::testKeepingTheirsDropsOurs() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  QVERIFY(resolver.resolve(1, ConflictChoice::Theirs));
  QCOMPARE(resolver.text(), QStringLiteral("before\ntheirs one\nafter\n"));
}

void TestGitConflictResolution::testKeepingBothOrders() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());

  QVERIFY(resolver.resolve(1, ConflictChoice::BothOursFirst));
  QCOMPARE(resolver.text(),
           QStringLiteral("before\nmine one\nmine two\ntheirs one\nafter\n"));

  QVERIFY(resolver.resolve(1, ConflictChoice::BothTheirsFirst));
  QCOMPARE(resolver.text(),
           QStringLiteral("before\ntheirs one\nmine one\nmine two\nafter\n"));
}

void TestGitConflictResolution::testKeepingNeitherRemovesTheRegion() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  QVERIFY(resolver.resolve(1, ConflictChoice::Neither));
  QCOMPARE(resolver.text(), QStringLiteral("before\nafter\n"));
}

void TestGitConflictResolution::testCustomTextReplacesTheRegion() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  QVERIFY(resolver.resolve(1, ConflictChoice::Custom,
                           QStringList() << "hand written"));
  QCOMPARE(resolver.text(), QStringLiteral("before\nhand written\nafter\n"));
  QCOMPARE(resolver.remainingCount(), 0);
}

void TestGitConflictResolution::testBaseIsOnlyOfferedWhenItExists() {
  ConflictFileResolver twoWay;
  twoWay.load(twoWayConflict());
  QVERIFY(!twoWay.resolve(1, ConflictChoice::Base));
  QCOMPARE(twoWay.remainingCount(), 1);

  ConflictFileResolver diff3;
  diff3.load(diff3Conflict());
  QVERIFY(diff3.resolve(1, ConflictChoice::Base));
  QCOMPARE(diff3.text(), QStringLiteral("head\noriginal\ntail\n"));
}

void TestGitConflictResolution::testResolveAllSettlesEveryRegion() {
  const QString content = QStringLiteral("<<<<<<< HEAD\nA\n=======\nB\n>>>>>>> "
                                         "x\nmid\n<<<<<<< HEAD\nC\n=======\nD\n"
                                         ">>>>>>> x\n");
  ConflictFileResolver resolver;
  resolver.load(content);
  QVERIFY(resolver.resolveAll(ConflictChoice::Theirs));
  QCOMPARE(resolver.remainingCount(), 0);
  QCOMPARE(resolver.text(), QStringLiteral("B\nmid\nD\n"));

  QVERIFY(!resolver.resolveAll(ConflictChoice::Theirs));
}

void TestGitConflictResolution::testCountsTrackProgress() {
  const QString content = QStringLiteral("<<<<<<< HEAD\nA\n=======\nB\n>>>>>>> "
                                         "x\nmid\n<<<<<<< HEAD\nC\n=======\nD\n"
                                         ">>>>>>> x\n");
  ConflictFileResolver resolver;
  resolver.load(content);
  QCOMPARE(resolver.totalConflicts(), 2);
  QCOMPARE(resolver.resolvedCount(), 0);
  QCOMPARE(resolver.remainingCount(), 2);

  resolver.resolve(2, ConflictChoice::Ours);
  QCOMPARE(resolver.resolvedCount(), 1);
  QCOMPARE(resolver.remainingCount(), 1);

  QCOMPARE(resolver.totalConflicts(), 2);
}

void TestGitConflictResolution::testUndoTakesBackOneDecision() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  QVERIFY(!resolver.canUndo());

  resolver.resolve(1, ConflictChoice::Ours);
  QVERIFY(resolver.canUndo());
  QVERIFY(resolver.undo());
  QCOMPARE(resolver.text(), twoWayConflict());
  QCOMPARE(resolver.remainingCount(), 1);
  QVERIFY(!resolver.canUndo());
}

void TestGitConflictResolution::testRedoReappliesADecision() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  resolver.resolve(1, ConflictChoice::Theirs);
  resolver.undo();

  QVERIFY(resolver.canRedo());
  QVERIFY(resolver.redo());
  QCOMPARE(resolver.text(), QStringLiteral("before\ntheirs one\nafter\n"));
  QVERIFY(!resolver.canRedo());
}

void TestGitConflictResolution::testNewDecisionClearsRedo() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  resolver.resolve(1, ConflictChoice::Theirs);
  resolver.undo();
  QVERIFY(resolver.canRedo());

  resolver.resolve(1, ConflictChoice::Ours);
  QVERIFY(!resolver.canRedo());
}

void TestGitConflictResolution::testUndoDescriptionNamesTheConflict() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  resolver.resolve(1, ConflictChoice::Theirs);

  const QString description = resolver.undoDescription();
  QVERIFY(description.contains(QStringLiteral("1")));
  QVERIFY(description.contains(conflictChoiceOutcome(ConflictChoice::Theirs)));
  QCOMPARE(resolver.history().size(), 1);
}

void TestGitConflictResolution::testReopeningAConflictBringsMarkersBack() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  resolver.resolve(1, ConflictChoice::Ours);
  QVERIFY(resolver.reopen(1));
  QCOMPARE(resolver.text(), twoWayConflict());
  QVERIFY(!resolver.reopen(1));
}

void TestGitConflictResolution::testLineNumbersFollowResolutions() {
  const QString content = QStringLiteral("a\n"
                                         "<<<<<<< HEAD\n"
                                         "one mine\n"
                                         "=======\n"
                                         "one theirs\n"
                                         ">>>>>>> other\n"
                                         "b\n"
                                         "<<<<<<< HEAD\n"
                                         "two mine\n"
                                         "=======\n"
                                         "two theirs\n"
                                         ">>>>>>> other\n");
  ConflictFileResolver resolver;
  resolver.load(content);
  QCOMPARE(resolver.lineOf(1), 2);
  QCOMPARE(resolver.lineOf(2), 8);

  resolver.resolve(1, ConflictChoice::Ours);
  QCOMPARE(resolver.lineOf(1), 2);
  QCOMPARE(resolver.lineOf(2), 4);
  QCOMPARE(resolver.lineOf(99), 0);
}

void TestGitConflictResolution::testDocumentItemsFoldLongContext() {
  QStringList lines;
  for (int i = 0; i < 40; ++i) {
    lines << QStringLiteral("line %1").arg(i);
  }
  const QString content =
      lines.join(QLatin1Char('\n')) +
      QStringLiteral("\n<<<<<<< HEAD\nA\n=======\nB\n>>>>>>>"
                     " x\n") +
      lines.join(QLatin1Char('\n')) + QStringLiteral("\n");

  ConflictFileResolver resolver;
  resolver.load(content);

  const QList<ConflictDocumentItem> items = resolver.documentItems(3);
  QCOMPARE(items.size(), 3);
  QCOMPARE(items.at(0).kind, ConflictDocumentItem::Kind::Context);
  QCOMPARE(items.at(1).kind, ConflictDocumentItem::Kind::Conflict);
  QCOMPARE(items.at(1).regionId, 1);

  QCOMPARE(items.at(0).trailingLines.size(), 3);
  QCOMPARE(items.at(0).leadingLines.size(), 0);
  QCOMPARE(items.at(0).hiddenCount(), 37);
  QCOMPARE(items.at(0).lineCount(), 40);

  QCOMPARE(items.at(2).leadingLines.size(), 3);
  QCOMPARE(items.at(2).trailingLines.size(), 0);
  QCOMPARE(items.at(2).hiddenCount(), 37);
}

void TestGitConflictResolution::testDocumentItemsKeepShortContextVisible() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());

  const QList<ConflictDocumentItem> items = resolver.documentItems(3);
  QCOMPARE(items.size(), 3);
  QCOMPARE(items.at(0).hiddenCount(), 0);
  QCOMPARE(items.at(0).leadingLines, QStringList() << "before");
  QCOMPARE(items.at(2).hiddenCount(), 0);
  QCOMPARE(items.at(2).leadingLines, QStringList() << "after");
  QCOMPARE(items.at(1).startLine, 2);
}

void TestGitConflictResolution::testWholeSideReconstruction() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());

  QCOMPARE(resolver.allOursLines(),
           QStringList() << "before" << "mine one" << "mine two" << "after");
  QCOMPARE(resolver.allTheirsLines(),
           QStringList() << "before" << "theirs one" << "after");
}

void TestGitConflictResolution::testLabelsAreExposed() {
  ConflictFileResolver resolver;
  resolver.load(twoWayConflict());
  QCOMPARE(resolver.oursLabel(), QStringLiteral("HEAD"));
  QCOMPARE(resolver.theirsLabel(), QStringLiteral("feature/login"));
  QCOMPARE(countConflictMarkers(twoWayConflict()), 1);
}

QTEST_MAIN(TestGitConflictResolution)
#include "test_gitconflictresolution.moc"
