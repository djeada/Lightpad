#ifndef NAVIGATIONHISTORY_H
#define NAVIGATIONHISTORY_H

#include <QObject>
#include <QStack>
#include <QString>

struct NavigationLocation {
  QString filePath;
  int line;
  int column;

  bool operator==(const NavigationLocation &other) const {
    return filePath == other.filePath && line == other.line &&
           column == other.column;
  }

  bool isValid() const { return !filePath.isEmpty(); }
};

class NavigationHistory : public QObject {
  Q_OBJECT

public:
  explicit NavigationHistory(QObject *parent = nullptr);
  ~NavigationHistory();

  void recordLocation(const NavigationLocation &location);

  void recordLocationIfSignificant(const NavigationLocation &location,
                                   int lineThreshold = 5);

  NavigationLocation goBack();

  NavigationLocation goForward();

  bool canGoBack() const;

  bool canGoForward() const;

  void clear();

  NavigationLocation currentLocation() const;

  void setMaxHistorySize(int size);

signals:

  void navigationStateChanged();

private:
  void trimHistory();

  QStack<NavigationLocation> m_backStack;
  QStack<NavigationLocation> m_forwardStack;
  NavigationLocation m_currentLocation;
  int m_maxHistorySize;
  static const int DEFAULT_MAX_HISTORY_SIZE = 100;
};

#endif
