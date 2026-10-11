// SPDX-License-Identifier: GPL-3.0-or-later
// Local player frontend for the maintained native workers. No network admission,
// personal save access, or alternate emulator synchronization protocol lives here.
#include "NativeSession.h"
#include "InputTrace.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QTouchEvent>
#include <QWidget>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <algorithm>
#include <array>
#include <csignal>
#include <iostream>
#include <map>
#include <set>

using namespace traineros::ds;
namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
constexpr int Width = 256, Height = 192;
constexpr double FrameMs = 1000.0 / 59.8261;
struct Control { QRectF rect; QString label; int bit; };

class Player final : public QWidget {
public:
    Player(std::vector<uint8_t> rom, unsigned seats, unsigned limit, bool jit, bool diagnostics, const QString& tracePath)
        : seats(seats), limit(limit), diagnostics(diagnostics), completed(seats, 0), images(seats), shown(seats, 0)
    {
        if (!tracePath.isEmpty()) {
            trace.setFileName(tracePath);
            if (!trace.open(QIODevice::WriteOnly | QIODevice::NewOnly))
                throw std::runtime_error("Input trace must be a new writable file");
            trace.write(QByteArray("TRAINEROS_DS_INPUT_V1 ")+QByteArray::number(seats)+'\n');
        }
        setWindowTitle("TrainerOS · DS Local Wireless");
        setAttribute(Qt::WA_AcceptTouchEvents);
        setFocusPolicy(Qt::StrongFocus);
        resize(1280, 720);
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0)
            throw std::runtime_error("Controller/audio initialization failed");
        for (int i = 0; i < SDL_NumJoysticks(); ++i) addController(i);
        SDL_AudioSpec wanted{}; wanted.freq = 48000; wanted.format = AUDIO_S16SYS;
        wanted.channels = 2; wanted.samples = 1024;
        audio = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
        if (audio) SDL_PauseAudioDevice(audio, 0);
        std::vector<melonDS::NDSArgs> args(seats);
        for (unsigned i = 0; i < seats; ++i) {
            if (!jit) args[i].JIT = std::nullopt;
            auto& header = args[i].Firmware.GetHeader();
            header.MacAddr = {0x02, 0, 0, 0, 0, uint8_t(i + 1)};
            header.UpdateChecksum();
        }
        session = std::make_unique<NativeSession>(std::move(args), rom, SessionOptions{60, true});
        clock.start();
        connect(&timer, &QTimer::timeout, this, [this] { tick(); });
        timer.setTimerType(Qt::PreciseTimer); timer.start(4);
    }
    ~Player() override {
        timer.stop(); if (session) session->Stop();
        for (auto [id, pad] : controllers) SDL_GameControllerClose(pad);
        if (audio) SDL_CloseAudioDevice(audio);
        SDL_Quit();
    }
    int result = 0;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#f5eddb"));
        QFont font = p.font(); font.setPixelSize(qMax(14, height() / 36)); p.setFont(font);
        for (unsigned i = 0; i < seats; ++i) button(p, tab(i), QString("DS %1").arg(i + 1), selected == i);
        button(p, exitRect(), "Exit", false);
        p.setPen(QColor("#37484d"));
        p.drawText(QRectF(20, 62, width() - 40, 35), Qt::AlignCenter,
                   error.isEmpty() ? "Temporary local session · progress is not saved" : error);
        for (int screen = 0; screen < 2; ++screen) {
            auto target = screenRect(screen);
            p.fillRect(target.adjusted(-4,-4,4,4), QColor("#6b7f88"));
            p.fillRect(target, Qt::black);
            if (!images[selected].isNull()) p.drawImage(target, images[selected], QRectF(0, screen * Height, Width, Height));
        }
        for (const auto& c : controls()) button(p, c.rect, c.label, held(c.bit));
        p.setPen(QColor("#37484d"));
        p.drawText(QRectF(20, height()-27, width()-40, 25), Qt::AlignCenter,
                   QString("DS %1 of %2 · %3 fps · touch the right screen").arg(selected+1).arg(seats).arg(fps,0,'f',1));
    }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->isAutoRepeat()) return;
        if (e->key() == Qt::Key_Escape) { close(); return; }
        if (e->key() >= Qt::Key_F1 && e->key() < Qt::Key_F1 + int(seats)) { select(e->key()-Qt::Key_F1); return; }
        if (int bit = keyBit(e->key()); bit >= 0) keyboard.insert(bit);
    }
    void keyReleaseEvent(QKeyEvent* e) override {
        if (!e->isAutoRepeat()) keyboard.erase(keyBit(e->key()));
    }
    void focusOutEvent(QFocusEvent*) override { keyboard.clear(); pointers.clear(); gateControllers(); }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) pointer(-1, e->position(), true);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (e->buttons() & Qt::LeftButton) pointer(-1, e->position(), false);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) pointers.erase(-1);
    }
    bool event(QEvent* e) override {
        if (e->type()==QEvent::TouchBegin || e->type()==QEvent::TouchUpdate || e->type()==QEvent::TouchEnd) {
            for (const auto& point : static_cast<QTouchEvent*>(e)->points()) {
                if (point.state()==QEventPoint::State::Released) pointers.erase(point.id());
                else pointer(point.id(), point.position(), point.state()==QEventPoint::State::Pressed);
            }
            e->accept(); return true;
        }
        if (e->type()==QEvent::TouchCancel) { pointers.clear(); return true; }
        return QWidget::event(e);
    }
private:
    QRectF tab(unsigned i) const { return {20.0 + i*110, 15, 100, 42}; }
    QRectF exitRect() const { return {double(width()-120), 15, 100, 42}; }
    QRectF screenRect(int index) const {
        const double scale = qMax(.1, qMin((width()-60.0)/(Width*2), (height()-210.0)/Height));
        const double w=Width*scale, h=Height*scale;
        return {(width()-(w*2+20))/2+index*(w+20), 102+(height()-210-h)/2, w, h};
    }
    std::vector<Control> controls() const {
        const std::array<const char*,12> names={"A","B","Select","Start","Right","Left","Up","Down","R","L","X","Y"};
        const std::array<int,12> order={5,4,6,7,9,8,2,3,11,10,1,0};
        std::vector<Control> out; const double w=(width()-30.0)/12;
        for (int i=0;i<12;++i) out.push_back({{15+i*w, double(height()-91), w-5, 51},names[order[i]],order[i]});
        return out;
    }
    void button(QPainter& p, QRectF rect, const QString& text, bool active) {
        p.setPen(QPen(QColor("#718995"),2)); p.setBrush(QColor(active?"#f7cd70":"#b9dce7"));
        p.drawRoundedRect(rect,8,8); p.setPen(QColor("#203b42")); p.drawText(rect,Qt::AlignCenter,text);
    }
    bool held(int bit) const {
        if (keyboard.count(bit)) return true;
        for (const auto& [id,pos] : pointers) for (const auto& c : controls()) if (c.bit==bit && c.rect.contains(pos)) return true;
        return false;
    }
    void select(unsigned seat) {
        selected=seat; keyboard.clear(); pointers.clear(); gateControllers();
        if (audio) SDL_ClearQueuedAudio(audio); update();
    }
    void gateControllers() { for (const auto& [id,pad]:controllers) neutralRequired.insert(id); }
    void pointer(int id, QPointF pos, bool press) {
        if (press && exitRect().contains(pos)) { close(); return; }
        if (press) for (unsigned i=0;i<seats;++i) if (tab(i).contains(pos)) { select(i); return; }
        pointers[id]=pos;
    }
    static int keyBit(int key) {
        switch (key) {
        case Qt::Key_X:return 0; case Qt::Key_Z:return 1; case Qt::Key_Backspace:return 2;
        case Qt::Key_Return:return 3; case Qt::Key_Right:return 4; case Qt::Key_Left:return 5;
        case Qt::Key_Up:return 6; case Qt::Key_Down:return 7; case Qt::Key_W:return 8;
        case Qt::Key_Q:return 9; case Qt::Key_S:return 10; case Qt::Key_A:return 11;
        default:return -1;
        }
    }
    void addController(int index) {
        if (!SDL_IsGameController(index)) return;
        auto* pad=SDL_GameControllerOpen(index); if (!pad) return;
        auto id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
        if (controllers.count(id)) SDL_GameControllerClose(pad);
        else { controllers[id]=pad; gateControllers(); }
    }
    void tick() {
        if (interrupted) { close(); return; }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type==SDL_CONTROLLERDEVICEADDED) addController(event.cdevice.which);
            if (event.type==SDL_CONTROLLERDEVICEREMOVED) {
                auto it=controllers.find(event.cdevice.which);
                if (it!=controllers.end()) {
                    SDL_GameControllerClose(it->second); controllers.erase(it);
                    neutralRequired.erase(event.cdevice.which); gateControllers();
                }
            }
        }
        FrameResult resultFrame;
        while (session->Take(resultFrame,std::chrono::milliseconds(0))) {
            completed.at(resultFrame.seat)=resultFrame.frame+1;
            ++totalFrames;
            if (diagnostics && resultFrame.frame % 600 == 0)
                std::cout << "DS_LOCAL seat=" << resultFrame.seat << " frame=" << resultFrame.frame
                          << " sent=" << resultFrame.wirelessSent << " received=" << resultFrame.wirelessReceived << std::endl;
        }
        if (session->Failed()) {
            result=2; error="The linked session stopped. Exit and start a new session.";
            timer.stop(); session->Stop(); update(); return;
        }
        for (unsigned i=0;i<seats;++i) {
            PresentationFrame frame;
            if (!session->TakePresentation(i,frame)) continue;
            images[i]=QImage(reinterpret_cast<const uchar*>(frame.pixels.data()),Width,Height*2,QImage::Format_RGB32).copy();
            shown[i]=frame.frame;
            if (audio && i==selected && !frame.audio.empty()) {
                if (SDL_GetQueuedAudioSize(audio)>48000*4/5) SDL_ClearQueuedAudio(audio);
                SDL_QueueAudio(audio,frame.audio.data(),Uint32(frame.audio.size()*sizeof(int16_t)));
            }
        }
        const auto done=*std::min_element(completed.begin(),completed.end());
        if (limit && done>=limit) { close(); return; }
        const double now=clock.nsecsElapsed()/1000000.0;
        fps=now>0 ? (double(totalFrames)/seats)*1000/now : 0;
        if (nextTime<now-250) nextTime=now; // Slow hardware never gets a catch-up burst.
        if (now>=nextTime && next-done<4 && (!limit || next<limit+8)) {
            std::vector<FrameInput> input(seats);
            for (auto& f:input) f.frame=next;
            auto& local=input[selected];
            const bool focused = isActiveWindow();
            for (int bit=0;bit<12;++bit) if (focused && held(bit)) local.keys &= ~(1u<<bit);
            for (const auto& [id,pos]:pointers) if (focused && screenRect(1).contains(pos)) {
                const auto r=screenRect(1); local.touching=true;
                local.x=qBound(0,int((pos.x()-r.x())*Width/r.width()),Width-1);
                local.y=qBound(0,int((pos.y()-r.y())*Height/r.height()),Height-1); break;
            }
            const std::array<SDL_GameControllerButton,12> buttons={SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_A,
                SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_START,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
                SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_X};
            unsigned index=0;
            for (const auto& [id,pad]:controllers) {
                auto& f=input[controllers.size()==1 ? selected : index];
                uint32_t pressed=0;
                for (int bit=0;bit<12;++bit) if (SDL_GameControllerGetButton(pad,buttons[bit])) pressed |= 1u<<bit;
                if (!focused) neutralRequired.insert(id);
                else if (neutralRequired.count(id)) {
                    if (!pressed) neutralRequired.erase(id);
                } else f.keys &= ~pressed;
                ++index; if (index>=seats) break;
            }
            if (session->Submit(input)) {
                if (trace.isOpen()) {
                    const auto line=InputTraceLine(input);
                    if (trace.write(line.data(),qint64(line.size()))!=qint64(line.size()) || !trace.flush()) {
                        result=2; close(); return;
                    }
                    if (next+1>=MaxTraceFrames) { close(); return; }
                }
                ++next; nextTime+=FrameMs;
            }
        }
        update();
    }
    unsigned seats, limit, selected=0, next=0;
    bool diagnostics=false;
    std::vector<uint32_t> completed;
    std::vector<QImage> images;
    std::vector<uint32_t> shown;
    std::unique_ptr<NativeSession> session;
    std::map<SDL_JoystickID,SDL_GameController*> controllers;
    std::set<SDL_JoystickID> neutralRequired;
    std::set<int> keyboard;
    std::map<int,QPointF> pointers;
    SDL_AudioDeviceID audio=0;
    QTimer timer;
    QElapsedTimer clock;
    double nextTime=0, fps=0;
    uint64_t totalFrames=0;
    QString error;
    QFile trace;
};
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCommandLineParser parser; parser.addHelpOption();
    parser.addPositionalArgument("rom","Existing local NDS cartridge (read only)");
    parser.addOptions({{QStringLiteral("seats"),"Local consoles, 2 to 4","count","2"},
                       {QStringLiteral("frames"),"Stop after bounded local frames; 0 means interactive","count","0"},
                       {QStringLiteral("interpreter"),"Disable JIT for this local session"},
                       {QStringLiteral("diagnostics"),"Print bounded wireless counters without packet contents"},
                       {QStringLiteral("trace-inputs"),"Record bounded input to a new local file for developer replay","path"},
                       {QStringLiteral("fullscreen"),"Use the complete display"}});
    parser.process(app);
    try {
        bool validSeats=false,validFrames=false;
        const unsigned seats=parser.value("seats").toUInt(&validSeats),frames=parser.value("frames").toUInt(&validFrames);
        if (parser.positionalArguments().size()!=1 || !validSeats || !validFrames || seats<2 || seats>4 || frames>36000)
            throw std::runtime_error("Expected a local cartridge, 2-4 seats and 0-36000 frames");
        QFile file(parser.positionalArguments().front());
        if (!file.open(QIODevice::ReadOnly) || file.size()<0x200 || file.size()>512*1024*1024)
            throw std::runtime_error("Cannot read local cartridge");
        auto bytes=file.readAll(); if (bytes.size()!=file.size()) throw std::runtime_error("Incomplete cartridge read");
        std::vector<uint8_t> rom(bytes.begin(),bytes.end()); bytes.clear(); file.close();
        std::signal(SIGTERM,interrupt); std::signal(SIGINT,interrupt);
        Player player(std::move(rom),seats,frames,!parser.isSet("interpreter"),parser.isSet("diagnostics"),parser.value("trace-inputs"));
        if (parser.isSet("fullscreen")) player.showFullScreen(); else player.show();
        app.exec();
        std::cout << "DS_LOCAL_EXIT result=" << player.result << '\n';
        return player.result;
    } catch (const std::exception& e) { std::cerr << "DS_LOCAL_ERROR " << e.what() << '\n'; return 1; }
}
