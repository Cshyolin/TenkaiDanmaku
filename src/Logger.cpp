#include "Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QMutexLocker>

Logger* Logger::instance()
{
    static Logger inst;
    return &inst;
}

Logger::~Logger()
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void Logger::init(const QString &dir)
{
    QMutexLocker lock(&m_mutex);
    m_logDir = dir;
    m_currentDate.clear();
    ensureFile();
}

void Logger::ensureFile()
{
    const QString today = QDateTime::currentDateTime().toString("yyyyMMdd");
    if (today == m_currentDate && m_file.isOpen())
        return;

    // Close previous file
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }

    QDir().mkpath(m_logDir);
    const QString path = m_logDir + "/danmaku_" + today + ".log";
    m_file.setFileName(path);
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_stream.setDevice(&m_file);
    m_currentDate = today;
}

void Logger::writeLine(const QString &line)
{
    ensureFile();
    m_stream << line << "\n";
    m_stream.flush();
}

void Logger::logDanmaku(const QString &type, const QString &ip,
                         const QString &content)
{
    QMutexLocker lock(&m_mutex);
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    writeLine(QString("[%1] %2 %3 %4").arg(ts, type, ip, content));
}

void Logger::logEvent(const QString &event)
{
    QMutexLocker lock(&m_mutex);
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    writeLine(QString("[%1] %2").arg(ts, event));
}

void Logger::logRateLimit(const QString &ip)
{
    QMutexLocker lock(&m_mutex);
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    writeLine(QString("[%1] RATE_LIMIT %2 超过速率限制").arg(ts, ip));
}
