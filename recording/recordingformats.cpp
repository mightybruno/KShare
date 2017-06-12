#include "recordingformats.hpp"

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <formats.hpp>
#include <gif-h/gif.h>
#include <platformbackend.hpp>
#include <settings.hpp>
#include <time.h>
#include <unistd.h>

RecordingFormats::RecordingFormats(formats::Recording f) {
    QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

    if (path.isEmpty()) {
        validator = [](QSize) { return false; };
        return;
    }
    tmpDir = QDir(path);
    QString name
    = QString("KShareTemp-") + QString::number(PlatformBackend::inst().pid()) + "-" + QTime::currentTime().toString();
    tmpDir.mkdir(name);
    tmpDir.cd(name);
    switch (f) {
    case formats::Recording::GIF: {
        iFormat = QImage::Format_RGBA8888;
        validator = [](QSize) { return true; };
        consumer = [&](QImage img) { frames.push_back(img); };
        finalizer = [&] {
            if (frames.size() == 0) return QByteArray();
            uint32_t d = 1000 / settings::settings().value("recording/framerate", 30).toInt();
            QImage &startImg = frames[0];
            GifWriter writer;
            GifBegin(&writer, tmpDir.absoluteFilePath("resulting.gif").toLocal8Bit().constData(), startImg.width(),
                     startImg.height(), d);
            for (QImage &a : frames) {
                QByteArray alpha8((char *)a.bits(), a.byteCount());
                GifWriteFrame(&writer, (uint8_t *)alpha8.data(), a.width(), a.height(), d);
            }
            GifEnd(&writer);
            QFile res(tmpDir.absoluteFilePath("resulting.gif"));
            if (!res.open(QFile::ReadOnly)) {
                return QByteArray();
            }
            QByteArray data = res.readAll();
            return data;
        };
        anotherFormat = formats::recordingFormatName(f);
        break;
    }
    case formats::Recording::WebM: {
        iFormat = QImage::Format_RGB888;
        finalizer = [&] {
            delete enc;
            QFile res(tmpDir.absoluteFilePath("res.webm"));
            if (!res.open(QFile::ReadOnly)) {
                return QByteArray();
            }
            QByteArray data = res.readAll();
            return data;
        };
        validator = [&](QSize s) {
            if (!enc) {
                QString path = tmpDir.absoluteFilePath("res.webm");
                enc = new Encoder(path, s);
                if (!enc->isRunning()) {
                    delete enc;
                    return false;
                }
            }
            return true;
        };
        consumer = [&](QImage img) { enc->addFrame(img); };
        break;
    }
    default:
        break;
    }
}

RecordingFormats::~RecordingFormats() {
    tmpDir.removeRecursively();
}

std::function<void(QImage)> RecordingFormats::getConsumer() {
    return consumer;
}

std::function<QByteArray()> RecordingFormats::getFinalizer() {
    return finalizer;
}

std::function<bool(QSize)> RecordingFormats::getValidator() {
    return validator;
}

QImage::Format RecordingFormats::getFormat() {
    return iFormat;
}

QString RecordingFormats::getAnotherFormat() {
    return anotherFormat;
}
