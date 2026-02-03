#ifndef NAVIGATIONHISTORY_H
#define NAVIGATIONHISTORY_H

#include <QObject>
#include <QStack>
#include <QString>

/**
 * @brief Represents a navigation location in the editor
 */
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

/**
 * @brief Manages navigation history for go back/forward functionality
 *
 * Tracks cursor positions and file navigation for implementing
 * VS Code-like back/forward navigation.
 */
class NavigationHistory : public QObject {
  Q_OBJECT

public:
  explicit NavigationHistory(QObject *parent = nullptr);
  ~NavigationHistory();

  /**
   * @brief Record a new navigation location
   *
   * When navigating to a new location, record it in history.
   * This clears the forward history.
   */
  void recordLocation(const NavigationLocation &location);

  /**
   * @brief Record a location change (only if significant)
   *
   * Used for cursor movements - only records if location changed
   * significantly (different file or line difference > threshold)
   */
  void recordLocationIfSignificant(const NavigationLocation &location,
                                   int lineThreshold = 5);

  /**
   * @brief Go back to the previous location
   */
  NavigationLocation goBack();

  /**
   * @brief Go forward to the next location
   */
  NavigationLocation goForward();

  /**
   * @brief Check if back navigation is available
   */
  bool canGoBack() const;

  /**
   * @brief Check if forward navigation is available
   */
  bool canGoForward() const;

  /**
   * @brief Clear all navigation history
   */
  void clear();

  /**
   * @brief Get the current location (without navigating)
   */
  NavigationLocation currentLocation() const;

  /**
   * @brief Set maximum history size
   */
  void setMaxHistorySize(int size);

signals:
  /**
   * @brief Emitted when navigation state changes
   */
  void navigationStateChanged();

private:
  void trimHistory();

  QStack<NavigationLocation> m_backStack;
  QStack<NavigationLocation> m_forwardStack;
  NavigationLocation m_currentLocation;
  int m_maxHistorySize;
  static const int DEFAULT_MAX_HISTORY_SIZE = 100;
};

#endif // NAVIGATIONHISTORY_H
