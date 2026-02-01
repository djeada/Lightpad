#include "navigationhistory.h"

NavigationHistory::NavigationHistory(QObject* parent)
    : QObject(parent)
    , m_maxHistorySize(DEFAULT_MAX_HISTORY_SIZE)
{
}

NavigationHistory::~NavigationHistory()
{
}

void NavigationHistory::recordLocation(const NavigationLocation& location)
{
    if (!location.isValid()) {
        return;
    }

    // Don't record if same as current location
    if (m_currentLocation.isValid() && m_currentLocation == location) {
        return;
    }

    // Push current location to back stack if it's valid
    if (m_currentLocation.isValid()) {
        m_backStack.push(m_currentLocation);
        trimHistory();
    }

    // Set new current location
    m_currentLocation = location;

    // Clear forward stack when new location is recorded
    m_forwardStack.clear();

    emit navigationStateChanged();
}

void NavigationHistory::recordLocationIfSignificant(const NavigationLocation& location, int lineThreshold)
{
    if (!location.isValid()) {
        return;
    }

    // If no current location, just record
    if (!m_currentLocation.isValid()) {
        recordLocation(location);
        return;
    }

    // Different file is always significant
    if (location.filePath != m_currentLocation.filePath) {
        recordLocation(location);
        return;
    }

    // Same file - check if line difference exceeds threshold
    int lineDiff = qAbs(location.line - m_currentLocation.line);
    if (lineDiff >= lineThreshold) {
        recordLocation(location);
    }
}

NavigationLocation NavigationHistory::goBack()
{
    if (!canGoBack()) {
        return NavigationLocation();
    }

    // Push current location to forward stack
    if (m_currentLocation.isValid()) {
        m_forwardStack.push(m_currentLocation);
    }

    // Pop from back stack
    m_currentLocation = m_backStack.pop();

    emit navigationStateChanged();

    return m_currentLocation;
}

NavigationLocation NavigationHistory::goForward()
{
    if (!canGoForward()) {
        return NavigationLocation();
    }

    // Push current location to back stack
    if (m_currentLocation.isValid()) {
        m_backStack.push(m_currentLocation);
    }

    // Pop from forward stack
    m_currentLocation = m_forwardStack.pop();

    emit navigationStateChanged();

    return m_currentLocation;
}

bool NavigationHistory::canGoBack() const
{
    return !m_backStack.isEmpty();
}

bool NavigationHistory::canGoForward() const
{
    return !m_forwardStack.isEmpty();
}

void NavigationHistory::clear()
{
    m_backStack.clear();
    m_forwardStack.clear();
    m_currentLocation = NavigationLocation();
    emit navigationStateChanged();
}

NavigationLocation NavigationHistory::currentLocation() const
{
    return m_currentLocation;
}

void NavigationHistory::setMaxHistorySize(int size)
{
    m_maxHistorySize = qMax(10, size);
    trimHistory();
}

void NavigationHistory::trimHistory()
{
    while (m_backStack.size() > m_maxHistorySize) {
        m_backStack.removeFirst();
    }
}
