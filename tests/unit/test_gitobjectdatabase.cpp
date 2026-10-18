#include "git/gitobjectdatabase.h"
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <QtEndian>

class TestGitObjectDatabase : public QObject {
  Q_OBJECT
private slots:
  void init();
  void testLooseObjectsAndInvalidInput();
  void testSearchEveryPackAndDuplicateCopy();
  void testPaginationHasNoMissingLocations();
  void testUnreachablePackedObject();
  void testVersionOneIndex();
  void testLargeOffsetsAndInvalidFanout();
  void testDeltaEncodedObjects();
  void testSha256Repository();
  void testWorktreeAndBareRepository();
  void testAlternateObjectStore();
  void testCommitTreeTagAndBinaryRelationships();
  void testBoundedPreviewAndLiteralObjectContents();
  void testMalformedAndMissingIndexes();
  void testVerificationDetectsPackCorruption();
  void testCancellation();

private:
  QTemporaryDir m_temp;
  QString m_repo;
  QString m_head, m_tree, m_blob;
  QByteArray git(const QStringList &args, const QByteArray &input = {},
                 const QString &path = {});
  QString pack(const QStringList &objects, int version = 2);
  QString blob(const QByteArray &content);
};

QByteArray TestGitObjectDatabase::git(const QStringList &args,
                                      const QByteArray &input,
                                      const QString &path) {
  QProcess process;
  process.setWorkingDirectory(path.isEmpty() ? m_repo : path);
  process.start("git", args);
  if (!process.waitForStarted(5000))
    qFatal("Could not start Git");
  process.write(input);
  process.closeWriteChannel();
  if (!process.waitForFinished(15000) || process.exitCode() != 0)
    qFatal("git %s: %s", qPrintable(args.join(' ')),
           process.readAllStandardError().constData());
  return process.readAllStandardOutput().trimmed();
}

void TestGitObjectDatabase::init() {
  QVERIFY(m_temp.isValid());
  static int counter = 0;
  m_repo = m_temp.path() + "/repo with spaces " + QString::number(counter++);
  QVERIFY(QDir().mkpath(m_repo));
  git({"init", "--initial-branch=main"});
  git({"config", "user.name", "Object Test"});
  git({"config", "user.email", "objects@example.com"});
  git({"config", "commit.gpgsign", "false"});
  git({"config", "gc.auto", "0"});
  QFile file(m_repo + "/hello.txt");
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("hello objects\n");
  file.close();
  git({"add", "hello.txt"});
  git({"commit", "-m", "Initial objects"});
  m_head = QString::fromLatin1(git({"rev-parse", "HEAD"}));
  m_tree = QString::fromLatin1(git({"rev-parse", "HEAD^{tree}"}));
  m_blob = QString::fromLatin1(git({"rev-parse", "HEAD:hello.txt"}));
}

QString TestGitObjectDatabase::pack(const QStringList &objects, int version) {
  const QString prefix = m_repo + "/.git/objects/pack/pack";
  const auto name =
      git({"pack-objects", QStringLiteral("--index-version=%1").arg(version),
           prefix},
          objects.join('\n').toLatin1() + '\n');
  return prefix + '-' + QString::fromLatin1(name) + ".pack";
}

QString TestGitObjectDatabase::blob(const QByteArray &content) {
  return QString::fromLatin1(git({"hash-object", "-w", "--stdin"}, content));
}

void TestGitObjectDatabase::testLooseObjectsAndInvalidInput() {
  const auto store = GitObjectDatabase::scan(m_repo);
  QVERIFY2(store.errors.isEmpty(), qPrintable(store.errors.join('\n')));
  QVERIFY(store.packs.isEmpty());
  QCOMPARE(store.looseObjects, 3);
  auto result = GitObjectDatabase::search(store, m_head.left(7).toUpper());
  QCOMPARE(result.totalMatches, 1);
  QCOMPARE(result.objects.first().hash, m_head);
  QCOMPARE(result.objects.first().type, QString("commit"));
  QVERIFY(!result.objects.first().loosePath.isEmpty());
  for (const QString &invalid :
       {QStringLiteral("abc"), QStringLiteral("--all"), QStringLiteral("HEAD"),
        QString(65, 'a'), QStringLiteral("abcd\n1234")}) {
    const auto bad = GitObjectDatabase::search(store, invalid);
    QVERIFY(!bad.errors.isEmpty());
    QVERIFY(bad.objects.isEmpty());
  }
  QVERIFY(!GitObjectDatabase::inspect(m_repo, "--batch").error.isEmpty());
}

void TestGitObjectDatabase::testSearchEveryPackAndDuplicateCopy() {
  const QString first = pack({m_head, m_tree, m_blob});
  const QString second = pack({m_head, m_tree});
  QVERIFY(first != second);
  const auto store = GitObjectDatabase::scan(m_repo);
  QCOMPARE(store.packs.size(), 2);
  for (const auto &info : store.packs) {
    QVERIFY2(info.error.isEmpty(), qPrintable(info.error));
    QCOMPARE(info.indexVersion, 2);
    QCOMPARE(info.packVersion, 2);
  }
  const auto result = GitObjectDatabase::search(store, m_head.left(5));
  QCOMPARE(result.totalMatches, 3); // two packs plus the original loose object
  QCOMPARE(result.objects.size(), 3);
  QSet<QString> paths;
  for (const auto &object : result.objects) {
    QCOMPARE(object.hash, m_head);
    QCOMPARE(object.type, QString("commit"));
    if (!object.packPath.isEmpty()) {
      QVERIFY(object.offset >= 12);
      QCOMPARE(object.crc.size(), 8);
      QCOMPARE(object.representation, QString("commit"));
      paths.insert(object.packPath);
    }
  }
  QCOMPARE(paths.size(), 2);
  const auto scoped = GitObjectDatabase::search(store, m_head, second);
  QCOMPARE(scoped.totalMatches, 1);
  QCOMPARE(scoped.objects.first().packPath, second);
}

void TestGitObjectDatabase::testPaginationHasNoMissingLocations() {
  pack({m_head, m_tree, m_blob});
  pack({m_head, m_tree});
  const auto store = GitObjectDatabase::scan(m_repo);
  const auto all = GitObjectDatabase::search(store, "");
  QCOMPARE(all.totalMatches, 8);
  QSet<QString> seen;
  for (int skip = 0; skip < all.totalMatches; skip += 2) {
    const auto page = GitObjectDatabase::search(store, "", "", skip, 2);
    QCOMPARE(page.totalMatches, all.totalMatches);
    QCOMPARE(page.objects.size(), 2);
    for (const auto &object : page.objects) {
      const QString key =
          object.hash + ':' + object.packPath + object.loosePath;
      QVERIFY(!seen.contains(key));
      seen.insert(key);
    }
  }
  QCOMPARE(seen.size(), all.totalMatches);
  QVERIFY(GitObjectDatabase::search(store, "", "", 100).objects.isEmpty());
}

void TestGitObjectDatabase::testUnreachablePackedObject() {
  const QString hash = blob("unreachable data");
  const QString file = pack({hash});
  QVERIFY(QFile::remove(m_repo + "/.git/objects/" + hash.left(2) + '/' +
                        hash.mid(2)));
  const auto result =
      GitObjectDatabase::search(GitObjectDatabase::scan(m_repo), hash.left(9));
  QCOMPARE(result.totalMatches, 1);
  QCOMPARE(result.objects.first().packPath, file);
  QCOMPARE(GitObjectDatabase::inspect(m_repo, hash).preview,
           QByteArray("unreachable data"));
}

void TestGitObjectDatabase::testVersionOneIndex() {
  const QString file = pack({m_head, m_tree, m_blob}, 1);
  const auto store = GitObjectDatabase::scan(m_repo);
  QCOMPARE(store.packs.first().indexVersion, 1);
  QVERIFY2(store.packs.first().error.isEmpty(),
           qPrintable(store.packs.first().error));
  const auto result = GitObjectDatabase::search(store, m_blob.left(7), file);
  QCOMPARE(result.totalMatches, 1);
  QCOMPARE(result.objects.first().hash, m_blob);
  QVERIFY(result.objects.first().crc.isEmpty());
  QVERIFY(result.objects.first().offset >= 12);
}

void TestGitObjectDatabase::testSha256Repository() {
  const QString path = m_temp.path() + "/sha256";
  QVERIFY(QDir().mkpath(path));
  git({"init", "--object-format=sha256", "--initial-branch=main"}, {}, path);
  const QString hash = QString::fromLatin1(
      git({"hash-object", "-w", "--stdin"}, "sha256 data", path));
  QCOMPARE(hash.size(), 64);
  git({"pack-objects", path + "/.git/objects/pack/pack"},
      hash.toLatin1() + '\n', path);
  const auto store = GitObjectDatabase::scan(path);
  QCOMPARE(store.hashLength, 64);
  QCOMPARE(store.packs.size(), 1);
  QVERIFY2(store.packs.first().error.isEmpty(),
           qPrintable(store.packs.first().error));
  const auto result = GitObjectDatabase::search(store, hash.left(11));
  QCOMPARE(result.totalMatches, 2);
  QCOMPARE(result.objects.first().hash, hash);
  QCOMPARE(GitObjectDatabase::inspect(path, hash).preview,
           QByteArray("sha256 data"));
}

void TestGitObjectDatabase::testLargeOffsetsAndInvalidFanout() {
  const QString file = pack({m_head, m_tree, m_blob});
  QFile idx(file.left(file.size() - 5) + ".idx");
  QVERIFY(idx.open(QIODevice::ReadOnly));
  QByteArray data = idx.readAll();
  idx.close();
  const quint32 count = qFromBigEndian<quint32>(data.constData() + 8 + 255 * 4);
  const int offsets = 8 + 1024 + count * 24;
  const quint32 originalOffset =
      qFromBigEndian<quint32>(data.constData() + offsets);
  qToBigEndian<quint32>(0x80000000U, data.data() + offsets);
  QByteArray large(8, '\0');
  qToBigEndian<quint64>(originalOffset, large.data());
  data.insert(data.size() - 40, large);
  auto saveIndex = [&]() {
    const QByteArray checksum = QCryptographicHash::hash(
        data.left(data.size() - 20), QCryptographicHash::Sha1);
    data.replace(data.size() - 20, 20, checksum);
    if (!idx.setPermissions(idx.permissions() | QFile::WriteOwner) ||
        !idx.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return false;
    const bool ok = idx.write(data) == data.size();
    idx.close();
    return ok;
  };
  QVERIFY(saveIndex());
  auto store = GitObjectDatabase::scan(m_repo);
  QVERIFY2(store.packs.first().error.isEmpty(),
           qPrintable(store.packs.first().error));
  const auto result = GitObjectDatabase::search(store, "", file, 0, 1);
  QCOMPARE(result.objects.size(), 1);
  QCOMPARE(result.objects.first().offset, quint64(originalOffset));
  QVERIFY(!result.objects.first().type.isEmpty());
  qToBigEndian<quint32>(count + 1, data.data() + 8);
  QVERIFY(saveIndex());
  store = GitObjectDatabase::scan(m_repo);
  QVERIFY(store.packs.first().error.contains("fanout"));
}

void TestGitObjectDatabase::testDeltaEncodedObjects() {
  QStringList hashes;
  for (int i = 0; i < 20; ++i) {
    QByteArray data(32 * 1024, 'x');
    data.replace(i * 50, 20, QByteArray::number(i).rightJustified(20, '0'));
    hashes << blob(data);
  }
  const QString prefix = m_repo + "/.git/objects/pack/pack";
  const QString name =
      QString::fromLatin1(git({"pack-objects", "--window=50", "--depth=50",
                               "--delta-base-offset", prefix},
                              hashes.join('\n').toLatin1() + '\n'));
  const QString path = prefix + '-' + name + ".pack";
  const auto result =
      GitObjectDatabase::search(GitObjectDatabase::scan(m_repo), "", path);
  QCOMPARE(result.objects.size(), 20);
  bool foundDelta = false;
  for (const auto &object : result.objects) {
    QCOMPARE(object.type, QString("blob"));
    if (object.representation == "ofs-delta") {
      foundDelta = true;
      QVERIFY(object.deltaBase.startsWith("offset "));
      QCOMPARE(GitObjectDatabase::inspect(m_repo, object.hash).size, 32 * 1024);
    }
  }
  QVERIFY(foundDelta);
}

void TestGitObjectDatabase::testWorktreeAndBareRepository() {
  pack({m_head, m_tree, m_blob});
  const QString worktree = m_repo + "-worktree";
  git({"worktree", "add", "--detach", worktree, "HEAD"});
  const auto shared = GitObjectDatabase::scan(worktree);
  QCOMPARE(shared.packs.size(), 1);
  QCOMPARE(GitObjectDatabase::search(shared, m_head).totalMatches, 2);
  const QString bare = m_repo + "-bare";
  git({"clone", "--bare", m_repo, bare});
  const auto bareStore = GitObjectDatabase::scan(bare);
  QVERIFY2(bareStore.errors.isEmpty(), qPrintable(bareStore.errors.join('\n')));
  QVERIFY(GitObjectDatabase::search(bareStore, m_blob).totalMatches > 0);
}

void TestGitObjectDatabase::testAlternateObjectStore() {
  pack({m_head, m_tree, m_blob});
  const QString clone = m_repo + "-shared";
  git({"clone", "--shared", m_repo, clone});
  const auto store = GitObjectDatabase::scan(clone);
  QCOMPARE(store.objectDirectories.size(), 2);
  QCOMPARE(store.packs.size(), 1);
  QVERIFY(store.packs.first().alternate);
  QCOMPARE(GitObjectDatabase::search(store, m_head).totalMatches, 2);
}

void TestGitObjectDatabase::testCommitTreeTagAndBinaryRelationships() {
  const auto commit = GitObjectDatabase::inspect(m_repo, m_head);
  QVERIFY2(commit.error.isEmpty(), qPrintable(commit.error));
  QCOMPARE(commit.type, QString("commit"));
  QCOMPARE(commit.links.size(), 1);
  QCOMPARE(commit.links.first().hash, m_tree);
  const auto tree = GitObjectDatabase::inspect(m_repo, m_tree);
  QCOMPARE(tree.links.size(), 1);
  QCOMPARE(tree.links.first().label, QString("hello.txt"));
  QCOMPARE(tree.links.first().hash, m_blob);
  git({"tag", "-a", "objects-v1", "-m", "Annotated tag"});
  const QString tagHash = QString::fromLatin1(git({"rev-parse", "objects-v1"}));
  const auto tag = GitObjectDatabase::inspect(m_repo, tagHash);
  QCOMPARE(tag.type, QString("tag"));
  QCOMPARE(tag.links.first().hash, m_head);
  const QByteArray data("binary\0content", 14);
  const auto binary = GitObjectDatabase::inspect(m_repo, blob(data));
  QVERIFY(binary.binary);
  QCOMPARE(binary.preview, data);
  QCOMPARE(GitObjectDatabase::inspect(m_repo, blob({})).preview, QByteArray());
  const QString strange = QString::fromUtf8("folder/\xc3\xa9\tline\nname.txt");
  QVERIFY(QDir().mkpath(m_repo + "/folder"));
  QFile file(m_repo + '/' + strange);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("path test");
  file.close();
  git({"add", "--", strange});
  const QString nextTree = QString::fromLatin1(git({"write-tree"}));
  const auto root = GitObjectDatabase::inspect(m_repo, nextTree);
  QString folderHash;
  for (const auto &link : root.links)
    if (link.label == "folder")
      folderHash = link.hash;
  const auto folder = GitObjectDatabase::inspect(m_repo, folderHash);
  QCOMPARE(folder.links.first().label, strange.mid(7));
}

void TestGitObjectDatabase::testBoundedPreviewAndLiteralObjectContents() {
  const QString large = blob(QByteArray(1024 * 1024, 'x'));
  const auto details = GitObjectDatabase::inspect(m_repo, large);
  QVERIFY2(details.error.isEmpty(), qPrintable(details.error));
  QVERIFY(details.truncated);
  QCOMPARE(details.size, 1024 * 1024);
  QCOMPARE(details.preview.size(), 256 * 1024);
  const QString replacement = blob("replacement contents");
  git({"replace", m_blob, replacement});
  QCOMPARE(GitObjectDatabase::inspect(m_repo, m_blob).preview,
           QByteArray("hello objects\n"));
}

void TestGitObjectDatabase::testMalformedAndMissingIndexes() {
  const QString file = pack({m_head, m_tree, m_blob});
  const QString idx = file.left(file.size() - 5) + ".idx";
  QFile index(idx);
  QVERIFY(index.setPermissions(index.permissions() | QFile::WriteOwner));
  QVERIFY(index.open(QIODevice::WriteOnly | QIODevice::Truncate));
  index.write("bad index");
  index.close();
  auto store = GitObjectDatabase::scan(m_repo);
  QVERIFY(!store.packs.first().error.isEmpty());
  auto result = GitObjectDatabase::search(store, m_head);
  QVERIFY(!result.errors.isEmpty());
  QCOMPARE(result.totalMatches, 1); // the readable loose copy still appears
  QVERIFY(QFile::remove(idx));
  store = GitObjectDatabase::scan(m_repo);
  QVERIFY(!store.packs.first().error.isEmpty());
}

void TestGitObjectDatabase::testVerificationDetectsPackCorruption() {
  const QString file = pack({m_head, m_tree, m_blob});
  bool ok = false;
  const QString idx = file.left(file.size() - 5) + ".idx";
  const auto good = GitObjectDatabase::verifyPack(m_repo, idx, &ok);
  QVERIFY2(ok, qPrintable(good));
  QFile packFile(file);
  QVERIFY(packFile.setPermissions(packFile.permissions() | QFile::WriteOwner));
  QVERIFY(packFile.open(QIODevice::ReadWrite));
  QVERIFY(packFile.seek(20));
  const char byte = packFile.read(1).at(0) ^ 0x40;
  QVERIFY(packFile.seek(20));
  packFile.write(&byte, 1);
  packFile.close();
  const auto bad = GitObjectDatabase::verifyPack(m_repo, idx, &ok);
  QVERIFY(!ok);
  QVERIFY(!bad.isEmpty());
}

void TestGitObjectDatabase::testCancellation() {
  const auto store = GitObjectDatabase::scan(m_repo);
  const auto result =
      GitObjectDatabase::search(store, "", "", 0, 250, []() { return true; });
  QVERIFY(result.cancelled);
  QVERIFY(result.objects.isEmpty());
  const auto details =
      GitObjectDatabase::inspect(m_repo, m_head, []() { return true; });
  QVERIFY(!details.error.isEmpty());
}

QTEST_GUILESS_MAIN(TestGitObjectDatabase)
#include "test_gitobjectdatabase.moc"
