#ifndef HACKERSCANLINEOVERLAY_H
#define HACKERSCANLINEOVERLAY_H

#include <QWidget>

class HackerScanlineOverlay : public QWidget {
  Q_OBJECT
public:
  explicit HackerScanlineOverlay(QWidget *parent = nullptr);

  void setEnabled(bool enabled);
  bool isEnabledScanline() const { return m_enabled; }

  void setLineSpacing(int px);
  void setLineOpacity(qreal opacity);

protected:
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  bool m_enabled = false;
  int m_lineSpacing = 3;
  qreal m_lineOpacity = 0.08;
};

#endif
