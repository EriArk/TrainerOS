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
    bool failed = false, awaitingFrame = false, motionExercised = false, returnExercised = false;
    QElapsedTimer clock, motionClock;
    QJsonArray dispatchMs, frameMs, examples, motionSamples;
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
            press(SDL_CONTROLLER_BUTTON_X); check(!shell.drawerOpen(),"Shared Y leaked through artwork panel");
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
                for (int i=0;i<80 && dex->detail()["formId"].toString()!=specimen.second;++i) press(SDL_CONTROLLER_BUTTON_Y);
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
            dex->applySearch("906"); dex->activateControl("list",0);
            state->imageWaits = 0; state->stage = 12; return;
        }
        if (state->stage == 12) {
            auto* actor=window->findChild<QQuickItem*>("dex-idle-sprite");
            if (!dex->spritePreview().isEmpty() && actor && !actor->property("ready").toBool() && state->imageWaits++<20) return;
            auto* illustration=window->findChild<QQuickItem*>("dex-detail-art");
            check(illustration && illustration->isVisible(),"Main illustration remains beside optional sprite");
            if (!dex->spritePreview().isEmpty()) check(actor && actor->property("ready").toBool(),"Detail idle sprite renders");
            if (!dex->spritePreview().isEmpty() && actor) {
                auto* frame=actor->findChild<QQuickItem*>("dex-sprite-actor-frame");
                auto* viewport=actor->findChild<QQuickItem*>("sprite-frame-viewport");
                check(frame && viewport && viewport->clip()
                    && qAbs(viewport->width()-frame->property("cellWidth").toReal()*frame->property("scaleFactor").toReal())<0.1,
                    "Atlas is clipped to exactly one scaled frame, not the larger preview area");
            }
            capture("detail-with-sprite");
            if (actor) QMetaObject::invokeMethod(actor,"chooseBehavior",Q_ARG(QVariant,QVariant(1)));
            state->stage=13; return;
        }
        if (state->stage == 13) {
            auto* actor=window->findChild<QQuickItem*>("dex-idle-sprite");
            if (actor && actor->property("active").toBool() && dex->spriteClips().contains("Sleep"))
                check(actor->property("activity").toString()=="Sleep","Available Sleep transition");
            capture("detail-sprite-sleep"); press(SDL_CONTROLLER_BUTTON_START);
            state->stage=14; return;
        }
        if (state->stage == 14) {
            auto* actor=window->findChild<QQuickItem*>("dex-idle-sprite");
            check(actor && !actor->property("active").toBool() && actor->property("activity").toString()=="Idle","Menu pauses and resets detail actor");
            press(b); if (actor) QMetaObject::invokeMethod(actor,"chooseBehavior",Q_ARG(QVariant,QVariant(2)));
            if (actor && actor->property("active").toBool() && dex->spriteClips().contains("WalkRight") && dex->spriteClips().contains("WalkLeft")) {
                check(actor->property("activity").toString()=="WalkRight","Walk uses its directional source clip");
                state->motionExercised=true;
            }
            state->motionClock.start(); state->imageWaits=0; state->stage=15; return;
        }
        if (state->stage == 15) {
            auto* actor=window->findChild<QQuickItem*>("dex-idle-sprite");
            auto* frame=window->findChild<QQuickItem*>("dex-sprite-actor-frame");
            state->motionSamples.append(QJsonObject{{"elapsedMs",state->motionClock.elapsed()},
                {"activity",actor ? actor->property("activity").toString() : QString()},
                {"x",frame ? frame->x() : -1.0}});
            if (actor && actor->property("activity").toString()=="WalkLeft") state->returnExercised=true;
            if (++state->imageWaits==3) capture("detail-sprite-walk-out");
            if (state->imageWaits==8) capture("detail-sprite-walk-back");
            // grabWindow can stall the render animation clock on the real compositor.
            // Await the completed transition, with a wall-clock deadline rather than a tick count.
            if (actor && actor->property("activity").toString()!="Idle" && state->motionClock.elapsed()<6000) return;
            check(actor && actor->property("activity").toString()=="Idle","Walk immediately returns and finishes at Idle");
            if (state->motionExercised) check(state->returnExercised && frame && qAbs(frame->x())<0.1,"Directional return reaches the original position");
            shell.settings()->selectCategory(0); shell.settings()->activateRow(1);
            state->stage=16; return;
        }
        if (state->stage == 16) {
            auto* actor=window->findChild<QQuickItem*>("dex-idle-sprite");
            check(shell.settings()->reducedMotion() && actor && !actor->property("active").toBool(),"Reduced Motion stops the actor");
            shell.settings()->activateRow(1);
            press(down); state->imageWaits=0; state->stage=9; return;
        }
        if (state->stage == 9) {
            check(dex->zone()=="sprites","Down opens separate sprite viewer");
            auto* control=window->activeFocusItem();
            check(control && control->objectName()=="dex-sprite-back","Sprite viewer has fixed Back focus");
            auto* image=window->findChild<QQuickItem*>("dex-sprite-image");
            if (!dex->spriteChoices().isEmpty()) {
                if (image && !image->property("ready").toBool() && state->imageWaits++<20) return;
                check(image && image->property("ready").toBool(),"Optional sprite asset renders");
            }
            capture("sprite-body-1080p");
            const auto choices=dex->spriteChoices();
            for (int i=0;i<choices.size();++i) {
                if (choices[i].toMap()["kind"]=="portrait") break;
                press(right);
            }
            state->imageWaits=0; state->stage++; return;
        }
        if (state->stage == 10) {
            auto* image=window->findChild<QQuickItem*>("dex-sprite-image");
            if (!dex->spriteChoices().isEmpty() && image && !image->property("ready").toBool() && state->imageWaits++<20) return;
            capture("sprite-portrait-1080p");
            press(SDL_CONTROLLER_BUTTON_X); check(!shell.drawerOpen(),"Shared Y cannot leak through sprite viewer");
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767); input.poll();
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768); input.poll();
            check(!shell.centerFace(),"Sprite viewer traps paired navigation");
            press(SDL_CONTROLLER_BUTTON_START); press(b); check(dex->zone()=="sprites","Start restores sprite viewer");
            press(b); check(dex->zone()=="detail","Sprite Back restores illustration detail");
            press(down); press(r1); press(l1); check(dex->zone()=="detail","Page switch closes sprite viewer");
            dex->applySearch("37"); dex->activateControl("list",0); press(SDL_CONTROLLER_BUTTON_Y); press(down);
            if (QGuiApplication::platformName()=="offscreen") window->resize(960,540);
            state->stage++; return;
        }
        if (state->stage == 11) {
            capture("sprite-form-960"); press(b);
            shell.trainer()->picker()->begin("vulpix");
            // The picker projection uses the same exact default-form provider.
            check(shell.trainer()->picker()->entries().size()==1025,"Species picker lost reference");
            shell.trainer()->picker()->cancel();
            check(warnings==0,"QML warnings emitted");
            QJsonObject report{{"motionExercised",state->motionExercised},{"motionSamples",state->motionSamples},{"passed",!state->failed},{"coverage",dex->artCoverage()},
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
