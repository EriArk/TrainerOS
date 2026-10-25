#include "DeviceService.h"
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <algorithm>

namespace trainer {
namespace {
int number(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return -1;
    const auto text = file.read(32).trimmed();
    static const QRegularExpression valid("^[0-9]{1,9}$");
    return valid.match(QString::fromLatin1(text)).hasMatch() ? text.toInt() : -1;
}
#ifdef Q_OS_LINUX
QByteArray command(const QString& program, const QStringList& arguments, int limit=4096) {
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C");
    process.setProcessEnvironment(environment);
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, arguments);
    if (!process.waitForStarted(1000) || !process.waitForFinished(1500)) {
        process.kill(); process.waitForFinished(1000); return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return {};
    const auto bytes=process.read(limit+1);
    return bytes.size()>limit ? QByteArray() : bytes;
}
QString changeCommand(const QStringList& arguments, const QString& program="/usr/bin/wpctl") {
    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, arguments);
    if (!process.waitForStarted(1000) || !process.waitForFinished(1500)) {
        process.kill(); process.waitForFinished(1000);
        return "The audio control did not respond. Try again.";
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 ? QString()
        : "The audio setting could not be changed. Check the device and try again.";
}
#endif
void storage(const QString& directory, qint64& free, qint64& total) {
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) return;
    QStorageInfo volume(directory);
    if (volume.isValid() && volume.isReady() && volume.bytesTotal() > 0 && volume.bytesAvailable() >= 0) {
        free = volume.bytesAvailable(); total = volume.bytesTotal();
    }
}
}
BacklightValue readBacklight(const QString& root) {
    QDirIterator entries(root, QDir::Dirs | QDir::NoDotAndDotDot);
    BacklightValue found; int count = 0;
    while (entries.hasNext()) {
        const auto directory = entries.next();
        if (++count > 16) return {};
        const int maximum = number(QDir(directory).filePath("max_brightness"));
        const int current = number(QDir(directory).filePath("brightness"));
        if (maximum < 1 || current < 0 || current > maximum) continue;
        if (!found.directory.isEmpty()) return {};
        found = {directory, current, maximum};
    }
    return found;
}
QString writeBacklight(const QString& root, int percent) {
    if (percent < 5 || percent > 100) return "Brightness must be between 5% and 100%.";
    const auto light = readBacklight(root);
    if (light.directory.isEmpty()) return "The screen brightness control is unavailable.";
    QFile file(QDir(light.directory).filePath("brightness"));
    if (!file.open(QIODevice::WriteOnly)) return "The screen brightness could not be changed.";
    const int value = std::clamp(int(std::lround(double(light.maximum) * percent / 100.0)), 1, light.maximum);
    const auto data = QByteArray::number(value) + '\n';
    if (file.write(data) != data.size() || !file.flush()) return "The screen brightness could not be changed.";
    return {};
}
DeviceSnapshot parseVolume(const QByteArray& output) {
    DeviceSnapshot result;
    static const QRegularExpression pattern("^Volume: ([0-9]+(?:\\.[0-9]+)?)( \\[MUTED\\])?$");
    const auto match = pattern.match(QString::fromUtf8(output.trimmed()));
    if (!match.hasMatch()) return result;
    bool valid = false; const double volume = match.captured(1).toDouble(&valid);
    if (!valid || !std::isfinite(volume) || volume < 0 || volume > 10) return result;
    result.volume = int(std::lround(volume * 100));
    result.muted = !match.captured(2).isEmpty();
    return result;
}
QVariantList parseAudioOutputs(const QByteArray& output) {
    QVariantList result;
    if(output.size()>1024*1024)return result;
    for(const auto& value:QJsonDocument::fromJson(output).array()){
        const auto item=value.toObject();const auto id=item["name"].toString();
        if(id.isEmpty()||id.size()>512)continue;
        // PipeWire's automatic fallback cannot play sound. Keep intentional
        // virtual outputs, but do not present this placeholder as a device.
        if(id=="auto_null"&&item["properties"].toObject()["factory.name"]=="support.null-audio-sink")continue;
        bool available=true;
        for(const auto& p:item["ports"].toArray()){
            const auto port=p.toObject();
            if(port["name"]==item["active_port"]&&port["availability"]=="not available")available=false;
        }
        result.append(QVariantMap{{"id",id},{"title",item["description"].toString(id)},{"available",available}});
    }
    return result;
}
QVariantList parseAudioStreams(const QByteArray& output) {
    QVariantList result;if(output.size()>1024*1024)return result;
    for(const auto& value:QJsonDocument::fromJson(output).array()) {
        const auto item=value.toObject(),props=item["properties"].toObject();
        if(!item["index"].isDouble()||item["index"].toDouble()<0)continue;
        const auto index=QString::number(qint64(item["index"].toDouble()));
        const auto serial=props["object.serial"].toString();
        // A recycled sink-input index must never target a different process.
        const auto identity=index+":"+serial+":"+props["application.process.id"].toString()+":"+props["application.process.binary"].toString()+":"+props["application.name"].toString();
        int volume=-1;
        const auto channels=item["volume"].toObject();
        for(auto it=channels.begin();it!=channels.end();++it) {
            const auto raw=it.value().toObject()["value"];
            if(raw.isDouble()&&raw.toDouble()>=0)volume=std::max(volume,int(std::lround(raw.toDouble()*100/65536)));
        }
        if(volume<0)continue;
        auto title=props["application.name"].toString();if(title.isEmpty())title=props["application.process.binary"].toString("Audio application");
        result.append(QVariantMap{{"id",identity},{"index",index},{"title",title.left(100)},{"detail",props["media.name"].toString().left(120)},
            {"level",volume},{"muted",item["mute"].toBool()},{"available",!serial.isEmpty()}});
        if(result.size()>=64)break;
    }return result;
}
QVariantList parseAudioInputs(const QByteArray& output) {
    QVariantList result;if(output.size()>1024*1024)return result;
    for(const auto& value:QJsonDocument::fromJson(output).array()) {
        const auto item=value.toObject();const auto id=item["name"].toString();
        if(id.isEmpty()||id.size()>512||id.endsWith(".monitor")||(!item["monitor_of_sink"].isNull()&&item["monitor_of_sink"].toInt(-1)>=0))continue;
        int volume=-1;const auto channels=item["volume"].toObject();
        for(auto it=channels.begin();it!=channels.end();++it){const auto raw=it.value().toObject()["value"];if(raw.isDouble()&&raw.toDouble()>=0)volume=std::max(volume,int(std::lround(raw.toDouble()*100/65536)));}
        bool available=volume>=0;
        for(const auto& p:item["ports"].toArray())if(p.toObject()["name"]==item["active_port"]&&p.toObject()["availability"]=="not available")available=false;
        result.append(QVariantMap{{"id",id},{"title",item["description"].toString(id).left(100)},{"level",volume},{"muted",item["mute"].toBool()},{"available",available}});
        if(result.size()>=32)break;
    }return result;
}
DeviceBackend systemDeviceBackend(const QString& dataDirectory, const QString& libraryDirectory) {
    return {[dataDirectory, libraryDirectory] {
        DeviceSnapshot result;
#ifdef Q_OS_LINUX
        result = parseVolume(command("/usr/bin/wpctl", {"get-volume", "@DEFAULT_AUDIO_SINK@"}));
        result.audioOutputs=parseAudioOutputs(command("/usr/bin/pactl",{"-f","json","list","sinks"},1024*1024));
        result.audioOutput=QString::fromUtf8(command("/usr/bin/pactl",{"get-default-sink"})).trimmed();
        result.audioStreams=parseAudioStreams(command("/usr/bin/pactl",{"-f","json","list","sink-inputs"},1024*1024));
        result.audioInputs=parseAudioInputs(command("/usr/bin/pactl",{"-f","json","list","sources"},1024*1024));
        result.audioInput=QString::fromUtf8(command("/usr/bin/pactl",{"get-default-source"})).trimmed();
        const auto light = readBacklight("/sys/class/backlight");
        if (light.maximum > 0) result.brightness = int(std::lround(double(light.current) * 100 / light.maximum));
        const auto network = command("/usr/bin/nmcli", {"-t", "-f", "STATE", "general"}).trimmed();
        result.network = network == "connected" ? "Connected" : network.startsWith("connected") ? "Limited connection"
            : network == "connecting" ? "Connecting" : network == "disconnected" || network == "asleep" ? "Offline" : "Unavailable";
        const auto radios = QJsonDocument::fromJson(command("/usr/bin/python3", {"-I", "/var/opt/traineros/integrations/radio-control.py", "status"})).object();
        result.wifi = radios.value("wifi").toInt(-1);
        result.bluetooth = radios.value("bluetooth").toInt(-1);
        result.airplane = radios.value("airplane").toInt(-1);
#endif
        storage(dataDirectory, result.internalFree, result.internalTotal);
        storage(libraryDirectory, result.libraryFree, result.libraryTotal);
        return result;
    }, [](const QString& control, int value) -> QString {
#ifdef Q_OS_LINUX
        if ((control == "wifi" || control == "bluetooth" || control == "airplane") && (value == 0 || value == 1)) {
            QProcess process;
            process.setStandardOutputFile(QProcess::nullDevice()); process.setStandardErrorFile(QProcess::nullDevice());
            process.start("/usr/bin/sudo", {"-n", "/var/opt/traineros/integrations/radio-control.py", control + (value ? "-on" : "-off")});
            if (!process.waitForStarted(1000) || !process.waitForFinished(15000)) {
                process.kill(); process.waitForFinished(1000); return "The radio control did not respond. Try again.";
            }
            return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 ? QString()
                : "Couldn't change the radio. Check the device connection and try again.";
        }
        if (control == "brightness") return writeBacklight("/sys/class/backlight", value);
        if (control == "volume" && value >= 0 && value <= 100)
            return changeCommand({"set-volume", "@DEFAULT_AUDIO_SINK@", QString::number(value / 100.0, 'f', 2)});
        if (control == "mute" && (value == 0 || value == 1))
            return changeCommand({"set-mute", "@DEFAULT_AUDIO_SINK@", QString::number(value)});
#else
        Q_UNUSED(control); Q_UNUSED(value);
#endif
        return "This device control is unavailable.";
    }, [](const QString& id)->QString {
#ifdef Q_OS_LINUX
        return changeCommand({"set-default-sink",id},"/usr/bin/pactl");
#else
        Q_UNUSED(id);return "Audio output selection is unavailable on this host.";
#endif
    },[](const QString& index,int value,bool mute) -> QString {
#ifdef Q_OS_LINUX
        static const QRegularExpression numeric("^[0-9]{1,12}$");
        if(!numeric.match(index).hasMatch())return "This audio source is unavailable.";
        return changeCommand({mute?"set-sink-input-mute":"set-sink-input-volume",index,mute?QString::number(bool(value)):QString::number(std::clamp(value,0,100))+"%"},"/usr/bin/pactl");
#else
        return "Separate audio sources are unavailable on this host.";
#endif
    },[](const QString& id,int value,const QString& operation) -> QString {
#ifdef Q_OS_LINUX
        if(operation=="default")return changeCommand({"set-default-source",id},"/usr/bin/pactl");
        return changeCommand({operation=="mute"?"set-source-mute":"set-source-volume",id,operation=="mute"?QString::number(bool(value)):QString::number(std::clamp(value,0,100))+"%"},"/usr/bin/pactl");
#else
        return "Microphone controls are unavailable on this host.";
#endif
    }};
}
DeviceService::DeviceService(DeviceBackend backend, QObject* parent)
    : QObject(parent), backend_(std::move(backend)), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
    audioDebounce_.setSingleShot(true);audioDebounce_.setInterval(150);
    connect(&audioDebounce_,&QTimer::timeout,this,[this]{if(busy_)audioDebounce_.start();else refresh();});
    connect(&audioEvents_,&QProcess::readyReadStandardOutput,this,[this]{audioEvents_.readAllStandardOutput();audioDebounce_.start();});
    audioEvents_.setStandardErrorFile(QProcess::nullDevice());
}
DeviceService::~DeviceService() { audioEvents_.kill();audioEvents_.waitForFinished(1000);thread_.quit(); thread_.wait(); }
void DeviceService::monitorAudio(bool enabled) {
#ifdef Q_OS_LINUX
    if(enabled&&audioEvents_.state()==QProcess::NotRunning)audioEvents_.start("/usr/bin/pactl",{"subscribe"});
    if(!enabled){audioDebounce_.stop();audioEvents_.kill();}
#endif
}
void DeviceService::setStream(const QString& id,int value,bool mute) {
    if(id.isEmpty()||value<0||value>(mute?1:100))return;
    const QString control=mute?"stream-mute":"stream-volume";
    if(busy_){if(pendingAudio_.size()<64)pendingAudio_[control+"\n"+id]={control,value,id};return;}
    execute(control,value,id);
}
void DeviceService::setInput(const QString& id,int value,const QString& operation) {
    if(id.isEmpty()||!QStringList{"volume","mute","default"}.contains(operation)||value<0||value>(operation=="volume"?100:1))return;
    if(busy_){if(pendingAudio_.size()<64)pendingAudio_["input-"+operation+"\n"+id]={"input-"+operation,value,id};return;}
    execute("input-"+operation,value,id);
}
void DeviceService::refresh() { execute({}, 0); }
int DeviceService::desired(const QString& control) const {
    if (pending_.contains(control)) return pending_.value(control);
    if (activeControl_ == control) return activeValue_;
    if (control == "volume") return snapshot_.volume;
    if (control == "brightness") return snapshot_.brightness;
    if (control == "wifi") return snapshot_.wifi;
    if (control == "bluetooth") return snapshot_.bluetooth;
    if (control == "airplane") return snapshot_.airplane;
    return snapshot_.volume < 0 ? -1 : int(snapshot_.muted);
}
void DeviceService::adjust(const QString& control, int delta) {
    if (control != "volume" && control != "brightness") return;
    const int current = desired(control);
    if (current >= 0) setValue(control, std::clamp(current + delta, control == "brightness" ? 5 : 0, 100));
}
void DeviceService::hardwareVolume(int delta) {
    if (!delta) return;
    delta = std::clamp(delta, -100, 100);
    if (busy_) { pending_["volume-step"] = std::clamp(pending_.value("volume-step") + delta, -100, 100); return; }
    execute("volume-step", delta);
}
void DeviceService::toggleMute() {
    const int current = desired("mute");
    if (current >= 0) setValue("mute", !current);
}
void DeviceService::toggleRadio(const QString& control) {
    if (control != "wifi" && control != "bluetooth" && control != "airplane") return;
    const auto current = desired(control);
    if (current >= 0) setValue(control, !current);
}
void DeviceService::setValue(const QString& control, int value) {
    const bool binary = control == "mute" || control == "wifi" || control == "bluetooth" || control == "airplane";
    if ((control != "volume" && control != "brightness" && !binary)
        || value < (control == "brightness" ? 5 : 0) || value > (binary ? 1 : 100)) return;
    if (busy_) { pending_[control] = value; return; }
    execute(control, value);
}
void DeviceService::selectOutput(const QString& id) {
    if(busy_||id.isEmpty())return;
    execute("audio-output",0,id);
}
void DeviceService::execute(const QString& control, int value, const QString& output) {
    if (busy_) return;
    busy_ = true; activeControl_ = control; activeValue_ = value;
    if (!control.isEmpty()) error_.clear();
    emit changed();
    QMetaObject::invokeMethod(worker_, [this, control, value, output] {
        QString error;
        if(control.startsWith("input-")) {
            const auto before=backend_.read();bool available=false;
            for(const auto& row:before.audioInputs)if(row.toMap()["id"]==output&&row.toMap()["available"].toBool())available=true;
            if(!available)error="This microphone is no longer available.";
            else if(!backend_.setInput)error="Microphone controls are unavailable on this host.";
            else error=backend_.setInput(output,value,control.mid(6));
        }else if(control=="stream-volume"||control=="stream-mute") {
            const auto before=backend_.read();QString index;
            for(const auto& row:before.audioStreams)if(row.toMap()["id"]==output&&row.toMap()["available"].toBool())index=row.toMap()["index"].toString();
            if(index.isEmpty())error="This audio source ended or changed. Select the current source.";
            else if(!backend_.setStream)error="Separate volume is unavailable on this host.";
            else error=backend_.setStream(index,value,control=="stream-mute");
        }else if(control=="audio-output"){
            const auto before=backend_.read();bool available=false;
            for(const auto& row:before.audioOutputs)if(row.toMap()["id"]==output&&row.toMap()["available"].toBool())available=true;
            if(!available)error="This output is no longer available. Choose another device.";
            else if(!backend_.selectOutput)error="Audio output selection is unavailable on this host.";
            else if(before.audioOutput!=output)error=backend_.selectOutput(output);
        } else if (control == "volume-step") {
            const auto before = backend_.read();
            error = before.volume < 0 ? "The audio control is unavailable."
                : backend_.write("volume", std::clamp(before.volume + value, 0, 100));
        } else if (!control.isEmpty()) error = backend_.write(control, value);
        const auto result = backend_.read();
        if(control=="audio-output"&&error.isEmpty()&&result.audioOutput!=output)error="The audio output did not change. Try again.";
        QMetaObject::invokeMethod(this, [this, error, result] {
            snapshot_ = result;
            if (!error.isEmpty()) { error_ = error; pending_.clear();pendingAudio_.clear(); }
            activeControl_.clear();
            if(!pendingAudio_.isEmpty()) {
                const auto change=pendingAudio_.take(pendingAudio_.firstKey());busy_=false;execute(change.control,change.value,change.target);
            }else if (!pending_.isEmpty()) {
                const auto control = pending_.firstKey(); const auto value = pending_.take(control);
                busy_ = false; execute(control, value); // No idle signal between queued writes.
            } else { busy_ = false; emit changed(); }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
