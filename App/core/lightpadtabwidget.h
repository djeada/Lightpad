#ifndef LIGHTPADTABWIDGET_H
#define LIGHTPADTABWIDGET_H

#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QMap>

const QString unsavedDocumentLabel = "Unsaved Document";
const int buttonSize = 25;

class MainWindow;
class LightpadPage;

class LightpadTabWidget : public QTabWidget {
    Q_OBJECT

public:
    LightpadTabWidget(QWidget* parent = nullptr);
    void addNewTab();
    void addViewerTab(QWidget* viewer, const QString& filePath);
    void addViewerTab(QWidget* viewer, const QString& filePath, const QString& projectRootPath);
    void setMainWindow(MainWindow* window);
    void setTheme(const QString& backgroundColor,
        const QString& foregroundColor,
        const QString& surfaceColor,
        const QString& hoverColor,
        const QString& accentColor,
        const QString& borderColor);
    void setFilePath(int index, QString filePath);
    void closeAllTabs();
    void closeCurrentTab();
    LightpadPage* getPage(int index);
    LightpadPage* getCurrentPage();
    QString getFilePath(int index);
    bool isViewerTab(int index) const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void tabRemoved(int index) override;

private:
    MainWindow* mainWindow;
    QToolButton* newTabButton;
    QMap<QWidget*, QString> m_viewerFilePaths;
};

#endif // CODEEDITORTABS_H
