#include "search/searchreplaceengine.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace SearchReplace;
class TestSearchReplaceEngine : public QObject {
  Q_OBJECT
  static Request buffer(const QString &text, const QString &query,
                        const QString &replacement = {}) {
    Request request;
    request.options.query = query;
    request.options.replacement = replacement;
    request.currentKey = "buffer:1";
    request.buffers.append({"buffer:1", {}, "Untitled", text, {}, true});
    return request;
  }
  static void write(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
      qFatal("Cannot write fixture");
  }
  static QByteArray read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      qFatal("Cannot read fixture");
    return file.readAll();
  }
private slots:
  void unicodeCoordinatesAndLiteralMatching() {
    const auto results =
        search(buffer(QString::fromUtf8("😀 a.b\n  A.B aXb"), "a.b", "z"));
    QCOMPARE(results.matchCount, 2);
    const auto &hits = results.files.first().occurrences;
    QCOMPARE(hits.at(0).location.columnNumber, 4);
    QCOMPARE(hits.at(1).location.lineNumber, 2);
    QCOMPARE(hits.at(1).location.columnNumber, 3);
    QCOMPARE(selectedChanges(results).first().after,
             QString::fromUtf8("😀 z\n  z aXb"));
  }
  void unicodeWholeWords() {
    auto request =
        buffer(QString::fromUtf8("café caféine _café café_ café\u0301 café"),
               QString::fromUtf8("café"));
    request.options.wholeWord = true;
    QCOMPARE(search(request).matchCount, 2);
    request.options.query = "_";
    QCOMPARE(search(request).matchCount, 0);
  }
  void caseAndPreservation() {
    auto request = buffer("name NAME Name naMe", "name", "newValue");
    request.options.preserveCase = true;
    QCOMPARE(selectedChanges(search(request)).first().after,
             "newvalue NEWVALUE Newvalue newValue");
    request.options.matchCase = true;
    QCOMPARE(search(request).matchCount, 1);
  }
  void regexGroupsAndEscapesAreSinglePass() {
    auto request =
        buffer("ab $1\\1", "(?<first>a)(b)", "${first}-$2-\\1-$$1-\\n");
    request.options.regex = true;
    request.options.escapeSequences = true;
    QCOMPARE(selectedChanges(search(request)).first().after,
             "a-b-a-$1-\n $1\\1");
    request = buffer("$1\\n", "(.+)", "$1");
    request.options.regex = true;
    request.options.escapeSequences = true;
    QCOMPARE(search(request).files.first().occurrences.first().replacement,
             "$1\\n");
  }
  void literalEscapesAndEmptyReplacement() {
    auto request = buffer("first\nsecond first\\nsecond", "first\\nsecond", "");
    QCOMPARE(search(request).matchCount, 1);
    request.options.escapeSequences = true;
    const auto changes = selectedChanges(search(request));
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().after, " first\\nsecond");
  }
  void multilineRegexAndExactLengths() {
    auto request = buffer("a\n  b\na\nb", "^a.*b$", "x");
    request.options.regex = true;
    QCOMPARE(search(request).matchCount, 0);
    request.options.dotMatchesNewline = true;
    const auto result = search(request);
    QCOMPARE(result.matchCount, 1);
    QCOMPARE(result.files.first().occurrences.first().location.matchLength, 9);
    QCOMPARE(selectedChanges(result).first().after, "x");
  }
  void selectionUsesDocumentContext() {
    auto request = buffer("cat scatter cat", "cat", "dog");
    request.scope = Scope::Selection;
    request.selectionStart = 5;
    request.selectionEnd = 15;
    request.options.wholeWord = true;
    QCOMPARE(search(request).matchCount, 1);
    QCOMPARE(selectedChanges(search(request)).first().after, "cat scatter dog");
    request.selectionEnd = 14;
    QCOMPARE(search(request).matchCount, 0);
  }
  void zeroLengthRegexAndNoOps() {
    auto request = buffer("ab", "(?=.)|$", "_");
    request.options.regex = true;
    const auto results = search(request);
    QCOMPARE(results.matchCount, 3);
    QCOMPARE(selectedChanges(results).first().after, "_a_b_");
    request.options.replacement.clear();
    QVERIFY(selectedChanges(search(request)).isEmpty());
  }
  void invalidRegexAndCancellation() {
    auto request = buffer("hello", "[");
    request.options.regex = true;
    QVERIFY(!search(request).error.isEmpty());
    request.options.query = ".";
    QVERIFY(search(request, []() { return true; }).cancelled);
  }
  void limitsDisablePartialReplacement() {
    auto request = buffer("a a a a", "a", "b");
    request.options.maxMatches = 2;
    auto results = search(request);
    QCOMPARE(results.matchCount, 2);
    QVERIFY(results.truncated);
    QVERIFY(selectedChanges(results).isEmpty());
    request.options.maxMatches = 100;
    request.options.maxSnapshotBytes = 4;
    results = search(request);
    QVERIFY(results.truncated);
    QVERIFY(selectedChanges(results).isEmpty());
    request.options.maxSnapshotBytes = 100;
    request.options.replacement = QString(100, 'b');
    results = search(request);
    QVERIFY(results.truncated);
    QVERIFY(selectedChanges(results).isEmpty());
  }
  void openBuffersOverrideDiskAndDuplicateViews() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("a.txt");
    write(path, "disk needle");
    auto request = buffer("dirty needle needle", "needle", "new");
    request.buffers[0].path = path;
    request.buffers[0].key = fileKey(path);
    request.currentKey = fileKey(path);
    request.scope = Scope::Project;
    request.files.rootPath = dir.path();
    request.buffers.append(request.buffers.first());
    auto results = search(request);
    QCOMPARE(results.matchCount, 2);
    QVERIFY(results.files.first().source.openBuffer);
    QCOMPARE(selectedChanges(results).first().after, "dirty new new");
    QCOMPARE(read(path), "disk needle");
    request.buffers[1].content = "conflict";
    QVERIFY(!search(request).error.isEmpty());
  }
  void projectFiltersAndTextDetection() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    write(dir.filePath("src/a.cpp"), "needle");
    write(dir.filePath("src/b.py"), "needle");
    write(dir.filePath("node_modules/x.cpp"), "needle");
    write(dir.filePath(".hidden"), "needle");
    write(dir.filePath("extensionless"), "needle");
    write(dir.filePath("image.bin"), QByteArray("needle\0bad", 10));
    write(dir.filePath("invalid.txt"), QByteArray("needle\xff", 7));
    write(dir.filePath("incomplete.txt"), QByteArray("needle\xc3", 7));
    Request request;
    request.options.query = "needle";
    request.scope = Scope::Project;
    request.files.rootPath = dir.path();
    auto results = search(request);
    QCOMPARE(results.matchCount, 4);
    QCOMPARE(results.skippedFiles, 3);
    request.files.includes = "src/**, *.py";
    request.files.excludes += ", *.py";
    results = search(request);
    QCOMPARE(results.matchCount, 1);
    QVERIFY(results.files.first().source.path.endsWith("a.cpp"));
  }
  void preserveBomAndMixedLineEndings() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto path = dir.filePath("mixed.txt");
    const QByteArray before =
        QByteArray::fromHex("efbbbf") + "first\r\nneedle\nlast\r\n";
    write(path, before);
    Request request;
    request.options.query = "needle";
    request.options.replacement = "one\\ntwo";
    request.options.escapeSequences = true;
    request.scope = Scope::Project;
    request.files.rootPath = dir.path();
    auto changes = selectedChanges(search(request));
    QCOMPARE(changes.size(), 1);
    const auto after =
        QByteArray::fromHex("efbbbf") + "first\r\none\r\ntwo\nlast\r\n";
    QCOMPARE(changes.first().afterBytes, after);
    auto report =
        applyDiskChanges({{path, before, changes.first().afterBytes}});
    QVERIFY2(report.success, qPrintable(report.error));
    QCOMPARE(read(path), after);
    report = applyDiskChanges({{path, after, before}});
    QVERIFY(report.success);
    QCOMPARE(read(path), before);
  }
  void multilineReplacementPreservesUntouchedBytes() {
    QTemporaryDir dir;
    const auto path = dir.filePath("a");
    write(path, "keep\r\none\r\ntwo\nend\r");
    Request request;
    request.scope = Scope::Project;
    request.files.rootPath = dir.path();
    request.options.query = "one\\ntwo";
    request.options.replacement = "new";
    request.options.regex = true;
    auto changes = selectedChanges(search(request));
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().afterBytes, "keep\r\nnew\nend\r");
  }
  void deselectionAndNoOpAreRespected() {
    auto results = search(buffer("cat cat cat", "cat", "dog"));
    results.files[0].occurrences[1].selected = false;
    QCOMPARE(selectedChanges(results).first().after, "dog cat dog");
    QCOMPARE(selectedChanges(results).first().edits.size(), 2);
    QVERIFY(selectedChanges(search(buffer("cat", "cat", "cat"))).isEmpty());
  }
  void preflightLeavesEveryFileUntouched() {
    QTemporaryDir dir;
    const auto a = dir.filePath("a"), b = dir.filePath("b");
    write(a, "old");
    write(b, "changed");
    const auto report =
        applyDiskChanges({{a, "old", "new"}, {b, "old", "new"}});
    QVERIFY(!report.success);
    QCOMPARE(read(a), "old");
    QCOMPARE(read(b), "changed");
  }
  void rollbackOnConcurrentEditAndCancel() {
    QTemporaryDir dir;
    const auto a = dir.filePath("a"), b = dir.filePath("b");
    write(a, "old");
    write(b, "old");
    const QVector<DiskChange> changes{{a, "old", "new"}, {b, "old", "new"}};
    auto report = applyDiskChanges(changes, {}, [&](int done, int) {
      if (done == 1)
        write(b, "external");
    });
    QVERIFY(!report.success);
    QVERIFY(report.retained.isEmpty());
    QCOMPARE(read(a), "old");
    QCOMPARE(read(b), "external");
    write(b, "old");
    bool cancelled = false;
    report = applyDiskChanges(
        changes, [&]() { return cancelled; },
        [&](int done, int) { cancelled = done == 1; });
    QVERIFY(!report.success);
    QCOMPARE(read(a), "old");
    QCOMPARE(read(b), "old");
  }
  void rollbackNeverOverwritesAnExternalEdit() {
    QTemporaryDir dir;
    const auto a = dir.filePath("a"), b = dir.filePath("b");
    write(a, "old");
    write(b, "old");
    const auto report = applyDiskChanges({{a, "old", "new"}, {b, "old", "new"}},
                                         {}, [&](int done, int) {
                                           if (done == 1) {
                                             write(a, "external a");
                                             write(b, "external b");
                                           }
                                         });
    QVERIFY(!report.success);
    QCOMPARE(report.retained.size(), 1);
    QCOMPARE(read(a), "external a");
    QCOMPARE(read(b), "external b");
  }
  void readOnlyAndSymlinksAreNotOverwritten() {
    QTemporaryDir dir;
    const auto a = dir.filePath("a");
    write(a, "old");
    QVERIFY(QFile::setPermissions(a, QFileDevice::ReadOwner |
                                         QFileDevice::ReadGroup));
    QVERIFY(!applyDiskChanges({{a, "old", "new"}}).success);
    QCOMPARE(read(a), "old");
    QVERIFY(QFile::setPermissions(a, QFileDevice::ReadOwner |
                                         QFileDevice::WriteOwner));
#ifndef Q_OS_WIN
    const auto link = dir.filePath("link");
    QVERIFY(QFile::link(a, link));
    QVERIFY(!applyDiskChanges({{link, "old", "new"}}).success);
    QCOMPARE(read(a), "old");
#endif
  }
};
QTEST_GUILESS_MAIN(TestSearchReplaceEngine)
#include "test_searchreplaceengine.moc"
