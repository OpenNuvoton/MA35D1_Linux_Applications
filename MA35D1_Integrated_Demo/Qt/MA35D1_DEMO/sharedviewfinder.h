#ifndef SHAREVIEWFINDER_H
#define SHAREVIEWFINDER_H

#include <QAbstractVideoSurface>
#include <QPixmap>

/// \brief The Frame_from_camera struct

class SharedViewfinder : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    SharedViewfinder();

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const;
    bool present(const QVideoFrame &frame);

signals:
    void frameReady(QPixmap);

};

#endif // MAINWINDOW_H
