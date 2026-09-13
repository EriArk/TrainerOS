#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <cstdlib>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/resource.h>
#endif

// An original, content-free child program, compiled only for tests.
int main(int argc, char** argv) {
#ifdef Q_OS_WIN
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#else
    const rlimit noCore{0, 0}; setrlimit(RLIMIT_CORE, &noCore);
#endif
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() >= 3 && args[1] == "arguments") {
        QFile file(args[2]); if (!file.open(QIODevice::WriteOnly)) return 3;
        file.write(QJsonDocument(QJsonArray::fromStringList(args.mid(3))).toJson());
        return 0;
    }
    if (args.size() >= 4 && args[1] == "echo") {
        QFile file(args[2]); if (!file.open(QIODevice::WriteOnly)) return 3;
        file.write(args[3].toUtf8()); return 0;
    }
    const auto mode = args.value(1);
    if (mode == "output") {
        QFile output; if (!output.open(stdout, QIODevice::WriteOnly)) return 3;
        output.write("original failure fixture\n"); output.flush();
        QTimer::singleShot(10000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    QTimer::singleShot(mode == "wait" ? 10000 : 100, &app, [&] {
        if (mode == "crash") std::abort();
        app.exit(mode == "error" ? 7 : 0);
    });
    return app.exec();
}
