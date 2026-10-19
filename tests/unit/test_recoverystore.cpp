#include "core/io/recoverystore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

using namespace Recovery;
class TestRecoveryStore : public QObject {
  Q_OBJECT
  static Draft draft(const QString &text = QStringLiteral("unsaved text")) {
    Draft result;
    result.id = newId();
    result.title = "notes.cpp";
    result.originalPath = "/original/notes.cpp";
    result.text = text;
    result.updated = QDateTime::currentDateTimeUtc();
    result.cursor = 3;
    result.format = {FileOpenGuard::TextEncoding::Utf8, true, "\r\n"};
    return result;
  }
  static void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
      qFatal("fixture");
    file.write(bytes);
  }
  static QByteArray read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      qFatal("fixture");
    return file.readAll();
  }
private slots:
  void privateSnapshotsSurviveOwnerDestruction() {
    QTemporaryDir root;
    Draft expected = draft(QString::fromUtf8("café 😀\nsecond\tline"));
    QString session;
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      session = owner.sessionId();
      QVERIFY(owner.save(expected));
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    const auto scan = reader.scan();
    QCOMPARE(scan.drafts.size(), 1);
    const auto actual = scan.drafts.first();
    QCOMPARE(actual.text, expected.text);
    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.sessionId, session);
    QCOMPARE(actual.originalPath, expected.originalPath);
    QCOMPARE(actual.cursor, expected.cursor);
    QCOMPARE(actual.format.encoding, expected.format.encoding);
    QCOMPARE(actual.format.hasBom, true);
    QCOMPARE(actual.format.lineEnding, "\r\n");
#ifndef Q_OS_WIN
    const auto permissions =
        QFileInfo(
            QDir(root.path()).filePath(session + '/' + actual.id + ".json"))
            .permissions();
    QVERIFY(
        !(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                         QFileDevice::ReadOther | QFileDevice::WriteOther)));
#endif
  }
  void runningWindowsAreExcluded() {
    QTemporaryDir root;
    Store first(root.path()), second(root.path());
    QVERIFY(first.start());
    QVERIFY(second.start());
    QVERIFY(first.save(draft()));
    QVERIFY(second.save(draft("other window")));
    QVERIFY(first.scan().drafts.isEmpty());
    QVERIFY(second.scan().drafts.isEmpty());
  }
  void untitledAndEmptyDirtyDocumentsRoundTrip() {
    QTemporaryDir root;
    auto expected = draft("");
    expected.originalPath.clear();
    expected.title = "Untitled";
    expected.cursor = 100;
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(expected));
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    const auto actual = reader.scan().drafts.first();
    QCOMPARE(actual.text, "");
    QVERIFY(actual.originalPath.isEmpty());
    QCOMPARE(actual.cursor, 0);
  }
  void originalFilesAreNeverChanged() {
    QTemporaryDir root;
    QTemporaryDir files;
    const auto original = files.filePath("original.txt");
    write(original, "on disk\r\n");
    auto expected = draft("edited text");
    expected.originalPath = original;
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(expected));
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    auto record = reader.scan().drafts.first();
    Draft adopted;
    QVERIFY(reader.transfer(record, newId(), &adopted));
    QCOMPARE(read(original), "on disk\r\n");
    QVERIFY(reader.finish());
    QCOMPARE(read(original), "on disk\r\n");
  }
  void transferWritesDurableDestinationBeforeRemovingSource() {
    QTemporaryDir root;
    auto expected = draft();
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(expected));
    }
    QString newSession, id = newId();
    {
      Store reader(root.path());
      QVERIFY(reader.start());
      auto record = reader.scan().drafts.first();
      Draft adopted;
      QVERIFY(reader.transfer(record, id, &adopted));
      newSession = reader.sessionId();
      QCOMPARE(adopted.id, id);
      QCOMPARE(adopted.text, expected.text);
      QVERIFY(reader.scan().drafts.isEmpty());
    }
    Store afterRestart(root.path());
    QVERIFY(afterRestart.start());
    const auto result = afterRestart.scan();
    QCOMPARE(result.drafts.size(), 1);
    QCOMPARE(result.drafts.first().sessionId, newSession);
    QCOMPARE(result.drafts.first().text, expected.text);
  }
  void failedTransferRetainsSource() {
    QTemporaryDir root;
    auto expected = draft("larger than limit");
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(expected));
    }
    Store reader(root.path(), 4);
    QVERIFY(reader.start());

    Store inspector(root.path());
    QVERIFY(inspector.start());
    const auto record = inspector.scan().drafts.first();
    QString error;
    QVERIFY(!reader.transfer(record, newId(), nullptr, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(inspector.scan().drafts.size(), 1);
  }
  void discardChecksPreviewFingerprint() {
    QTemporaryDir root;
    QString path;
    auto original = draft();
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(original));
      path = QDir(owner.sessionPath()).filePath(original.id + ".json");
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    const auto before = reader.scan().drafts.first();
    auto object = QJsonDocument::fromJson(read(path)).object();
    object["title"] = "changed externally";
    write(path, QJsonDocument(object).toJson());
    QString error;
    QVERIFY(!reader.discard(before, &error));
    QVERIFY(error.contains("changed"));
    QVERIFY(QFileInfo::exists(path));
    const auto refreshed = reader.scan().drafts.first();
    QVERIFY(reader.discard(refreshed));
    QVERIFY(!QFileInfo::exists(path));
  }
  void transferChecksPreviewFingerprint() {
    QTemporaryDir root;
    auto original = draft();
    QString path;
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(original));
      path = QDir(owner.sessionPath()).filePath(original.id + ".json");
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    auto before = reader.scan().drafts.first();
    auto object = QJsonDocument::fromJson(read(path)).object();
    object["cursor"] = 0;
    write(path, QJsonDocument(object).toJson());
    QVERIFY(!reader.transfer(before, newId(), nullptr));
    QCOMPARE(reader.scan().drafts.size(), 1);
  }
  void corruptOrUnsupportedCopiesAreRetainedAndReported() {
    QTemporaryDir root;
    auto original = draft();
    QString path;
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(original));
      path = QDir(owner.sessionPath()).filePath(original.id + ".json");
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    const auto bytes = read(path);
    write(path, "{truncated");
    auto result = reader.scan();
    QVERIFY(result.drafts.isEmpty());
    QVERIFY(!result.warnings.isEmpty());
    QVERIFY(QFileInfo::exists(path));
    auto object = QJsonDocument::fromJson(bytes).object();
    object["version"] = 99;
    write(path, QJsonDocument(object).toJson());
    result = reader.scan();
    QVERIFY(result.drafts.isEmpty());
    QVERIFY(!result.warnings.isEmpty());
    QVERIFY(QFileInfo::exists(path));
    object["version"] = 1;
    object["text"] = "tampered";
    write(path, QJsonDocument(object).toJson());
    result = reader.scan();
    QVERIFY(result.drafts.isEmpty());
    QVERIFY(!result.warnings.isEmpty());
  }
  void oversizedUpdateKeepsLastGoodSnapshot() {
    QTemporaryDir root;
    auto original = draft("good");
    {
      Store owner(root.path(), 8);
      QVERIFY(owner.start());
      QVERIFY(owner.save(original));
      original.text = QString(100, 'x');
      QString error;
      QVERIFY(!owner.save(original, &error));
      QVERIFY(error.contains("limit"));
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    QCOMPARE(reader.scan().drafts.first().text, "good");
  }
  void normalCloseOnlyRemovesItsOwnSession() {
    QTemporaryDir root;
    {
      Store abandoned(root.path());
      QVERIFY(abandoned.start());
      QVERIFY(abandoned.save(draft("keep orphan")));
    }
    {
      Store normal(root.path());
      QVERIFY(normal.start());
      QVERIFY(normal.save(draft("discard approved")));
      QVERIFY(normal.finish());
    }
    Store reader(root.path());
    QVERIFY(reader.start());
    const auto result = reader.scan();
    QCOMPARE(result.drafts.size(), 1);
    QCOMPARE(result.drafts.first().text, "keep orphan");
  }
  void invalidIdsCannotEscapeRecoveryDirectory() {
    QTemporaryDir root;
    Store owner(root.path());
    QVERIFY(owner.start());
    auto record = draft();
    record.id = "../../outside";
    QVERIFY(!owner.save(record));
    QVERIFY(!owner.remove(record.id));
    QVERIFY(!owner.discard(record));
    QVERIFY(!owner.transfer(record, newId(), nullptr));
  }
  void symbolicLinksAreNotFollowed() {
#ifndef Q_OS_WIN
    QTemporaryDir root, outside;
    const auto link = root.filePath("linked");
    QVERIFY(QFile::link(outside.path(), link));
    Store invalid(link);
    QVERIFY(!invalid.start());
    auto record = draft();
    QString path;
    {
      Store owner(root.path());
      QVERIFY(owner.start());
      QVERIFY(owner.save(record));
      path = QDir(owner.sessionPath()).filePath(record.id + ".json");
    }
    const auto external = outside.filePath("keep.json");
    write(external, "external");
    QVERIFY(QFile::remove(path));
    QVERIFY(QFile::link(external, path));
    Store reader(root.path());
    QVERIFY(reader.start());
    auto scan = reader.scan();
    QVERIFY(scan.drafts.isEmpty());
    QVERIFY(!scan.warnings.isEmpty());
    QCOMPARE(read(external), "external");
#endif
  }
  void killedProcessLeavesRecoverableDrafts() {
    QTemporaryDir root;
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LIGHTPAD_RECOVERY_CRASH_TEST", root.path());
    process.setProcessEnvironment(environment);
    process.start(QCoreApplication::applicationFilePath(), {});
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForReadyRead(3000));
    QVERIFY(process.readAllStandardOutput().contains("ready"));
    process.kill();
    QVERIFY(process.waitForFinished(3000));
    Store reader(root.path());
    QVERIFY(reader.start());
    const auto scan = reader.scan();
    QCOMPARE(scan.drafts.size(), 1);
    QCOMPARE(scan.drafts.first().text, "survives process termination");
  }
};
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const QString root = qEnvironmentVariable("LIGHTPAD_RECOVERY_CRASH_TEST");
  if (!root.isEmpty()) {
    Store owner(root);
    if (!owner.start())
      return 2;
    Draft record;
    record.id = newId();
    record.title = "Crash fixture";
    record.text = "survives process termination";
    if (!owner.save(record))
      return 3;
    QTextStream output(stdout);
    output << "ready\n";
    output.flush();
    return app.exec();
  }
  TestRecoveryStore test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_recoverystore.moc"
