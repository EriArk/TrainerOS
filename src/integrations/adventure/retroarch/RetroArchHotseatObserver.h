#pragma once
#include <QByteArray>
#include <QList>
#include <QtGlobal>

namespace trainer::retroarch {
// Read-only observation of stock RetroArch 1.22.2 host packets. This does not
// replace its ownership, input or frame synchronization protocol. Only the
// reviewed two-person, exclusive P1 layout is accepted. Nicknames/SRAM are
// never retained or exposed. Transport/session authorization belongs to caller.
class HotseatObserver {
public:
    struct Ownership {
        quint64 sequence = 0;
        quint32 frame = 0;
        int owner = -1; // -1: released; 0: native host; 1: native guest
        bool initial = false;
    };
    bool feed(const QByteArray& bytes);
    QList<Ownership> takeEvents();
    bool failed() const { return failed_; }
    void reset();
private:
    bool finishPacket();
    bool reject();
    QByteArray header_, prefix_;
    QList<Ownership> events_;
    quint32 command_ = 0, remaining_ = 0;
    quint64 sequence_ = 0;
    int owner_ = -1;
    bool handshake_ = false, synchronized_ = false, failed_ = false;
};
}
