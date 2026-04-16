#ifndef HACKERSTYLE_H
#define HACKERSTYLE_H

#include <QProxyStyle>

class HackerStyle : public QProxyStyle {
  Q_OBJECT

public:
  explicit HackerStyle(QStyle *baseStyle = nullptr);

  void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget) const override;

  void drawControl(ControlElement element, const QStyleOption *option,
                   QPainter *painter, const QWidget *widget) const override;

  void drawComplexControl(ComplexControl control,
                          const QStyleOptionComplex *option, QPainter *painter,
                          const QWidget *widget) const override;

  int pixelMetric(PixelMetric metric, const QStyleOption *option,
                  const QWidget *widget) const override;

  QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                         const QSize &contentsSize,
                         const QWidget *widget) const override;

  int styleHint(StyleHint hint, const QStyleOption *option,
                const QWidget *widget,
                QStyleHintReturn *returnData) const override;

  void polish(QPalette &palette) override;

private:
  void drawMenuItem(const QStyleOption *option, QPainter *painter,
                    const QWidget *widget) const;
  void drawScrollBarHandle(const QStyleOptionComplex *option, QPainter *painter,
                           const QWidget *widget) const;
  void drawProgressBarContents(const QStyleOption *option, QPainter *painter,
                               const QWidget *widget) const;
  void drawTabShape(const QStyleOption *option, QPainter *painter,
                    const QWidget *widget) const;
  void drawHeaderSection(const QStyleOption *option, QPainter *painter,
                         const QWidget *widget) const;
};

#endif
