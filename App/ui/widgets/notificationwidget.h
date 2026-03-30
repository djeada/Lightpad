#ifndef NOTIFICATIONWIDGET_H
#define NOTIFICATIONWIDGET_H

#include <QFrame>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

class NotificationWidget : public QFrame {
  Q_OBJECT
  Q_PROPERTY(qreal windowOpacity READ windowOpacity WRITE setWindowOpacity)

public:
  enum class Level { Info, Warning, Error };

  explicit NotificationWidget(QWidget *parent = nullptr);

  void showNotification(const QString &title, const QString &message,
                        Level level = Level::Info, int durationMs = 6000);

  void dismiss();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void setupUi();
  void applyStyle(Level level);
  void positionInParent();

  QLabel *m_iconLabel;
  QLabel *m_titleLabel;
  QLabel *m_messageLabel;
  QPushButton *m_closeButton;
  QTimer *m_dismissTimer;
  QPropertyAnimation *m_fadeAnimation;
};

class NotificationManager : public QObject {
  Q_OBJECT

public:
  explicit NotificationManager(QWidget *parentWidget,
                               QObject *parent = nullptr);

  void showInfo(const QString &title, const QString &message,
                int durationMs = 5000);
  void showWarning(const QString &title, const QString &message,
                   int durationMs = 8000);
  void showError(const QString &title, const QString &message,
                 int durationMs = 10000);

private:
  QWidget *m_parentWidget;
};

#endif
