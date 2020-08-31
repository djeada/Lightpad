#ifndef LIGHTPADTABWIDGET_H
#define LIGHTPADTABWIDGET_H

#include <QTabWidget>
#include <QToolButton>

class LightpadTabWidget : public QTabWidget
{
    Q_OBJECT

    public:
        LightpadTabWidget(QWidget* parent = nullptr);
        void correctTabButtonPosition();

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void tabRemoved(int index) override;

    private:
        QToolButton* newTabButton;
};

#endif // CODEEDITORTABS_H
