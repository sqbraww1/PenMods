#include "system/input/MediaKeys.h"

#include "base/Hook.h"
#include "base/YPointer.h"

#include "common/Event.h"

#include "filemanager/player/MusicPlayer.h"

#include <QDir>
#include <QFile>

#include <cstdint>
#include <fcntl.h>
#include <unistd.h>

namespace mod {

namespace {

struct RawInputEvent {
    uint64_t tvSec;
    uint64_t tvUsec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

constexpr uint16_t kEvKey = 1;

constexpr uint16_t kNextSong = 163;
constexpr uint16_t kPrevSong = 165;
constexpr uint16_t kStopCd   = 166;
constexpr uint16_t kPlayCd   = 200;
constexpr uint16_t kPauseCd  = 201;

bool isBtMacName(const QString& name) { return name.length() == 17 && name.count(QLatin1Char(':')) == 5; }

QString sysNameOf(const QString& eventName) {
    return QStringLiteral("/sys/class/input/") + eventName + QStringLiteral("/device/name");
}

} // namespace

MediaKeys::MediaKeys() : Logger("MediaKeys") {
    mScanTimer = new QTimer(this);
    mScanTimer->setInterval(2000);
    connect(mScanTimer, &QTimer::timeout, this, &MediaKeys::rescan);

    mPollTimer = new QTimer(this);
    mPollTimer->setInterval(100);
    connect(mPollTimer, &QTimer::timeout, this, &MediaKeys::poll);

    // QTimer 必须在事件循环启动后才能真正生效，因此在 uiCompleted 时再启动。
    connect(&Event::getInstance(), &Event::uiCompleted, this, [this]() {
        info("MediaKeys started (uiCompleted), scanning /dev/input for AVRCP devices");
        mScanTimer->start();
        mPollTimer->start();
        rescan();
    });
}

void MediaKeys::rescan() {
    if (mFd >= 0) {
        if (QFile::exists(sysNameOf(mEventName))) {
            return;
        }
        info("AVRCP media-keys device detached: {}", mEventName.toStdString());
        closeDevice();
    }

    const QDir dir(QStringLiteral("/dev/input"));
    const auto entries = dir.entryList(
        QStringList() << QStringLiteral("event*"),
        QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot
    );
    for (const auto& entry : entries) {
        QFile f(sysNameOf(entry));
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString name = QString::fromUtf8(f.readAll()).trimmed();
        if (!isBtMacName(name)) {
            continue;
        }
        openDevice(entry);
        info("AVRCP media-keys device attached: {} ({})", entry.toStdString(), name.toStdString());
        return;
    }
}

void MediaKeys::openDevice(const QString& eventName) {
    const QString path = QStringLiteral("/dev/input/") + eventName;
    const int     fd   = ::open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        error("Failed to open {}", path.toStdString());
        return;
    }
    mFd        = fd;
    mEventName = eventName;
}

void MediaKeys::closeDevice() {
    if (mFd >= 0) {
        ::close(mFd);
        mFd = -1;
    }
    mEventName.clear();
}

void MediaKeys::poll() {
    if (mFd < 0) {
        return;
    }
    RawInputEvent ev{};
    while (::read(mFd, &ev, sizeof(ev)) == static_cast<ssize_t>(sizeof(ev))) {
        if (ev.type == kEvKey && ev.value == 1) {
            onKey(ev.code);
        }
    }
}

void MediaKeys::onKey(uint16_t code) {
    auto* manager = YPointer<YMediaPlayerManager>::getInstance();
    switch (code) {
    case kNextSong:
        debug("media key: next");
        filemanager::MusicPlayer::getInstance().clickNext();
        break;
    case kPrevSong:
        debug("media key: previous");
        filemanager::MusicPlayer::getInstance().clickPrev();
        break;
    case kStopCd:
        debug("media key: stop");
        filemanager::MusicPlayer::getInstance().stop();
        break;
    case kPlayCd:
        debug("media key: play");
        if (manager) PEN_CALL(void, "_ZN19YMediaPlayerManager13onClickedPlayEv", void*)(manager);
        break;
    case kPauseCd:
        debug("media key: pause");
        if (manager) PEN_CALL(void, "_ZN19YMediaPlayerManager14onClickedPauseEv", void*)(manager);
        break;
    default:
        break;
    }
}

} // namespace mod
