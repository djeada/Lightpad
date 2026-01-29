#include "filemanager.h"
#include "../logging/logger.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

FileManager& FileManager::instance()
{
    static FileManager instance;
    return instance;
}

FileManager::FileManager()
    : QObject(nullptr)
{
}

FileManager::FileResult FileManager::readFile(const QString& filePath)
{
    FileResult result;
    result.success = false;

    if (filePath.isEmpty()) {
        result.errorMessage = "File path is empty";
        LOG_WARNING(result.errorMessage);
        emit fileError(filePath, result.errorMessage);
        return result;
    }

    QFile file(filePath);
    if (!file.exists()) {
        result.errorMessage = QString("File does not exist: %1").arg(filePath);
        LOG_WARNING(result.errorMessage);
        emit fileError(filePath, result.errorMessage);
        return result;
    }

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        result.errorMessage = QString("Cannot open file for reading: %1").arg(filePath);
        LOG_ERROR(result.errorMessage);
        emit fileError(filePath, result.errorMessage);
        return result;
    }

    QTextStream stream(&file);
    result.content = stream.readAll();
    result.success = true;
    file.close();

    LOG_INFO(QString("Successfully read file: %1").arg(filePath));
    emit fileOpened(filePath);

    return result;
}

FileManager::FileResult FileManager::writeFile(const QString& filePath, const QString& content)
{
    FileResult result;
    result.success = false;

    if (filePath.isEmpty()) {
        result.errorMessage = "File path is empty";
        LOG_WARNING(result.errorMessage);
        emit fileError(filePath, result.errorMessage);
        return result;
    }

    QFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        result.errorMessage = QString("Cannot open file for writing: %1").arg(filePath);
        LOG_ERROR(result.errorMessage);
        emit fileError(filePath, result.errorMessage);
        return result;
    }

    QTextStream stream(&file);
    stream << content;
    file.close();

    result.success = true;
    LOG_INFO(QString("Successfully saved file: %1").arg(filePath));
    emit fileSaved(filePath);

    return result;
}

bool FileManager::fileExists(const QString& filePath)
{
    return QFileInfo(filePath).exists();
}

QString FileManager::getFileExtension(const QString& filePath)
{
    return QFileInfo(filePath).completeSuffix();
}

QString FileManager::getFileName(const QString& filePath)
{
    return QFileInfo(filePath).fileName();
}

QString FileManager::getDirectory(const QString& filePath)
{
    return QFileInfo(filePath).absoluteDir().path();
}
