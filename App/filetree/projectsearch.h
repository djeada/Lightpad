#ifndef PROJECTSEARCH_H
#define PROJECTSEARCH_H

#include <QString>
#include <QVector>
#include <functional>

namespace ProjectSearch {

enum class Mode { FileNames, Contents };

struct Options {
  QString rootPath;
  QString scopePath;
  QString query;
  Mode mode = Mode::FileNames;
  QString includes;
  QString excludes =
      QStringLiteral(".git, .hg, .svn, node_modules, build, dist, .venv, venv, "
                     "__pycache__, .cache");
  bool caseSensitive = false;
  bool wholeWord = false;
  bool regularExpression = false;
  bool includeHidden = true;
  bool includeDirectories = true;
  int maxResults = 1000;
  qint64 maxFileBytes = 2 * 1024 * 1024;

  bool operator==(const Options &other) const {
    return rootPath == other.rootPath && scopePath == other.scopePath &&
           query == other.query && mode == other.mode &&
           includes == other.includes && excludes == other.excludes &&
           caseSensitive == other.caseSensitive &&
           wholeWord == other.wholeWord &&
           regularExpression == other.regularExpression &&
           includeHidden == other.includeHidden &&
           maxResults == other.maxResults &&
           includeDirectories == other.includeDirectories &&
           maxFileBytes == other.maxFileBytes;
  }
};

struct Match {
  QString filePath;
  QString relativePath;
  bool isDirectory = false;
  int line = 0;
  int column = 0;
  int length = 0;
  QString preview;
  int previewStart = 0;
  int previewLength = 0;
  int score = 0;
};

struct Results {
  QVector<Match> matches;
  int scannedFiles = 0;
  int skippedFiles = 0;
  bool truncated = false;
  bool cancelled = false;
  QString error;
};

Results search(const Options &options,
               const std::function<bool()> &isCancelled = {},
               const std::function<void(int)> &progress = {});

int fileNameScore(const QString &query, const QString &relativePath,
                  Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive);

} // namespace ProjectSearch

#endif
