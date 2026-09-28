#include "core/io/fileopenguard.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace FileOpenGuard;

class TestFileOpenGuard : public QObject {
  Q_OBJECT

private slots:
  void init();

  void testNulByteMeansBinary();
  void testPlainTextIsNotBinary();
  void testUtf8TextIsNotBinary();
  void testEmptySampleIsNotBinary();

  void testFormatsSizesForPeople();

  void testOrdinarySourceFileOpensDirectly();
  void testMissingFileIsUnreadable();
  void testDirectoryIsUnreadable();
  void testBinaryFileIsFlagged();
  void testLargeTextFileIsFlagged();
  void testBinaryBeatsLargeWhenBoth();
  void testAssessmentNamesTheFileAndSize();
  void testAssessOnlyReadsASample();

  void testCappedReadStopsAtTheLimit();
  void testCappedReadReportsNoTruncationForSmallFiles();
  void testCappedReadDoesNotSplitMultiByteCharacters();
  void testCappedReadOnMissingFileFails();

  void testUtf8DecodeKeepsUtf8AndNoBom();
  void testUtf8BomIsRememberedAndWrittenBack();
  void testInvalidUtf8FallsBackToLatin1RoundTrip();
  void testLatin1FallsBackToUtf8WhenTextIsNotRepresentable();
  void testCrlfIsDetectedAndRestoredOnEncode();
  void testLfFileStaysLf();
  void testAtomicWriteReplacesContentAndKeepsPermissions();
  void testAtomicWriteFailsForMissingDirectory();

private:
  QTemporaryDir m_dir;
  QString path(const QString &name) const { return m_dir.filePath(name); }
  void write(const QString &name, const QByteArray &bytes);
};

void TestFileOpenGuard::init() { QVERIFY(m_dir.isValid()); }

void TestFileOpenGuard::write(const QString &name, const QByteArray &bytes) {
  QFile file(path(name));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(bytes);
  file.close();
}

void TestFileOpenGuard::testNulByteMeansBinary() {
  QVERIFY(looksBinary(QByteArray("abc\0def", 7)));
  QVERIFY(looksBinary(QByteArray("\0", 1)));
}

void TestFileOpenGuard::testPlainTextIsNotBinary() {
  QVERIFY(!looksBinary(QByteArray("int main() { return 0; }\n")));
  QVERIFY(!looksBinary(QByteArray("tabs\tand\r\nwindows endings\r\n")));
}

void TestFileOpenGuard::testUtf8TextIsNotBinary() {

  QVERIFY(!looksBinary(QString::fromUtf8("héllo — wörld ✓").toUtf8()));
}

void TestFileOpenGuard::testEmptySampleIsNotBinary() {
  QVERIFY(!looksBinary(QByteArray()));
}

void TestFileOpenGuard::testFormatsSizesForPeople() {
  QCOMPARE(formatSize(512), QStringLiteral("512 byte(s)"));
  QCOMPARE(formatSize(2048), QStringLiteral("2.0 KB"));
  QCOMPARE(formatSize(5LL * 1024 * 1024), QStringLiteral("5.0 MB"));
  QCOMPARE(formatSize(3LL * 1024 * 1024 * 1024), QStringLiteral("3.0 GB"));
}

void TestFileOpenGuard::testOrdinarySourceFileOpensDirectly() {
  write("main.cpp", QByteArray("int main() { return 0; }\n"));

  const Assessment a = assess(path("main.cpp"));
  QCOMPARE(a.verdict, Verdict::Open);
  QVERIFY(a.exists);
  QVERIFY(!a.looksBinary);
  QVERIFY(!a.needsConfirmation());
}

void TestFileOpenGuard::testMissingFileIsUnreadable() {
  const Assessment a = assess(path("nope.txt"));
  QCOMPARE(a.verdict, Verdict::Unreadable);
  QVERIFY(!a.exists);
  QVERIFY(!a.needsConfirmation());
  QVERIFY(!a.summary.isEmpty());
}

void TestFileOpenGuard::testDirectoryIsUnreadable() {
  QVERIFY(QDir().mkpath(path("subdir")));
  const Assessment a = assess(path("subdir"));
  QCOMPARE(a.verdict, Verdict::Unreadable);
}

void TestFileOpenGuard::testBinaryFileIsFlagged() {
  QByteArray elf("\x7f", 1);
  elf.append("ELF");
  elf.append('\0');
  elf.append(2000, 'A');
  write("a.out", elf);

  const Assessment a = assess(path("a.out"));
  QCOMPARE(a.verdict, Verdict::Binary);
  QVERIFY(a.looksBinary);
  QVERIFY(a.needsConfirmation());
  QVERIFY(a.consequence.contains(QStringLiteral("corrupt")));
}

void TestFileOpenGuard::testLargeTextFileIsFlagged() {

  write("big.log", QByteArray(5000, 'x'));

  const Assessment a = assess(path("big.log"), 4096);
  QCOMPARE(a.verdict, Verdict::Large);
  QVERIFY(!a.looksBinary);
  QVERIFY(a.needsConfirmation());
  QCOMPARE(a.sizeBytes, 5000LL);
}

void TestFileOpenGuard::testBinaryBeatsLargeWhenBoth() {
  QByteArray bytes(5000, 'x');
  bytes[10] = '\0';
  write("big.bin", bytes);

  const Assessment a = assess(path("big.bin"), 4096);
  QCOMPARE(a.verdict, Verdict::Binary);
}

void TestFileOpenGuard::testAssessmentNamesTheFileAndSize() {
  write("report.log", QByteArray(5000, 'x'));

  const Assessment a = assess(path("report.log"), 4096);
  QVERIFY(a.summary.contains(QStringLiteral("report.log")));
  QVERIFY(a.summary.contains(QStringLiteral("KB")));
  QVERIFY(!a.consequence.isEmpty());
}

void TestFileOpenGuard::testAssessOnlyReadsASample() {

  QByteArray bytes(BinarySampleBytes + 5000, 'x');
  bytes[BinarySampleBytes + 100] = '\0';
  write("late_nul.txt", bytes);

  const Assessment a = assess(path("late_nul.txt"));
  QVERIFY(!a.looksBinary);
}

void TestFileOpenGuard::testCappedReadStopsAtTheLimit() {
  write("big.txt", QByteArray(10000, 'y'));

  bool truncated = false;
  bool ok = false;
  const QString text = readTextCapped(path("big.txt"), 1000, &truncated, &ok);

  QVERIFY(ok);
  QVERIFY(truncated);
  QCOMPARE(text.size(), 1000);
}

void TestFileOpenGuard::testCappedReadReportsNoTruncationForSmallFiles() {
  write("small.txt", QByteArray("hello\n"));

  bool truncated = true;
  bool ok = false;
  const QString text = readTextCapped(path("small.txt"), 1000, &truncated, &ok);

  QVERIFY(ok);
  QVERIFY(!truncated);
  QCOMPARE(text, QStringLiteral("hello\n"));
}

void TestFileOpenGuard::testCappedReadDoesNotSplitMultiByteCharacters() {

  const QByteArray bytes = QString::fromUtf8("abécd").toUtf8();
  write("utf8.txt", bytes);

  bool truncated = false;
  const QString text = readTextCapped(path("utf8.txt"), 3, &truncated);

  QVERIFY(truncated);
  QCOMPARE(text, QStringLiteral("ab"));
  QVERIFY(!text.contains(QChar(0xFFFD)));
}

void TestFileOpenGuard::testCappedReadOnMissingFileFails() {
  bool ok = true;
  const QString text = readTextCapped(path("gone.txt"), 100, nullptr, &ok);
  QVERIFY(!ok);
  QVERIFY(text.isEmpty());
}

void TestFileOpenGuard::testUtf8DecodeKeepsUtf8AndNoBom() {
  TextFormat format;
  const QString text =
      decodeText(QString::fromUtf8("h\u00e9\n").toUtf8(), &format);
  QCOMPARE(text, QString::fromUtf8("h\u00e9\n"));
  QVERIFY(format.encoding == TextEncoding::Utf8);
  QVERIFY(!format.hasBom);
  QCOMPARE(encodeText(text, &format), QString::fromUtf8("h\u00e9\n").toUtf8());
}

void TestFileOpenGuard::testUtf8BomIsRememberedAndWrittenBack() {
  const QByteArray bytes("\xEF\xBB\xBF"
                         "abc\n");
  write("bom.txt", bytes);

  TextFormat format;
  bool ok = false;
  const QString text =
      readTextCapped(path("bom.txt"), 0, nullptr, &ok, &format);
  QVERIFY(ok);
  QCOMPARE(text, QStringLiteral("abc\n"));
  QVERIFY(format.hasBom);
  QCOMPARE(encodeText(text, &format), bytes);
}

void TestFileOpenGuard::testInvalidUtf8FallsBackToLatin1RoundTrip() {
  const QByteArray bytes("caf\xE9 cr\xE8me\n");
  TextFormat format;
  const QString text = decodeText(bytes, &format);
  QVERIFY(format.encoding == TextEncoding::Latin1);
  QCOMPARE(text, QString::fromUtf8("caf\u00e9 cr\u00e8me\n"));
  QVERIFY(!text.contains(QChar(0xFFFD)));

  bool changed = true;
  QCOMPARE(encodeText(text, &format, &changed), bytes);
  QVERIFY(!changed);
}

void TestFileOpenGuard::testLatin1FallsBackToUtf8WhenTextIsNotRepresentable() {
  TextFormat format;
  decodeText(QByteArray("caf\xE9\n"), &format);
  QVERIFY(format.encoding == TextEncoding::Latin1);

  const QString edited = QString::fromUtf8("caf\u00e9 \u20ac\n");
  bool changed = false;
  const QByteArray bytes = encodeText(edited, &format, &changed);
  QVERIFY(changed);
  QVERIFY(format.encoding == TextEncoding::Utf8);
  QCOMPARE(bytes, edited.toUtf8());
}

void TestFileOpenGuard::testCrlfIsDetectedAndRestoredOnEncode() {
  const QByteArray bytes("one\r\ntwo\r\nthree\r\n");
  TextFormat format;
  const QString text = decodeText(bytes, &format);
  QCOMPARE(text, QStringLiteral("one\ntwo\nthree\n"));
  QCOMPARE(format.lineEnding, QStringLiteral("\r\n"));
  QCOMPARE(encodeText(text + "four\n", &format),
           QByteArray("one\r\ntwo\r\nthree\r\nfour\r\n"));
}

void TestFileOpenGuard::testLfFileStaysLf() {
  TextFormat format;
  const QString text = decodeText(QByteArray("a\nb\n"), &format);
  QCOMPARE(format.lineEnding, QStringLiteral("\n"));
  QCOMPARE(encodeText(text, &format), QByteArray("a\nb\n"));
}

void TestFileOpenGuard::testAtomicWriteReplacesContentAndKeepsPermissions() {
  write("perm.sh", QByteArray("old contents that are longer\n"));
  QFile original(path("perm.sh"));
  const QFileDevice::Permissions wanted =
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
  QVERIFY(original.setPermissions(wanted));

  QString error;
  QVERIFY(writeFileAtomically(path("perm.sh"), QByteArray("new\n"), &error));
  QVERIFY(error.isEmpty());

  QFile reread(path("perm.sh"));
  QVERIFY(reread.open(QIODevice::ReadOnly));
  QCOMPARE(reread.readAll(), QByteArray("new\n"));
  QVERIFY(QFileInfo(path("perm.sh")).permissions() & QFileDevice::ExeOwner);
}

void TestFileOpenGuard::testAtomicWriteFailsForMissingDirectory() {
  QString error;
  QVERIFY(!writeFileAtomically(path("no/such/dir/file.txt"), QByteArray("x"),
                               &error));
  QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TestFileOpenGuard)
#include "test_fileopenguard.moc"
