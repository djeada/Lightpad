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

/**
 * @brief Custom tab bar with context menu support
 */
class LightpadTabBar : public QTabBar {
    Q_OBJECT

public:
    explicit LightpadTabBar(QWidget* parent = nullptr);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

signals:
    void closeTab(int index);
    void closeOtherTabs(int index);
    void closeTabsToTheRight(int index);
    void closeAllTabs();
};

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
    void tabInserted(int index) override;

private slots:
    void onCloseTab(int index);
    void onCloseOtherTabs(int index);
    void onCloseTabsToTheRight(int index);
    void onCloseAllTabs();

private:
    void updateCloseButtons();
    void setupTabBar();
    MainWindow* mainWindow;
    QToolButton* newTabButton;
    QMap<QWidget*, QString> m_viewerFilePaths;
    QString m_foregroundColor;
    QString m_hoverColor;
    QString m_accentColor;
};

#endif // CODEEDITORTABS_H
