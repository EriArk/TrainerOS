// SPDX-License-Identifier: GPL-3.0-or-later
// Player frontend for the maintained native workers. GameParty authorizes the
// session; this process never writes original saves or transfers ROM/firmware.
#include "NativeSession.h"
#include "InputTrace.h"
#include "PreparedSession.h"
#include "RemoteSession.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCloseEvent>
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
    Player(std::vector<uint8_t> rom, unsigned seats, unsigned limit, bool jit, bool diagnostics,
           const QString& tracePath, std::optional<PreparedSession> prepared, const QJsonObject& networkConfig,
           InputTrace playback)
        : seats(seats), limit(limit), diagnostics(diagnostics), completed(seats, 0), images(seats), shown(seats, 0),
          prepared(std::move(prepared)),playback(std::move(playback))
    {
        if (!tracePath.isEmpty()) {
            trace.setFileName(tracePath);
            if (!trace.open(QIODevice::WriteOnly | QIODevice::NewOnly))
                throw std::runtime_error("Input trace must be a new writable file");
            trace.write(QByteArray("TRAINEROS_DS_INPUT_V1 ")+QByteArray::number(seats)+'\n');
        }
        setWindowTitle("TrainerOS · Nintendo DS");
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
        if (this->prepared) {
            selected=unsigned(this->prepared->ownerSeat);
            if(!networkConfig.isEmpty()) {
                if(seats!=2)throw std::runtime_error("The network player currently admits two devices");
                QJsonArray states;
                for(const auto& state:this->prepared->startup.seats)
                    states.append(state?StateHash(QByteArray(reinterpret_cast<const char*>(state->data()),qsizetype(state->size()))):QString("fresh"));
                QJsonObject admission{{"protocol",1},{"content",this->prepared->contentHash},
                    {"runtime",StateHash(ReadStagedFile(QCoreApplication::applicationFilePath(),64*1024*1024))},
                    {"jit",jit},{"firmware","generated-v1-rtc-20260101"},{"states",states}};
                std::optional<RemoteSession::OwnerSeed> ownerSeed;
                if(this->prepared->peerPending) {
                    MachineContext context;
                    const auto cart=melonDS::NDSCart::ParseROM(rom.data(),uint32_t(rom.size()),&context);
                    if(!cart)throw std::runtime_error("Local cartridge rejected");
                    ownerSeed=RemoteSession::OwnerSeed{cart->GetSaveMemoryLength(),this->prepared->startup.seats[selected]};
                    admission.remove("states");admission["stateExchange"]="owned-cartridge-v1";
                    admission["saveBytes"]=qint64(ownerSeed->expectedSize);
                }
                network=std::make_unique<RemoteSession>(networkConfig,
                    StateHash(QJsonDocument(admission).toJson(QJsonDocument::Compact)),selected,
                    [](QJsonObject endpoint){
                        // Private child output: the ordinary launcher routes this
                        // through accepted GameParty admission, never a user log.
                        std::cout<<"TRAINEROS_DS_ENDPOINT "
                                 <<QJsonDocument(endpoint).toJson(QJsonDocument::Compact).toBase64().constData()<<std::endl;
                    },std::move(ownerSeed));
            }
            pendingStartup=std::move(this->prepared->startup);
        }
        pendingArgs=std::move(args);pendingRom=std::move(rom);
        if(!this->prepared||!this->prepared->peerPending)createMachines();
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
    void Abort() {result=3;close();}
protected:
    void closeEvent(QCloseEvent* event) override {
        if(network&&!network->Ready()&&!result) {
            result=3;QWidget::closeEvent(event);return;
        }
        if (prepared && !result && (!returnComplete || (network&&!network->Finished()))) {
            event->ignore();
            if (!finishing) {
                if(network) {
                    network->Finish(next);finishing=true;returnDeadline=clock.elapsed()+15000;
                    keyboard.clear();pointers.clear();gateControllers();
                    error="Finishing the session together...";return;
                }
                if (!session->RequestOwnedReturn(next)) {
                    result=2; error="Could not finish the session. Your original save is unchanged.";
                    timer.stop(); session->Stop(); update(); return;
                }
                finishing=true; returnDeadline=clock.elapsed()+5000;
                keyboard.clear(); pointers.clear(); gateControllers();
                error="Finishing your local session...";
            }
            return;
        }
        QWidget::closeEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#f5eddb"));
        QFont font = p.font(); font.setPixelSize(qMax(14, height() / 36)); p.setFont(font);
        if(network)button(p,tab(selected),QString("Player %1").arg(selected+1),true);
        else for (unsigned i = 0; i < seats; ++i) button(p, tab(i), QString("DS %1").arg(i + 1), selected == i);
        button(p, exitRect(), "Exit", false);
        p.setPen(QColor("#37484d"));
        p.drawText(QRectF(20, 62, width() - 40, 35), Qt::AlignCenter,
                   error.isEmpty() ? (network ? (network->Ready()?"Playing together":"Connecting to your friend...")
                                             : prepared ? "Local session · progress kept on exit"
                                              : "Temporary local session · progress is not saved") : error);
        for (int screen = 0; screen < 2; ++screen) {
            auto target = screenRect(screen);
            p.fillRect(target.adjusted(-4,-4,4,4), QColor("#6b7f88"));
            p.fillRect(target, Qt::black);
            if (!images[selected].isNull()) p.drawImage(target, images[selected], QRectF(0, screen * Height, Width, Height));
        }
        for (const auto& c : controls()) button(p, c.rect, c.label, held(c.bit));
        p.setPen(QColor("#37484d"));
        p.drawText(QRectF(20, height()-27, width()-40, 25), Qt::AlignCenter,
                   diagnostics ? QString("DS %1 of %2 · %3 fps · touch the right screen").arg(selected+1).arg(seats).arg(fps,0,'f',1)
                               : QString("Touch the right screen to play"));
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
    void createMachines() {
        session=std::make_unique<NativeSession>(std::move(pendingArgs),pendingRom,SessionOptions{60,true},std::move(pendingStartup));
        pendingRom.clear();pendingRom.shrink_to_fit();
        if(network)network->MachinesReady();
    }
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
        if(network)return;
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
        if (interrupted) { if (prepared) result=3; close(); return; }
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
        if(network) {
            network->Tick(next);
            if(network->Failed()) {
                std::cout<<"[TrainerOSDs] error"<<std::endl;
                if(diagnostics)std::cout<<"DS_NETWORK_FAIL "<<network->Failure().toStdString()<<std::endl;
                result=2;error="The connection stopped or the games differed. Your original save is unchanged.";
                timer.stop();if(session)session->Stop();update();return;
            }
            if(!session&&network->StartupReady()) {
                try {
                    pendingStartup->seats[1-selected]=network->TakePeerState();
                    createMachines();
                } catch(const std::exception&) {
                    result=2;error="Could not prepare the DS consoles. Your original save is unchanged.";
                    timer.stop();network.reset();update();return;
                }
            }
            if(!network->Ready()){update();return;}
            if(!networkReported){
                networkReported=true;
                std::cout<<"[TrainerOSDs] running"<<std::endl;
                if(diagnostics)std::cout<<"DS_NETWORK_READY delay="<<network->Delay()<<std::endl;
            }
            if(network->Finishing()&&!finishing) {
                finishing=true;returnDeadline=clock.elapsed()+15000;error="Finishing the session together...";
                keyboard.clear();pointers.clear();gateControllers();
            }
            if(network->EndFrame()&&!returnRequested) {
                if(!session->RequestOwnedReturn(*network->EndFrame())) {
                    result=2;close();return;
                }
                returnRequested=true;
            }
            if(returnComplete) {
                if(network->Finished()){
                    if(diagnostics)std::cout<<"DS_NETWORK_EXIT checkpoints="<<network->VerifiedChecks()<<std::endl;
                    timer.stop();close();return;
                }
                if(clock.elapsed()>returnDeadline){result=2;close();return;}
                update();return;
            }
        }
        FrameResult resultFrame;
        while (session->Take(resultFrame,std::chrono::milliseconds(0))) {
            if(network)network->Checkpoint(resultFrame);
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
        if (finishing) {
            OwnerCartridgeReturn value;
            try {
                if ((!network||network->ReturnSafe()) && session->TakeOwnedReturn(value)) {
                    WriteOwnerReturn(*prepared,value);
                    returnComplete=true;
                    if(network){network->OwnerRetained();return;}
                    timer.stop(); close(); return;
                }
                if (clock.elapsed()>returnDeadline) throw std::runtime_error("Local session exit timed out");
            } catch (const std::exception&) {
                result=2; error="Could not retain session progress. Your original save is unchanged.";
                timer.stop(); session->Stop(); update(); return;
            }
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
        if (limit && done>=limit && !finishing) { close(); return; }
        const double now=clock.nsecsElapsed()/1000000.0;
        fps=now>0 ? (double(totalFrames)/seats)*1000/now : 0;
        if (nextTime<now-250) nextTime=now; // Slow hardware never gets a catch-up burst.
        if (now>=nextTime && next-done<4 && (!limit || finishing || next<limit+8)) {
            std::vector<FrameInput> input(seats);
            for (auto& f:input) f.frame=next;
            auto& local=input[selected];
            const bool focused = isActiveWindow() && !finishing;
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
                auto& f=input[network||controllers.size()==1 ? selected : index];
                uint32_t pressed=0;
                for (int bit=0;bit<12;++bit) if (SDL_GameControllerGetButton(pad,buttons[bit])) pressed |= 1u<<bit;
                if (!focused) neutralRequired.insert(id);
                else if (neutralRequired.count(id)) {
                    if (!pressed) neutralRequired.erase(id);
                } else f.keys &= ~pressed;
                ++index; if (network||index>=seats) break;
            }
            if(!playback.empty()) {
                if(next<playback.size())input=playback[next];
                else {input.assign(seats,FrameInput{});for(auto& frame:input)frame.frame=next;}
            }
            if(network&&!network->Inputs(next,input[selected],input)){update();return;}
            if (session->Submit(input)) {
                if(network)network->Committed(next);
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
    std::vector<melonDS::NDSArgs> pendingArgs;
    std::vector<uint8_t> pendingRom;
    std::optional<CartridgeStartup> pendingStartup;
    std::unique_ptr<RemoteSession> network;
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
    std::optional<PreparedSession> prepared;
    bool finishing=false, returnComplete=false, returnRequested=false;
    bool networkReported=false;
    qint64 returnDeadline=0;
    InputTrace playback;
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
                       {QStringLiteral("prepared-session"),"Private local launcher staging directory","path"},
                       {QStringLiteral("network-session"),"Private accepted-party transport configuration","path"},
                       {QStringLiteral("replay-inputs"),"Developer-only bounded local input recording","path"},
                       {QStringLiteral("fullscreen"),"Use the complete display"}});
    parser.process(app);
    try {
        bool validSeats=false,validFrames=false;
        const unsigned seats=parser.value("seats").toUInt(&validSeats),frames=parser.value("frames").toUInt(&validFrames);
        if (parser.positionalArguments().size()!=1 || !validSeats || !validFrames || seats<2 || seats>4 || frames>72000)
            throw std::runtime_error("Expected a local cartridge, 2-4 seats and 0-72000 frames");
        QFile file(parser.positionalArguments().front());
        if (!file.open(QIODevice::ReadOnly) || file.size()<0x200 || file.size()>512*1024*1024)
            throw std::runtime_error("Cannot read local cartridge");
        auto bytes=file.readAll(); if (bytes.size()!=file.size()) throw std::runtime_error("Incomplete cartridge read");
        std::optional<PreparedSession> prepared;
        if (parser.isSet("prepared-session"))
            prepared=ReadPreparedSession(parser.value("prepared-session"),StateHash(bytes),seats,parser.isSet("network-session"));
        QJsonObject networkConfig;
        if(parser.isSet("network-session")) {
            if(!prepared||seats!=2)throw std::runtime_error("Network play requires a prepared two-player session");
            QJsonParseError error;
            const auto config=QJsonDocument::fromJson(ReadStagedFile(parser.value("network-session"),4096),&error);
            networkConfig=config.object();
            if(error.error!=QJsonParseError::NoError||networkConfig.isEmpty())throw std::runtime_error("Invalid network session");
        }
        std::vector<uint8_t> rom(bytes.begin(),bytes.end()); bytes.clear(); file.close();
        std::signal(SIGTERM,interrupt); std::signal(SIGINT,interrupt);
        const auto playback=parser.isSet("replay-inputs")?ReadInputTrace(parser.value("replay-inputs").toStdString(),seats):InputTrace{};
        Player player(std::move(rom),seats,frames,!parser.isSet("interpreter"),parser.isSet("diagnostics"),
                      parser.value("trace-inputs"),std::move(prepared),networkConfig,playback);
        // Failure screens intentionally stop emulation's timer. Process/session
        // shutdown must remain responsive while such a screen is displayed.
        QTimer interrupts;
        QObject::connect(&interrupts,&QTimer::timeout,&player,[&player]{if(interrupted)player.Abort();});
        interrupts.start(100);
        if (parser.isSet("fullscreen")) player.showFullScreen(); else player.show();
        app.exec();
        std::cout << "DS_LOCAL_EXIT result=" << player.result << '\n';
        return player.result;
    } catch (const std::exception& e) { std::cerr << "DS_LOCAL_ERROR " << e.what() << '\n'; return 1; }
}
