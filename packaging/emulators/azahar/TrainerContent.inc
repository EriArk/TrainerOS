// SPDX-License-Identifier: GPL-2.0-or-later
// Native metadata/install entry point. Runs without a game, window or network.
// Caller supplies an isolated user directory before any FileUtil path access.
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include "common/file_derived.h"
#include "core/file_sys/cia_container.h"
#include "core/file_sys/ncch_container.h"
#include "core/loader/loader.h"

namespace TrainerContent {
static std::optional<int> Run(int argc, char** argv) {
    if (argc != 3 || (std::string_view(argv[1]) != "--traineros-content" &&
                      std::string_view(argv[1]) != "--traineros-install")) return std::nullopt;
    QCoreApplication app(argc, argv);
    const auto user = qEnvironmentVariable("TRAINEROS_AZAHAR_CONTENT_USER");
    const QFileInfo root(user), input(QString::fromUtf8(argv[2]));
    if (!root.isAbsolute() || !root.isDir() || root.isSymLink() || root.canonicalFilePath() != user ||
        !input.isAbsolute() || !input.isFile() || input.size() < 32 || input.size() > 8LL*1024*1024*1024)
        return 2;
    FileUtil::SetUserPath(user.toStdString() + "/");
    Common::Log::Initialize();
    Common::Log::Start();
    QtConfig config;
    const auto path = input.absoluteFilePath().toStdString();
    const bool cia = input.suffix().compare(QStringLiteral("cia"), Qt::CaseInsensitive) == 0 ||
                     input.suffix().compare(QStringLiteral("zcia"), Qt::CaseInsensitive) == 0;
    u64 title_id = 0;
    u16 version = 0;
    bool executable = false;
    QString package_entry;
    if (cia) {
        std::unique_ptr<FileUtil::IOFileBase> file = std::make_unique<FileUtil::IOFile>(path, "rb");
        bool compressed = FileUtil::Z3DSReadIOFile::GetUnderlyingFileMagic(file.get()).has_value();
        if (compressed) file = std::make_unique<FileUtil::Z3DSReadIOFile>(std::move(file));
        FileSys::CIAHeader header{};
        if (file->ReadAtBytes(&header,sizeof(header),0) != sizeof(header) ||
            header.header_size != sizeof(header) || header.cert_size > 1024*1024 ||
            header.tik_size > 2*1024*1024 || header.tmd_size > 4*1024*1024 ||
            header.meta_size > 1024*1024 || header.content_size > 8ULL*1024*1024*1024 ||
            file->GetSize() > 8ULL*1024*1024*1024) return 3;
        file->Seek(0,SEEK_SET);
        FileSys::CIAContainer container;
        if (container.Load(file.get()) != Loader::ResultStatus::Success) return 3;
        if (container.GetContentOffset() > file->GetSize() ||
            container.GetTotalContentSize() > file->GetSize() - container.GetContentOffset()) return 3;
        if (Service::AM::CheckCIAToInstall(path, compressed, true) != Service::AM::InstallStatus::Success)
            return 3;
        const auto& metadata = container.GetTitleMetadata();
        title_id = metadata.GetTitleID();version = metadata.GetTitleVersion();
        const u32 high = static_cast<u32>(title_id >> 32);
        if (high != 0x00040000 && high != 0x00040002 && high != 0x0004000e && high != 0x0004008c) return 4;
        if (Service::AM::GetTitleMediaType(title_id) != Service::FS::MediaType::SDMC ||
            metadata.GetContentCount() == 0) return 4;
        const bool subfolder = metadata.GetContentCount() > 1 &&
            (metadata.GetContentTypeByIndex(1) & FileSys::TMDContentTypeFlag::Optional);
        package_entry = QString::fromStdString(Service::AM::GetTitlePath(Service::FS::MediaType::SDMC,title_id)) +
            QStringLiteral("content/") + (subfolder ? QStringLiteral("00000000/") : QString()) +
            QStringLiteral("%1.app").arg(metadata.GetContentIDByIndex(0),8,16,QLatin1Char('0'));
        executable = high == 0x00040000 || high == 0x00040002;
        if (executable) {
            NCCH_Header header{};
            if (file->ReadAtBytes(&header,sizeof(header),container.GetContentOffset()) != sizeof(header) ||
                !header.is_executable || u64(header.program_id) != title_id) return 4;
        }
        if (std::string_view(argv[1]) == "--traineros-install" &&
            Service::AM::InstallCIA(path) != Service::AM::InstallStatus::Success) return 5;
    } else {
        if (std::string_view(argv[1]) == "--traineros-install") return 4;
        const auto loader = Loader::GetLoader(path);
        if (!loader || loader->IsExecutable(executable) != Loader::ResultStatus::Success || !executable)
            return 3;
        // Homebrew may intentionally have no installed title identity.
        if (loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) title_id = 0;
    }
    const auto media = Service::FS::MediaType::SDMC;
    const auto id = QStringLiteral("%1").arg(title_id, 16, 16, QLatin1Char('0'));
    QJsonArray extra;
    if ((title_id >> 32) == 0x00040000 || (title_id >> 32) == 0x00040002) {
        for (const u64 kind : {0x0004000eULL, 0x0004008cULL})
            extra.append(QString::fromStdString(Service::AM::GetTitlePath(media, (kind << 32) | (title_id & 0xffffffff)) + "content"));
    }
    QJsonObject result{{QStringLiteral("protocol"),1}, {QStringLiteral("title"),id},
        {QStringLiteral("version"),version}, {QStringLiteral("package"),cia},
        {QStringLiteral("executable"),executable}, {QStringLiteral("extras"),extra}};
    if (cia) {
        result[QStringLiteral("contentRoot")] = QString::fromStdString(Service::AM::GetTitlePath(media,title_id) + "content");
        result[QStringLiteral("entry")] = package_entry;
        result[QStringLiteral("tickets")] = QFileInfo(QString::fromStdString(Service::AM::GetTicketPath(title_id,0))).absolutePath();
    }
    const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Compact);
    std::fwrite(bytes.data(),1,bytes.size(),stdout);
    return 0;
}
}
