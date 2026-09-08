#include "commitplan.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QObject>
#include <QSet>

namespace {

QString testStem(const QString &fileName) {
  QString stem = QFileInfo(fileName).completeBaseName();
  const QStringList prefixes{QStringLiteral("test_"), QStringLiteral("tests_"),
                             QStringLiteral("test-")};
  for (const QString &prefix : prefixes) {
    if (stem.startsWith(prefix, Qt::CaseInsensitive)) {
      return stem.mid(prefix.size());
    }
  }
  const QStringList suffixes{QStringLiteral("_test"), QStringLiteral("-test"),
                             QStringLiteral(".test"), QStringLiteral("_spec"),
                             QStringLiteral(".spec")};
  for (const QString &suffix : suffixes) {
    if (stem.endsWith(suffix, Qt::CaseInsensitive)) {
      return stem.left(stem.size() - suffix.size());
    }
  }
  if (stem.endsWith(QStringLiteral("Test")) && stem.size() > 4) {
    return stem.left(stem.size() - 4);
  }
  return QString();
}

} // namespace

QString CommitChangeRef::label() const {
  if (kind == Kind::UntrackedFile) {
    return QObject::tr("%1 — new file").arg(filePath);
  }
  return QObject::tr("%1 — hunk %2 · +%3 −%4")
      .arg(filePath)
      .arg(hunkIndex + 1)
      .arg(additions)
      .arg(deletions);
}

int CommitBucket::additions() const {
  int total = 0;
  for (const CommitChangeRef &change : changes) {
    total += change.additions;
  }
  return total;
}

int CommitBucket::deletions() const {
  int total = 0;
  for (const CommitChangeRef &change : changes) {
    total += change.deletions;
  }
  return total;
}

void CommitPlan::setAvailableChanges(const QList<CommitChangeRef> &changes) {
  m_available = changes;
  m_stale.clear();

  for (CommitBucket &bucket : m_buckets) {
    QList<CommitChangeRef> kept;
    for (const CommitChangeRef &assigned : bucket.changes) {
      bool found = false;
      for (const CommitChangeRef &change : m_available) {
        if (change == assigned) {

          kept.append(change);
          found = true;
          break;
        }
      }
      if (!found) {
        m_stale.append(assigned);
      }
    }
    bucket.changes = kept;
  }
}

QList<CommitChangeRef> CommitPlan::unassigned() const {
  QList<CommitChangeRef> result;
  for (const CommitChangeRef &change : m_available) {
    if (bucketOf(change) < 0) {
      result.append(change);
    }
  }
  return result;
}

int CommitPlan::addBucket(const QString &name, const QString &message) {
  CommitBucket bucket;
  bucket.name = name;
  bucket.message = message;
  m_buckets.append(bucket);
  return m_buckets.size() - 1;
}

void CommitPlan::removeBucket(int index) {
  if (index >= 0 && index < m_buckets.size()) {
    m_buckets.removeAt(index);
  }
}

void CommitPlan::renameBucket(int index, const QString &name) {
  if (index >= 0 && index < m_buckets.size()) {
    m_buckets[index].name = name;
  }
}

void CommitPlan::setMessage(int index, const QString &message) {
  if (index >= 0 && index < m_buckets.size()) {
    m_buckets[index].message = message;
  }
}

void CommitPlan::assign(int bucketIndex, const CommitChangeRef &change) {
  if (bucketIndex < 0 || bucketIndex >= m_buckets.size()) {
    return;
  }
  unassign(change);
  m_buckets[bucketIndex].changes.append(change);
}

void CommitPlan::unassign(const CommitChangeRef &change) {
  for (CommitBucket &bucket : m_buckets) {
    bucket.changes.removeAll(change);
  }
}

int CommitPlan::bucketOf(const CommitChangeRef &change) const {
  for (int i = 0; i < m_buckets.size(); ++i) {
    if (m_buckets.at(i).changes.contains(change)) {
      return i;
    }
  }
  return -1;
}

QString hunkFingerprint(const GitHunk &hunk) {
  QCryptographicHash hash(QCryptographicHash::Sha1);
  for (const GitDiffLine &line : hunk.lines) {
    hash.addData(QByteArray::number(static_cast<int>(line.type)));
    hash.addData(line.text.toUtf8());
    hash.addData("\n");
  }
  return QString::fromLatin1(hash.result().toHex()).left(16);
}

bool isWhitespaceOnlyHunk(const GitHunk &hunk) {
  QStringList removed;
  QStringList added;
  for (const GitDiffLine &line : hunk.lines) {
    if (line.type == GitDiffLineType::Removed) {
      removed << line.text.simplified();
    } else if (line.type == GitDiffLineType::Added) {
      added << line.text.simplified();
    }
  }
  if (removed.isEmpty() && added.isEmpty()) {
    return false;
  }

  removed.removeAll(QString());
  added.removeAll(QString());
  return removed == added;
}

bool looksGenerated(const QString &path) {
  static const QStringList names{
      QStringLiteral("package-lock.json"), QStringLiteral("yarn.lock"),
      QStringLiteral("pnpm-lock.yaml"),    QStringLiteral("Cargo.lock"),
      QStringLiteral("poetry.lock"),       QStringLiteral("Gemfile.lock"),
      QStringLiteral("composer.lock"),     QStringLiteral("go.sum")};
  static const QStringList directories{
      QStringLiteral("node_modules/"), QStringLiteral("vendor/"),
      QStringLiteral("dist/"), QStringLiteral("build/"),
      QStringLiteral(".generated/")};
  static const QStringList suffixes{
      QStringLiteral(".min.js"), QStringLiteral(".min.css"),
      QStringLiteral(".pb.go"),  QStringLiteral(".pb.cc"),
      QStringLiteral(".pb.h"),   QStringLiteral("_pb2.py"),
      QStringLiteral(".g.dart"), QStringLiteral(".generated.cpp")};

  const QString name = QFileInfo(path).fileName();
  if (names.contains(name, Qt::CaseInsensitive)) {
    return true;
  }
  for (const QString &directory : directories) {
    if (path.startsWith(directory) || path.contains('/' + directory)) {
      return true;
    }
  }
  for (const QString &suffix : suffixes) {
    if (name.endsWith(suffix, Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}

bool looksLikeTestOf(const QString &testPath, const QString &sourcePath) {
  if (testPath == sourcePath) {
    return false;
  }
  const QString stem = testStem(QFileInfo(testPath).fileName());
  if (stem.isEmpty()) {
    return false;
  }
  return QFileInfo(sourcePath)
             .completeBaseName()
             .compare(stem, Qt::CaseInsensitive) == 0;
}

QList<ReviewPrompt>
buildReviewPrompts(const QList<CommitChangeRef> &changes,
                   const QMap<QString, GitDiffFile> &diffs) {
  QList<ReviewPrompt> prompts;

  QList<CommitChangeRef> whitespace;
  QList<CommitChangeRef> generated;
  QSet<QString> paths;

  for (const CommitChangeRef &change : changes) {
    paths.insert(change.filePath);

    if (looksGenerated(change.filePath)) {
      generated.append(change);
      continue;
    }
    if (change.kind != CommitChangeRef::Kind::Hunk) {
      continue;
    }
    const auto it = diffs.constFind(change.filePath);
    if (it == diffs.constEnd() || change.hunkIndex < 0 ||
        change.hunkIndex >= it->hunks.size()) {
      continue;
    }
    if (isWhitespaceOnlyHunk(it->hunks.at(change.hunkIndex))) {
      whitespace.append(change);
    }
  }

  if (!whitespace.isEmpty()) {
    ReviewPrompt prompt;
    prompt.kind = ReviewPrompt::Kind::WhitespaceOnly;
    prompt.subjects = whitespace;
    prompt.text =
        whitespace.size() == 1
            ? QObject::tr("1 change touches only whitespace. Separating it "
                          "keeps the behavioural commit readable.")
            : QObject::tr("%1 changes touch only whitespace. Separating them "
                          "keeps the behavioural commit readable.")
                  .arg(whitespace.size());
    prompts.append(prompt);
  }

  if (!generated.isEmpty()) {
    QSet<QString> generatedPaths;
    for (const CommitChangeRef &change : generated) {
      generatedPaths.insert(change.filePath);
    }
    ReviewPrompt prompt;
    prompt.kind = ReviewPrompt::Kind::GeneratedFile;
    prompt.subjects = generated;
    QStringList sorted(generatedPaths.constBegin(), generatedPaths.constEnd());
    sorted.sort();
    prompt.text =
        QObject::tr("%1 looks generated or vendored. Reviewers usually want "
                    "it separate from hand-written changes.")
            .arg(sorted.join(QStringLiteral(", ")));
    prompts.append(prompt);
  }

  for (const QString &testPath : paths) {
    for (const QString &sourcePath : paths) {
      if (!looksLikeTestOf(testPath, sourcePath)) {
        continue;
      }
      ReviewPrompt prompt;
      prompt.kind = ReviewPrompt::Kind::CoupledChange;
      for (const CommitChangeRef &change : changes) {
        if (change.filePath == testPath || change.filePath == sourcePath) {
          prompt.subjects.append(change);
        }
      }
      prompt.text =
          QObject::tr("%1 and %2 changed together and look coupled — they may "
                      "belong in the same commit.")
              .arg(sourcePath, testPath);
      prompts.append(prompt);
    }
  }

  return prompts;
}
