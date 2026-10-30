#pragma once

#include <QByteArray>
#include <QSemaphore>
#include <QString>
#include <QThread>

#include <atomic>

class FileChunkReader : public QThread
{
    Q_OBJECT

public:
    explicit FileChunkReader(const QString &path, quint32 chunkSize = 64U * 1024U,
                             QObject *parent = nullptr);
    ~FileChunkReader() override;

    void requestNextChunk();
    void cancel();

signals:
    void chunkReady(quint64 offset, const QByteArray &data, bool last);
    void readFailed(const QString &message);

protected:
    void run() override;

private:
    QString m_path;
    quint32 m_chunkSize = 64U * 1024U;
    QSemaphore m_requests;
    std::atomic_bool m_cancelled{false};
};
