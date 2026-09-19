#pragma once
#include "Models.h"
#include <QImage>

namespace trainer {
// Captured at process start, before any exit request. Domain is explicit even
// though the current personal library only supports the Pokemon context.
struct ExitMediaSource {
    QString trainerId;
    QString domain = "pokemon";
    AdventureRegistration registration;
};
struct ExitCapture {
    QImage frame;
    QDateTime capturedAt;
};
// One latest successful exit image per owner/context/Adventure. This is media,
// never a restore point, save assertion or completion/achievement record.
struct ExitMedia {
    QString sessionId, trainerId, domain, adventureId;
    int registrationRevision = 0;
    QString buildSha256;
    QDateTime capturedAt;
    QByteArray jpeg;
};
}
