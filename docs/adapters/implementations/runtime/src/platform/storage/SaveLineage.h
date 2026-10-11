#pragma once
#include "core/model/SaveBackup.h"
#include <memory>

namespace trainer {
enum class LineageState { Untracked, Imported, Managed, Changed, Broken, Running };
struct SaveLineageProof {
    QByteArray publicKey;
    QList<QByteArray> records, signatures;
    QString error;
};
struct SaveLineageStatus {
    LineageState state = LineageState::Untracked;
    QString head, saveHash, error;
    int count = 0;
};
// Metadata only: proofs never contain save bytes, filesystem paths or private keys.
// The caller must pin the expected identity and stream; a valid signature alone
// does not establish verified gameplay or an honest host.
QString saveLineageStream(const SaveTarget&);
SaveLineageProof readSaveLineage(const QString& backupRoot, const SaveTarget&);
SaveLineageStatus verifySaveLineage(const SaveLineageProof&, const QByteArray& expectedPublicKey,
                                  const QString& owner, const QString& stream, const QString& currentSaveHash);

// Used only inside the existing protected file transaction. Preparation may
// record an imported/external observation, never a successful edit. finish()
// belongs AFTER durable replacement and independent readback. An interruption
// before finish leaves no signed successor; the next observation breaks continuity.
class SaveLineageEdit final {
public:
    SaveLineageEdit(const QString& backupRoot, const SaveTarget&, const AdventureRegistration&,
                    const QString& sourceToken, const QByteArray& before, bool existed,
                    const QString& endingSession = {});
    ~SaveLineageEdit();
    QString error() const;
    QString finish(const QByteArray& after, const QString& operation, const QString& protectionId);
    QString beginSession(); // Signed launch intent, not proof that a process started.
    QString finishSession(bool started, int exitCode, bool crashed, bool stopped);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
