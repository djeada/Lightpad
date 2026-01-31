#include "shellprofile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ShellProfileManager& ShellProfileManager::instance()
{
    static ShellProfileManager manager;
    return manager;
}

ShellProfileManager::ShellProfileManager()
{
    detectSystemShells();
}

void ShellProfileManager::detectSystemShells()
{
    m_profiles.clear();

#ifdef Q_OS_WIN
    // Windows shells
    
    // PowerShell
    QString pwsh = "pwsh.exe";
    QString powershell = "powershell.exe";
    
    // Try to find PowerShell Core (pwsh)
    QStringList pwshPaths = {
        "C:/Program Files/PowerShell/7/pwsh.exe",
        "C:/Program Files (x86)/PowerShell/7/pwsh.exe"
    };
    
    for (const QString& path : pwshPaths) {
        if (QFile::exists(path)) {
            m_profiles.append(ShellProfile("PowerShell Core", path, {"-NoLogo"}));
            break;
        }
    }
    
    // Windows PowerShell (always available)
    QString system32 = qEnvironmentVariable("SystemRoot", "C:\\Windows") + "\\System32";
    QString psPath = system32 + "\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (QFile::exists(psPath)) {
        m_profiles.append(ShellProfile("Windows PowerShell", psPath, {"-NoLogo"}));
    }
    
    // Command Prompt
    QString comspec = qEnvironmentVariable("COMSPEC", "cmd.exe");
    if (QFile::exists(comspec)) {
        m_profiles.append(ShellProfile("Command Prompt", comspec));
    }
    
    // Git Bash (if installed)
    QStringList gitBashPaths = {
        "C:/Program Files/Git/bin/bash.exe",
        "C:/Program Files (x86)/Git/bin/bash.exe"
    };
    
    for (const QString& path : gitBashPaths) {
        if (QFile::exists(path)) {
            m_profiles.append(ShellProfile("Git Bash", path, {"-i", "-l"}));
            break;
        }
    }
    
    // WSL (if available)
    QString wslPath = system32 + "\\wsl.exe";
    if (QFile::exists(wslPath)) {
        m_profiles.append(ShellProfile("WSL", wslPath));
    }
    
    // Set default
    if (!m_profiles.isEmpty()) {
        m_defaultProfileName = m_profiles.first().name;
    }
    
#else
    // Unix/Linux/macOS shells
    QStringList shellPaths = {
        "/bin/bash",
        "/usr/bin/bash",
        "/bin/zsh",
        "/usr/bin/zsh",
        "/bin/fish",
        "/usr/bin/fish",
        "/bin/sh",
        "/usr/bin/sh",
        "/bin/tcsh",
        "/usr/bin/tcsh",
        "/bin/ksh",
        "/usr/bin/ksh"
    };
    
    QMap<QString, bool> addedShells;
    
    for (const QString& path : shellPaths) {
        if (QFile::exists(path)) {
            QFileInfo fi(path);
            QString shellName = fi.fileName();
            
            // Skip if we already added this shell type
            if (addedShells.contains(shellName)) {
                continue;
            }
            addedShells[shellName] = true;
            
            ShellProfile profile;
            profile.command = path;
            
            if (shellName == "bash") {
                profile.name = "Bash";
                profile.arguments = QStringList() << "-i";
            } else if (shellName == "zsh") {
                profile.name = "Zsh";
                profile.arguments = QStringList() << "-i";
            } else if (shellName == "fish") {
                profile.name = "Fish";
                profile.arguments = QStringList() << "-i";
            } else if (shellName == "sh") {
                profile.name = "POSIX Shell";
                profile.arguments = QStringList() << "-i";
            } else if (shellName == "tcsh") {
                profile.name = "TCSH";
                profile.arguments = QStringList() << "-i";
            } else if (shellName == "ksh") {
                profile.name = "Korn Shell";
                profile.arguments = QStringList() << "-i";
            } else {
                profile.name = shellName;
                profile.arguments = QStringList() << "-i";
            }
            
            m_profiles.append(profile);
        }
    }
    
    // Also check user's default shell
    QString userShell = qEnvironmentVariable("SHELL", "/bin/sh");
    if (QFile::exists(userShell)) {
        QFileInfo fi(userShell);
        QString shellName = fi.fileName();
        
        // Set user's shell as default
        for (const ShellProfile& profile : m_profiles) {
            if (profile.command == userShell || 
                QFileInfo(profile.command).fileName() == shellName) {
                m_defaultProfileName = profile.name;
                break;
            }
        }
    }
    
    // If no default found, use the first one
    if (m_defaultProfileName.isEmpty() && !m_profiles.isEmpty()) {
        m_defaultProfileName = m_profiles.first().name;
    }
#endif
}

QVector<ShellProfile> ShellProfileManager::availableProfiles() const
{
    return m_profiles;
}

ShellProfile ShellProfileManager::defaultProfile() const
{
    for (const ShellProfile& profile : m_profiles) {
        if (profile.name == m_defaultProfileName) {
            return profile;
        }
    }
    
    // Fallback
    if (!m_profiles.isEmpty()) {
        return m_profiles.first();
    }
    
    // Last resort fallback
#ifdef Q_OS_WIN
    return ShellProfile("Command Prompt", qEnvironmentVariable("COMSPEC", "cmd.exe"));
#else
    return ShellProfile("Shell", qEnvironmentVariable("SHELL", "/bin/sh"), {"-i"});
#endif
}

ShellProfile ShellProfileManager::profileByName(const QString& name) const
{
    for (const ShellProfile& profile : m_profiles) {
        if (profile.name == name) {
            return profile;
        }
    }
    return ShellProfile();
}

void ShellProfileManager::addProfile(const ShellProfile& profile)
{
    if (!profile.isValid()) {
        return;
    }
    
    // Remove existing profile with same name
    removeProfile(profile.name);
    m_profiles.append(profile);
}

bool ShellProfileManager::removeProfile(const QString& name)
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].name == name) {
            m_profiles.remove(i);
            return true;
        }
    }
    return false;
}
