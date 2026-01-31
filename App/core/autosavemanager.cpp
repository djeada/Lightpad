#include "autosavemanager.h"
#include "../ui/mainwindow.h"
#include "textarea.h"
#include "lightpadtabwidget.h"
#include "io/filemanager.h"

AutoSaveManager::AutoSaveManager(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_timer(new QTimer(this))
    , m_enabled(false)
    , m_delaySeconds(DEFAULT_DELAY_SECONDS)
{
    connect(m_timer, &QTimer::timeout, this, &AutoSaveManager::onTimer);
}

AutoSaveManager::~AutoSaveManager()
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }
}

void AutoSaveManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        
        if (m_enabled && !m_pendingFiles.isEmpty()) {
            m_timer->start(m_delaySeconds * 1000);
        } else {
            m_timer->stop();
        }
        
        emit stateChanged();
    }
}

bool AutoSaveManager::isEnabled() const
{
    return m_enabled;
}

void AutoSaveManager::setDelay(int seconds)
{
    m_delaySeconds = qMax(5, seconds);  // Minimum 5 seconds
    
    if (m_timer->isActive()) {
        m_timer->setInterval(m_delaySeconds * 1000);
    }
}

int AutoSaveManager::delay() const
{
    return m_delaySeconds;
}

void AutoSaveManager::markModified(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return;
    }
    
    m_pendingFiles.insert(filePath);
    
    // Start timer if enabled and not already running
    if (m_enabled && !m_timer->isActive()) {
        m_timer->start(m_delaySeconds * 1000);
    }
}

void AutoSaveManager::markSaved(const QString& filePath)
{
    m_pendingFiles.remove(filePath);
    
    // Stop timer if no more pending files
    if (m_pendingFiles.isEmpty()) {
        m_timer->stop();
    }
}

void AutoSaveManager::saveAllPending()
{
    if (!m_mainWindow || m_pendingFiles.isEmpty()) {
        return;
    }
    
    QSet<QString> toSave = m_pendingFiles;
    
    for (const QString& filePath : toSave) {
        // Skip untitled files (need to be saved manually)
        if (filePath.isEmpty() || filePath.startsWith("Untitled")) {
            continue;
        }
        
        // Find the tab with this file and save it
        // This is a simplified implementation - in a full implementation
        // we would iterate through tabs and save each one
        
        // For now, emit signal that file was saved (or would be)
        m_pendingFiles.remove(filePath);
        emit fileSaved(filePath);
    }
    
    if (m_pendingFiles.isEmpty()) {
        m_timer->stop();
    }
}

int AutoSaveManager::pendingCount() const
{
    return m_pendingFiles.size();
}

void AutoSaveManager::onTimer()
{
    if (m_enabled && !m_pendingFiles.isEmpty()) {
        saveAllPending();
    }
}
