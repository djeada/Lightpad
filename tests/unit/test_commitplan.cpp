#include "git/commitplan.h"
#include <QtTest/QtTest>

namespace {

GitHunk makeHunk(const QStringList &spec) {

  GitHunk hunk;
  for (const QString &entry : spec) {
    GitDiffLine line;
    const QChar marker = entry.at(0);
    line.text = entry.mid(1);
    if (marker == '+') {
      line.type = GitDiffLineType::Added;
    } else if (marker == '-') {
      line.type = GitDiffLineType::Removed;
    } else {
      line.type = GitDiffLineType::Context;
    }
    hunk.lines.append(line);
  }
  return hunk;
}

CommitChangeRef makeChange(const QString &path, int hunkIndex,
                           const QString &fingerprint, int additions = 1,
                           int deletions = 0) {
  CommitChangeRef change;
  change.kind = CommitChangeRef::Kind::Hunk;
  change.filePath = path;
  change.hunkIndex = hunkIndex;
  change.fingerprint = fingerprint;
  change.additions = additions;
  change.deletions = deletions;
  return change;
}

} // namespace

class TestCommitPlan : public QObject {
  Q_OBJECT

private slots:
  void testEverythingStartsUnassigned();
  void testAssignMovesOutOfUnassigned();
  void testAssignToSecondBucketMovesIt();
  void testUnassignReturnsIt();
  void testRemoveBucketReturnsItsChanges();
  void testBucketStats();
  void testEditedFileDropsStaleAssignment();
  void testMovedHunkKeepsAssignment();
  void testRenameAndMessage();

  void testFingerprintIgnoresLineNumbers();
  void testFingerprintDiffersOnContent();
  void testWhitespaceOnlyHunk();
  void testWhitespaceDetectionIgnoresRealChanges();
  void testLooksGenerated();
  void testLooksLikeTestOf();

  void testPromptsFlagWhitespace();
  void testPromptsFlagGeneratedFiles();
  void testPromptsSuggestCoupledChanges();
  void testPromptsStaySilentWhenNothingIsOdd();
};

void TestCommitPlan::testEverythingStartsUnassigned() {
  CommitPlan plan;
  plan.setAvailableChanges(
      {makeChange("a.cpp", 0, "aa"), makeChange("b.cpp", 0, "bb")});
  QCOMPARE(plan.unassigned().size(), 2);
  QVERIFY(plan.buckets().isEmpty());
}

void TestCommitPlan::testAssignMovesOutOfUnassigned() {
  CommitPlan plan;
  const CommitChangeRef change = makeChange("a.cpp", 0, "aa");
  plan.setAvailableChanges({change, makeChange("b.cpp", 0, "bb")});

  const int bucket = plan.addBucket("bug fix");
  plan.assign(bucket, change);

  QCOMPARE(plan.unassigned().size(), 1);
  QCOMPARE(plan.buckets().at(bucket).changes.size(), 1);
  QCOMPARE(plan.bucketOf(change), bucket);
}

void TestCommitPlan::testAssignToSecondBucketMovesIt() {
  CommitPlan plan;
  const CommitChangeRef change = makeChange("a.cpp", 0, "aa");
  plan.setAvailableChanges({change});

  const int first = plan.addBucket("one");
  const int second = plan.addBucket("two");
  plan.assign(first, change);
  plan.assign(second, change);

  QCOMPARE(plan.buckets().at(first).changes.size(), 0);
  QCOMPARE(plan.buckets().at(second).changes.size(), 1);
  QCOMPARE(plan.bucketOf(change), second);
}

void TestCommitPlan::testUnassignReturnsIt() {
  CommitPlan plan;
  const CommitChangeRef change = makeChange("a.cpp", 0, "aa");
  plan.setAvailableChanges({change});
  const int bucket = plan.addBucket("one");
  plan.assign(bucket, change);

  plan.unassign(change);
  QCOMPARE(plan.unassigned().size(), 1);
  QCOMPARE(plan.bucketOf(change), -1);
}

void TestCommitPlan::testRemoveBucketReturnsItsChanges() {
  CommitPlan plan;
  const CommitChangeRef change = makeChange("a.cpp", 0, "aa");
  plan.setAvailableChanges({change});
  const int bucket = plan.addBucket("one");
  plan.assign(bucket, change);

  plan.removeBucket(bucket);
  QVERIFY(plan.buckets().isEmpty());
  QCOMPARE(plan.unassigned().size(), 1);
}

void TestCommitPlan::testBucketStats() {
  CommitPlan plan;
  plan.setAvailableChanges(
      {makeChange("a.cpp", 0, "aa", 3, 1), makeChange("b.cpp", 0, "bb", 2, 5)});
  const int bucket = plan.addBucket("one");
  plan.assign(bucket, makeChange("a.cpp", 0, "aa", 3, 1));
  plan.assign(bucket, makeChange("b.cpp", 0, "bb", 2, 5));

  QCOMPARE(plan.buckets().at(bucket).additions(), 5);
  QCOMPARE(plan.buckets().at(bucket).deletions(), 6);
}

void TestCommitPlan::testEditedFileDropsStaleAssignment() {
  CommitPlan plan;
  const CommitChangeRef original = makeChange("a.cpp", 0, "aa");
  plan.setAvailableChanges({original});
  const int bucket = plan.addBucket("one");
  plan.assign(bucket, original);

  plan.setAvailableChanges({makeChange("a.cpp", 0, "zz")});

  QVERIFY(plan.buckets().at(bucket).changes.isEmpty());
  QCOMPARE(plan.staleAssignments().size(), 1);
  QCOMPARE(plan.staleAssignments().first().filePath, QString("a.cpp"));
  QCOMPARE(plan.unassigned().size(), 1);
}

void TestCommitPlan::testMovedHunkKeepsAssignment() {
  CommitPlan plan;
  const CommitChangeRef original = makeChange("a.cpp", 1, "aa");
  plan.setAvailableChanges({makeChange("a.cpp", 0, "xx"), original});
  const int bucket = plan.addBucket("one");
  plan.assign(bucket, original);

  plan.setAvailableChanges({makeChange("a.cpp", 0, "aa")});

  QCOMPARE(plan.buckets().at(bucket).changes.size(), 1);
  QCOMPARE(plan.buckets().at(bucket).changes.first().hunkIndex, 0);
  QVERIFY(plan.staleAssignments().isEmpty());
}

void TestCommitPlan::testRenameAndMessage() {
  CommitPlan plan;
  const int bucket = plan.addBucket("one");
  plan.renameBucket(bucket, "bug fix");
  plan.setMessage(bucket, "Fix the login redirect");

  QCOMPARE(plan.buckets().at(bucket).name, QString("bug fix"));
  QCOMPARE(plan.buckets().at(bucket).message,
           QString("Fix the login redirect"));
}

void TestCommitPlan::testFingerprintIgnoresLineNumbers() {
  GitHunk first = makeHunk({" ctx", "-old", "+new"});
  first.oldStart = 10;
  first.newStart = 10;
  GitHunk second = makeHunk({" ctx", "-old", "+new"});
  second.oldStart = 90;
  second.newStart = 92;

  QCOMPARE(hunkFingerprint(first), hunkFingerprint(second));
}

void TestCommitPlan::testFingerprintDiffersOnContent() {
  QVERIFY(hunkFingerprint(makeHunk({"+one"})) !=
          hunkFingerprint(makeHunk({"+two"})));

  QVERIFY(hunkFingerprint(makeHunk({"+one"})) !=
          hunkFingerprint(makeHunk({"-one"})));
}

void TestCommitPlan::testWhitespaceOnlyHunk() {
  QVERIFY(isWhitespaceOnlyHunk(makeHunk({"-  foo();", "+    foo();"})));
  QVERIFY(isWhitespaceOnlyHunk(makeHunk({" ctx", "-if (a) {", "+if  (a)  {"})));
}

void TestCommitPlan::testWhitespaceDetectionIgnoresRealChanges() {
  QVERIFY(!isWhitespaceOnlyHunk(makeHunk({"-foo();", "+bar();"})));
  QVERIFY(!isWhitespaceOnlyHunk(makeHunk({"+brand new line"})));
  QVERIFY(!isWhitespaceOnlyHunk(makeHunk({" only context"})));
}

void TestCommitPlan::testLooksGenerated() {
  QVERIFY(looksGenerated("package-lock.json"));
  QVERIFY(looksGenerated("app/yarn.lock"));
  QVERIFY(looksGenerated("node_modules/left-pad/index.js"));
  QVERIFY(looksGenerated("web/dist/bundle.js"));
  QVERIFY(looksGenerated("static/app.min.js"));
  QVERIFY(looksGenerated("proto/api.pb.go"));

  QVERIFY(!looksGenerated("src/auth.cpp"));
  QVERIFY(!looksGenerated("README.md"));
  QVERIFY(!looksGenerated("src/builder/factory.cpp"));
}

void TestCommitPlan::testLooksLikeTestOf() {
  QVERIFY(looksLikeTestOf("tests/test_auth.cpp", "src/auth.cpp"));
  QVERIFY(looksLikeTestOf("auth_test.go", "auth.go"));
  QVERIFY(looksLikeTestOf("spec/auth.spec.ts", "src/auth.ts"));
  QVERIFY(looksLikeTestOf("AuthTest.java", "Auth.java"));

  QVERIFY(!looksLikeTestOf("src/auth.cpp", "src/auth.cpp"));
  QVERIFY(!looksLikeTestOf("tests/test_session.cpp", "src/auth.cpp"));

  QVERIFY(!looksLikeTestOf("src/latest.cpp", "src/auth.cpp"));
}

void TestCommitPlan::testPromptsFlagWhitespace() {
  GitDiffFile file;
  file.valid = true;
  file.path = "src/auth.cpp";
  file.hunks.append(makeHunk({"-  foo();", "+    foo();"}));

  QMap<QString, GitDiffFile> diffs;
  diffs.insert(file.path, file);

  const QList<ReviewPrompt> prompts =
      buildReviewPrompts({makeChange("src/auth.cpp", 0, "aa")}, diffs);
  QCOMPARE(prompts.size(), 1);
  QCOMPARE(prompts.first().kind, ReviewPrompt::Kind::WhitespaceOnly);
  QVERIFY(prompts.first().text.contains("whitespace"));
}

void TestCommitPlan::testPromptsFlagGeneratedFiles() {
  QMap<QString, GitDiffFile> diffs;
  const QList<ReviewPrompt> prompts =
      buildReviewPrompts({makeChange("package-lock.json", 0, "aa")}, diffs);

  QCOMPARE(prompts.size(), 1);
  QCOMPARE(prompts.first().kind, ReviewPrompt::Kind::GeneratedFile);
  QVERIFY(prompts.first().text.contains("package-lock.json"));
}

void TestCommitPlan::testPromptsSuggestCoupledChanges() {
  GitDiffFile source;
  source.valid = true;
  source.path = "src/auth.cpp";
  source.hunks.append(makeHunk({"-a();", "+b();"}));
  GitDiffFile test;
  test.valid = true;
  test.path = "tests/test_auth.cpp";
  test.hunks.append(makeHunk({"+EXPECT(b());"}));

  QMap<QString, GitDiffFile> diffs;
  diffs.insert(source.path, source);
  diffs.insert(test.path, test);

  const QList<ReviewPrompt> prompts =
      buildReviewPrompts({makeChange("src/auth.cpp", 0, "aa"),
                          makeChange("tests/test_auth.cpp", 0, "bb")},
                         diffs);

  QCOMPARE(prompts.size(), 1);
  QCOMPARE(prompts.first().kind, ReviewPrompt::Kind::CoupledChange);
  QCOMPARE(prompts.first().subjects.size(), 2);
  QVERIFY(prompts.first().text.contains("coupled"));
}

void TestCommitPlan::testPromptsStaySilentWhenNothingIsOdd() {
  GitDiffFile file;
  file.valid = true;
  file.path = "src/auth.cpp";
  file.hunks.append(makeHunk({"-a();", "+b();"}));

  QMap<QString, GitDiffFile> diffs;
  diffs.insert(file.path, file);

  QVERIFY(buildReviewPrompts({makeChange("src/auth.cpp", 0, "aa")}, diffs)
              .isEmpty());
}

QTEST_APPLESS_MAIN(TestCommitPlan)
#include "test_commitplan.moc"
