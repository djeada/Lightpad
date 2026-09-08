#ifndef COMMITPLAN_H
#define COMMITPLAN_H

#include "gitdiffmodel.h"
#include <QList>
#include <QMap>
#include <QString>

struct CommitChangeRef {
  enum class Kind { Hunk, UntrackedFile };

  Kind kind = Kind::Hunk;
  QString filePath;
  int hunkIndex = -1;

  QString fingerprint;
  int additions = 0;
  int deletions = 0;

  bool operator==(const CommitChangeRef &other) const {
    return kind == other.kind && filePath == other.filePath &&
           fingerprint == other.fingerprint;
  }

  QString label() const;
};

struct CommitBucket {
  QString name;
  QString message;
  QList<CommitChangeRef> changes;

  int additions() const;
  int deletions() const;
};

struct ReviewPrompt {
  enum class Kind { WhitespaceOnly, GeneratedFile, CoupledChange };

  Kind kind = Kind::WhitespaceOnly;
  QString text;
  QList<CommitChangeRef> subjects;
};

class CommitPlan {
public:
  void setAvailableChanges(const QList<CommitChangeRef> &changes);

  const QList<CommitChangeRef> &available() const { return m_available; }
  QList<CommitChangeRef> unassigned() const;
  const QList<CommitBucket> &buckets() const { return m_buckets; }

  int addBucket(const QString &name, const QString &message = QString());
  void removeBucket(int index);
  void renameBucket(int index, const QString &name);
  void setMessage(int index, const QString &message);

  void assign(int bucketIndex, const CommitChangeRef &change);
  void unassign(const CommitChangeRef &change);
  int bucketOf(const CommitChangeRef &change) const;

  const QList<CommitChangeRef> &staleAssignments() const { return m_stale; }

  bool isEmpty() const { return m_buckets.isEmpty(); }

private:
  QList<CommitChangeRef> m_available;
  QList<CommitBucket> m_buckets;
  QList<CommitChangeRef> m_stale;
};

QString hunkFingerprint(const GitHunk &hunk);

bool isWhitespaceOnlyHunk(const GitHunk &hunk);

bool looksGenerated(const QString &path);

bool looksLikeTestOf(const QString &testPath, const QString &sourcePath);

QList<ReviewPrompt> buildReviewPrompts(const QList<CommitChangeRef> &changes,
                                       const QMap<QString, GitDiffFile> &diffs);

#endif
