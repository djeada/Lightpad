#ifndef LIGHTPADTABWIDGET_H
#define LIGHTPADTABWIDGET_H

#include <QTabWidget>
#include <QToolButton>

class MainWindow;

class LightpadTabWidget : public QTabWidget
{
    Q_OBJECT

    public:
        LightpadTabWidget(QWidget* parent = nullptr);
        void correctTabButtonPosition();
        void addNewTab();
        void setMainWindow(MainWindow* window);
        void ensureNewTabButtonVisible();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void tabRemoved(int index) override;

    private:
        MainWindow* mainWindow;
        QToolButton* newTabButton;
};

#endif // CODEEDITORTABS_H
