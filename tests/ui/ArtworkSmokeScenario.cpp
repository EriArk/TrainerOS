#include "ArtworkSmokeScenario.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QQuickItem>
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImageReader>
#include <QUrl>
#include <memory>
using namespace trainer;
namespace {
struct Run {
    int stage = 0, specimen = 0, scroll = 0, imageWaits = 0;
    bool failed = false, awaitingFrame = false;
    QElapsedTimer clock;
    QJsonArray dispatchMs, frameMs, examples;
};
QString memory() {
    QFile file("/proc/self/status"); if (!file.open(QIODevice::ReadOnly)) return {};
    for (const auto& line : file.readAll().split('\n')) if (line.startsWith("VmRSS:")) return QString::fromUtf8(line).trimmed();
    return {};
}
}
void startArtworkSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
                       SDL_Joystick* joystick, const QString& directory, bool& completed,
                       int& warnings, QStringList& diagnostics) {
    auto state = std::make_shared<Run>();
    auto* timer = new QTimer(window); timer->setInterval(250);
    const auto baseline = memory();
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [state] {
        if (state->awaitingFrame) { state->frameMs.append(state->clock.nsecsElapsed()/1000000.0); state->awaitingFrame = false; }
    });
    QObject::connect(timer, &QTimer::timeout, window, [=, &shell, &input, &completed, &warnings, &diagnostics] {
        const auto check = [&](bool good, const QString& message) { if (!good) { state->failed = true; diagnostics.append(message); } };
        const auto press = [&](SDL_GameControllerButton button) {
            SDL_JoystickSetVirtualButton(joystick,button,1); input.poll();
            SDL_JoystickSetVirtualButton(joystick,button,0); input.poll();
        };
        const auto capture = [&](const QString& name) {
            if (!directory.isEmpty()) check(window->grabWindow().save(directory+'/'+name+".png"),"Capture failed: "+name);
        };
        constexpr auto a = SDL_CONTROLLER_BUTTON_B, b = SDL_CONTROLLER_BUTTON_A;
        constexpr auto up = SDL_CONTROLLER_BUTTON_DPAD_UP, down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        constexpr auto right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        constexpr auto l1 = SDL_CONTROLLER_BUTTON_LEFTSHOULDER, r1 = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        auto* dex = shell.pokedex();
        const QList<QPair<QString,QString>> specimens{{"1","1"},{"37","10205"},{"321","321"},{"797","797"},
            {"201","201"},{"869","10498"},{"678","10539"},{"716","10132"},{"1008","10439"},{"1025","1025"}};
        if (state->stage == 0) {
            window->resize(1920,1080); press(r1); press(r1);
            check(dex->entries().size()==1025,"Complete offline reference not composed");
            state->stage++; return;
        }
        if (state->stage == 1) { capture("list-1080p"); press(a); state->stage++; return; }
        if (state->stage == 2) {
            capture("detail-1080p"); press(up); state->stage++; return;
        }
        if (state->stage == 3) {
            check(dex->zone()=="art","Up opens artwork panel"); capture("artwork-source");
            press(SDL_CONTROLLER_BUTTON_Y); check(!shell.drawerOpen(),"Shared Y leaked through artwork panel");
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767); input.poll();
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768); input.poll();
            check(!shell.centerFace(),"Paired face leaked through artwork panel");
            press(SDL_CONTROLLER_BUTTON_START); press(b);
            check(dex->zone()=="art","Start/Back must restore artwork panel");
            press(r1); press(l1); check(dex->zone()=="detail","Primary navigation closes draft");
            state->stage++; return;
        }
        if (state->stage == 4) {
            if (state->specimen < specimens.size()) {
                const auto specimen = specimens[state->specimen];
                dex->applySearch(specimen.first); dex->activateControl("list",0);
                for (int i=0;i<80 && dex->detail()["formId"].toString()!=specimen.second;++i) press(SDL_CONTROLLER_BUTTON_X);
                check(dex->detail()["formId"].toString()==specimen.second,"Form unreachable: "+specimen.second);
                state->imageWaits = 0;
                state->stage = 5; return;
            }
            dex->applySearch(""); dex->activateControl("list",0); press(b);
            timer->setInterval(20); state->stage = 6; return;
        }
        if (state->stage == 5) {
            const auto target = specimens[state->specimen];
            const auto art = dex->detail()["art"].toMap();
            auto* item = window->findChild<QQuickItem*>("dex-detail-art");
            if (art["available"].toBool() && item && !item->property("ready").toBool() && state->imageWaits++ < 20) return;
            if (art["available"].toBool()) check(item && item->property("ready").toBool(),"Derivative didn't render: "+target.second);
            capture("form-"+target.second);
            QElapsedTimer decode; decode.start();
            QImageReader reader(QUrl(art["url"].toString()).toLocalFile());
            const auto decoded = art["available"].toBool() ? reader.read() : QImage{};
            state->examples.append(QJsonObject{{"speciesNumber",target.first},{"formId",target.second},{"available",art["available"].toBool()},
                {"derivativeDecodeMs",decode.nsecsElapsed()/1000000.0},{"decodedBytes",double(decoded.sizeInBytes())}});
            state->specimen++; state->stage = 4; return;
        }
        if (state->stage == 6) {
            if (state->scroll++ < 129) {
                state->clock.restart(); state->awaitingFrame = true; press(right);
                state->dispatchMs.append(state->clock.nsecsElapsed()/1000000.0); return;
            }
            check(dex->entryIndex()==1024,"Rapid controller scrolling didn't reach final species");
            timer->setInterval(250); state->stage++; return;
        }
        if (state->stage == 7) {
            capture("last-species"); dex->applySearch("716"); dex->activateControl("list",0); press(up);
            press(right); state->stage++; return;
        }
        if (state->stage == 8) {
            capture("alternative-illustration"); press(b);
            shell.trainer()->picker()->begin("vulpix");
            // The picker projection uses the same exact default-form provider.
            check(shell.trainer()->picker()->entries().size()==1025,"Species picker lost reference");
            shell.trainer()->picker()->cancel();
            check(warnings==0,"QML warnings emitted");
            QJsonObject report{{"passed",!state->failed},{"coverage",dex->artCoverage()},
                {"logicalViewport",QJsonArray{960,540}},{"windowPixels",QJsonArray{window->width(),window->height()}},
                {"devicePixelRatio",window->devicePixelRatio()},{"platform",QGuiApplication::platformName()},
                {"rssBefore",baseline},{"rssAfter",memory()},{"controllerDispatchMs",state->dispatchMs},
                {"requestToFrameMs",state->frameMs},{"examples",state->examples},{"warnings",warnings},
                {"errors",QJsonArray::fromStringList(diagnostics)}};
            QFile file(directory+"/artwork-report.json");
            check(file.open(QIODevice::WriteOnly),"Cannot write artwork report"); file.write(QJsonDocument(report).toJson());
            timer->stop(); completed = true; qApp->exit(state->failed?1:0);
        }
    });
    timer->start();
}
