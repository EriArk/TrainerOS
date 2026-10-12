#include "RetroArchHotseatObserver.h"
#include <QtEndian>
#include <utility>

namespace trainer::retroarch {
namespace {
quint32 word(const QByteArray& bytes, int offset) {
    return qFromBigEndian<quint32>(bytes.constData() + offset);
}
constexpr quint32 Sync = 0x23, Mode = 0x26, Refused = 0x27;
constexpr quint32 You = 1U << 31, Playing = 1U << 30;
constexpr quint32 MaxPacket = 64 * 1024 * 1024;
bool unshared(const QByteArray& bytes, int offset) {
    for (int i = 0; i < 16; ++i) if (bytes[offset + i]) return false;
    return true;
}
}
void HotseatObserver::reset() { *this = HotseatObserver{}; }
bool HotseatObserver::reject() {
    failed_ = true;
    header_.clear(); prefix_.clear(); events_.clear();
    return false;
}
QList<HotseatObserver::Ownership> HotseatObserver::takeEvents() {
    return std::exchange(events_, {});
}
bool HotseatObserver::feed(const QByteArray& bytes) {
    if (failed_) return false;
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        if (!handshake_ || !remaining_) {
            const int need = handshake_ ? 8 : 24;
            const auto count = qMin<qsizetype>(need - header_.size(), bytes.size() - offset);
            header_.append(bytes.constData() + offset, count); offset += count;
            if (header_.size() != need) break;
            if (!handshake_) {
                // The reviewed native MODE/SYNC layouts are version 5–7.
                if (word(header_, 0) != 0x52414e50 || word(header_, 16) < 5 || word(header_, 16) > 7)
                    return reject();
                handshake_ = true; header_.clear(); continue;
            }
            command_ = word(header_, 0); remaining_ = word(header_, 4);
            header_.clear(); prefix_.clear();
            if (remaining_ > MaxPacket ||
                (command_ == Sync && (synchronized_ || remaining_ < 184)) ||
                (command_ == Mode && (!synchronized_ || remaining_ != 60)) ||
                (command_ == Refused && remaining_ != 4)) return reject();
            if (!remaining_) continue;
        }
        const auto count = qMin<qsizetype>(remaining_, bytes.size() - offset);
        // SYNC ends in SRAM, MODE ends in a private nickname. Keep neither.
        const int limit = command_ == Sync ? 152 : command_ == Mode ? 28 : 0;
        const auto keep = qMin<qsizetype>(count, qMax<qsizetype>(0, limit - prefix_.size()));
        prefix_.append(bytes.constData() + offset, keep);
        offset += count; remaining_ -= quint32(count);
        if (!remaining_ && !finishPacket()) return reject();
    }
    return true;
}
bool HotseatObserver::finishPacket() {
    if (command_ == Refused) return false; // caller must not retry an uncertain toggle
    if (command_ != Sync && command_ != Mode) return true;
    if (events_.size() >= 32) return false;
    const quint32 frame = word(prefix_, 0);
    if (command_ == Sync) {
        // 8 bytes frame/client + 64 devices + 16 share modes + 64 port owners.
        if ((word(prefix_, 4) & ~You) != 1 || word(prefix_, 8) != 1 ||
            !unshared(prefix_, 72) || word(prefix_, 88) != 1) return false;
        for (int port = 1; port < 16; ++port)
            if (word(prefix_, 88 + port * 4)) return false;
        synchronized_ = true; owner_ = 0;
        events_.append({++sequence_, frame, owner_, true});
        return true;
    }
    const quint32 mode = word(prefix_, 4), devices = word(prefix_, 8);
    const int client = int(mode & 0xffff);
    const bool playing = mode & Playing;
    if (client > 1 || (mode & ~(You | Playing | 0xffffU)) ||
        bool(mode & You) != (client == 1) || !unshared(prefix_, 12) ||
        devices != (playing ? 1U : 0U)) return false;
    if (playing) {
        // Duplicate acquisition or overlapping owners is not a new handoff.
        if (owner_ != -1) return false;
        owner_ = client;
    } else {
        if (owner_ != client) return false;
        owner_ = -1;
    }
    events_.append({++sequence_, frame, owner_, false});
    return true;
}
}
