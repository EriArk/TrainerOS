#include "VolumeKeys.h"
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_LINUX
#include <linux/input.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace trainer {
VolumeKeys::VolumeKeys(bool enabled, QObject* parent) : QObject(parent) {
    repeat_.setSingleShot(true);
    connect(&repeat_, &QTimer::timeout, this, [this] {
        if (!held_) return;
        emit adjustmentRequested(held_ == 115 ? 5 : -5);
        repeat_.start(110);
    });
    if (!enabled) return;
    connect(&scanTimer_, &QTimer::timeout, this, &VolumeKeys::scan);
    scanTimer_.start(5000); scan();
}
VolumeKeys::~VolumeKeys() {
#ifdef Q_OS_LINUX
    for (auto fd : nodes_.keys()) { delete nodes_.take(fd); ::close(fd); }
#endif
}
void VolumeKeys::release() { held_ = 0; repeat_.stop(); }
void VolumeKeys::keyEvent(int code, int value) {
    if (code != 114 && code != 115) return;
    if (value == 0) { if (held_ == code) release(); return; }
    if (value != 1 || held_ == code) return; // Own repeat; ignore kernel repeats.
    held_ = code; emit adjustmentRequested(code == 115 ? 5 : -5); repeat_.start(450);
}
void VolumeKeys::scan() {
#ifdef Q_OS_LINUX
    for (const auto& entry : QDir("/dev/input").entryList({"event*"}, QDir::System | QDir::Files)) {
        const QString path = "/dev/input/" + entry;
        const QString physical = QFileInfo("/sys/class/input/" + entry + "/device").canonicalFilePath();
        if (physical.isEmpty() || physical.contains("/virtual/")) continue;
        bool known = false;
        for (auto* node : nodes_) if (node->objectName() == path) known = true;
        if (known) continue;
        int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        unsigned char keys[(KEY_MAX + 8) / 8]{};
        if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0
            || (!(keys[KEY_VOLUMEUP / 8] & (1 << (KEY_VOLUMEUP % 8)))
                && !(keys[KEY_VOLUMEDOWN / 8] & (1 << (KEY_VOLUMEDOWN % 8))))) { ::close(fd); continue; }
        auto* node = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        node->setObjectName(path); nodes_.insert(fd, node);
        connect(node, &QSocketNotifier::activated, this, [this, fd, node] {
            input_event event{}; ssize_t count;
            while ((count = ::read(fd, &event, sizeof(event))) == sizeof(event)) {
                if (event.type == EV_KEY) keyEvent(event.code, event.value);
                else if (event.type == EV_SYN && event.code == SYN_DROPPED) release();
            }
            if (count == 0 || (count < 0 && errno != EAGAIN && errno != EINTR)) {
                release(); nodes_.remove(fd); node->setEnabled(false); node->deleteLater(); ::close(fd);
            }
        });
    }
#endif
}
}
