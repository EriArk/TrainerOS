#pragma once

#include <QFile>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QDebug>

namespace trainer {

inline void configureShellGraphics()
{
#ifdef Q_OS_LINUX
    // Odin's current Armada/Freedreno OpenGL path has repeatedly left the
    // QSGRenderThread in an unrecoverable GPU wait. Use Qt's Vulkan renderer
    // for the shell only; never export a backend choice to emulator children.
    // An explicit Qt backend remains the maintenance/rollback override.
    if (qEnvironmentVariableIsSet("QSG_RHI_BACKEND")
        || qEnvironmentVariableIsSet("QT_QUICK_BACKEND")
        || qEnvironmentVariableIsSet("QMLSCENE_DEVICE"))
        return;
    const auto platform = QGuiApplication::platformName();
    if (platform != QStringLiteral("xcb") && !platform.startsWith(QStringLiteral("wayland")))
        return;
    QFile model(QStringLiteral("/proc/device-tree/model"));
    if (!model.open(QIODevice::ReadOnly))
        return;
    auto name = model.readAll();
    while (name.endsWith('\0')) name.chop(1);
    if (name.trimmed() != "AYN Odin 2")
        return;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    qInfo() << "TrainerOS shell graphics: Vulkan (Odin 2 workaround)";
#endif
}

} // namespace trainer
