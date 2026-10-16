#ifndef SEARCHREPLACEENGINE_H
#define SEARCHREPLACEENGINE_H

#include "../filetree/projectsearch.h"
#include "../ui/panels/findreplacesearch.h"
#include <QByteArray>
#include <QString>
#include <QVector>
#include <functional>

namespace SearchReplace {

enum class Scope { CurrentFile, Selection, OpenFiles, Project };

struct Options {
  QString query;
  QString replacement;
  bool matchCase = false;
  bool wholeWord = false;
  bool regex = false;
  bool preserveCase = false;
  bool escapeSequences = false;
  bool dotMatchesNewline = false;
  int maxMatches = 20000;
  qint64 maxFileBytes = 2 * 1024 * 1024;
  qint64 maxSnapshotBytes = 64 * 1024 * 1024;
};

struct Snapshot {
  QString key;
  QString path;
  QString label;
  QString content; // QTextDocument coordinates: LF and UTF-16 code units.
  QByteArray
      bytes; // Exact original bytes for closed files, including BOM/EOLs.
  bool openBuffer = false;
};

struct Occurrence {
  GlobalSearchResult location;
  QString replacement;
  bool selected = true;
};

struct FileResult {
  Snapshot source;
  QVector<Occurrence> occurrences;
};

struct Request {
  Options options;
  Scope scope = Scope::CurrentFile;
  ProjectSearch::Options files;
  QVector<Snapshot> buffers;
  QString currentKey;
  int selectionStart = 0;
  int selectionEnd = -1;
};

struct Results {
  QVector<FileResult> files;
  int matchCount = 0;
  int scannedFiles = 0;
  int skippedFiles = 0;
  bool cancelled = false;
  bool truncated = false;
  QString error;
};

struct Edit {
  int start = 0;
  int length = 0;
  QString replacement;
};
struct Change {
  Snapshot source;
  QString after;
  QByteArray afterBytes;
  QVector<Edit> edits;
};

struct DiskChange {
  QString path;
  QByteArray before;
  QByteArray after;
};
struct DiskReport {
  bool success = false;
  QString error;
  QStringList touchedPaths;
  QVector<DiskChange> retained; // Changes that could not be rolled back.
};

QString fileKey(const QString &path);
QString normalizedText(const QString &text);
QRegularExpression pattern(const Options &options);
QString preserveCase(const QString &replacement, const QString &matched);
QString replacementText(const Options &options,
                        const QRegularExpressionMatch &match);
Results search(const Request &request,
               const std::function<bool()> &cancelled = {},
               const std::function<void(int)> &progress = {});
QVector<Change> selectedChanges(const Results &results);

// Preflight the entire batch, write with QSaveFile, and roll back earlier
// writes if any later write fails. Never overwrite a file changed since its
// snapshot.
DiskReport applyDiskChanges(const QVector<DiskChange> &changes,
                            const std::function<bool()> &cancelled = {},
                            const std::function<void(int, int)> &progress = {});

} // namespace SearchReplace

#endif
