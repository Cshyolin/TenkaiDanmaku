#pragma once

#include <QString>
#include <QMutex>
#include <QFile>
#include <QTextStream>

/// Thread-safe singleton logger with daily-rolling files.
class Logger {
public:
    static Logger* instance();

    /// Initialise / change the log output directory.
    void init(const QString &dir);

    /// Log a danmaku message.  type = "弹幕 IPv6" | "弹幕 IPv4"
    void logDanmaku(const QString &type, const QString &ip, const QString &content);

    /// Log a program event (start / stop / ip change / error).
    void logEvent(const QString &event);

    /// Log a rate-limit trigger.
    void logRateLimit(const QString &ip);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void ensureFile();
    void writeLine(const QString &line);

    QMutex m_mutex;
    QString m_logDir;
    QFile   m_file;
    QTextStream m_stream;
    QString m_currentDate;
};
