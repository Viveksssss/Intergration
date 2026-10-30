#include "filechunkreader.h"

#include <QFile>

FileChunkReader::FileChunkReader(const QString &path, quint32 chunkSize,
                                 QObject *parent)
    : QThread(parent)
    , m_path(path)
    , m_chunkSize(chunkSize)
{
}

FileChunkReader::~FileChunkReader()
{
    cancel();
    wait();
}

void FileChunkReader::requestNextChunk()
{
    m_requests.release();
}

void FileChunkReader::cancel()
{
    m_cancelled.store(true, std::memory_order_release);
    m_requests.release();
}

void FileChunkReader::run()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit readFailed(QStringLiteral("无法读取文件：%1").arg(file.errorString()));
        return;
    }

    quint64 offset = 0;
    while (!m_cancelled.load(std::memory_order_acquire)) {
        m_requests.acquire();
        if (m_cancelled.load(std::memory_order_acquire)) {
            return;
        }
        const QByteArray chunk = file.read(m_chunkSize);
        if (chunk.isEmpty()) {
            if (file.error() != QFileDevice::NoError) {
                emit readFailed(file.errorString());
            } else {
                emit readFailed(QStringLiteral("文件在传输完成前提前结束"));
            }
            return;
        }
        const bool last = file.atEnd();
        emit chunkReady(offset, chunk, last);
        offset += quint64(chunk.size());
        if (last) {
            return;
        }
    }
}
