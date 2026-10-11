// SPDX-License-Identifier: GPL-3.0-or-later
// Headless verification host only. Core file access, persistent saves, WFC,
// camera, microphone and physical output are deliberately unavailable here.
#include "NativeSession.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace melonDS::Platform {
using traineros::ds::MachineContext;
MachineContext& Context(void* data) { return *static_cast<MachineContext*>(data); }
void SignalStop(StopReason, void* data) { if (data) Context(data).stopped = true; }
void Log(LogLevel, const char*, ...) {} // Do not emit core memory/save diagnostics.

struct Thread { std::thread value; };
Thread* Thread_Create(std::function<void()> fn) { return new Thread {std::thread(std::move(fn))}; }
void Thread_Wait(Thread* t) { if (t && t->value.joinable()) t->value.join(); }
void Thread_Free(Thread* t) { Thread_Wait(t); delete t; }
struct Semaphore { std::mutex lock; std::condition_variable cv; int count = 0; };
Semaphore* Semaphore_Create() { return new Semaphore; }
void Semaphore_Free(Semaphore* s) { delete s; }
void Semaphore_Reset(Semaphore* s) { std::lock_guard<std::mutex> g(s->lock); s->count = 0; }
void Semaphore_Wait(Semaphore* s) {
    std::unique_lock<std::mutex> g(s->lock);
    s->cv.wait(g, [&] { return s->count > 0; }); --s->count;
}
bool Semaphore_TryWait(Semaphore* s, int ms) {
    std::unique_lock<std::mutex> g(s->lock);
    if (!s->cv.wait_for(g, std::chrono::milliseconds(std::max(ms, 0)), [&] { return s->count > 0; })) return false;
    --s->count; return true;
}
void Semaphore_Post(Semaphore* s, int n) {
    std::lock_guard<std::mutex> g(s->lock); s->count += n; s->cv.notify_all();
}
struct Mutex { std::mutex value; };
Mutex* Mutex_Create() { return new Mutex; }
void Mutex_Free(Mutex* m) { delete m; }
void Mutex_Lock(Mutex* m) { m->value.lock(); }
void Mutex_Unlock(Mutex* m) { m->value.unlock(); }
bool Mutex_TryLock(Mutex* m) { return m->value.try_lock(); }
void Sleep(u64 us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }
u64 GetUSCount() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
u64 GetMSCount() { return GetUSCount() / 1000; }

std::string GetLocalFilePath(const std::string&) { return {}; }
FileHandle* OpenFile(const std::string&, FileMode) { return nullptr; }
FileHandle* OpenLocalFile(const std::string&, FileMode) { return nullptr; }
bool FileExists(const std::string&) { return false; }
bool LocalFileExists(const std::string&) { return false; }
bool CheckFileWritable(const std::string&) { return false; }
bool CheckLocalFileWritable(const std::string&) { return false; }
bool CloseFile(FileHandle*) { return false; }
bool IsEndOfFile(FileHandle*) { return true; }
bool FileReadLine(char*, int, FileHandle*) { return false; }
u64 FilePosition(FileHandle*) { return 0; }
bool FileSeek(FileHandle*, s64, FileSeekOrigin) { return false; }
void FileRewind(FileHandle*) {}
u64 FileRead(void*, u64, u64, FileHandle*) { return 0; }
bool FileFlush(FileHandle*) { return false; }
u64 FileWrite(const void*, u64, u64, FileHandle*) { return 0; }
u64 FileWriteFormatted(FileHandle*, const char*, ...) { return 0; }
u64 FileLength(FileHandle*) { return 0; }
void WriteNDSSave(const u8*, u32, u32, u32, void* data) { ++Context(data).saveNotifications; }
void WriteGBASave(const u8*, u32, u32, u32, void* data) { ++Context(data).saveNotifications; }
void WriteFirmware(const Firmware&, u32, u32, void*) {}
void WriteDateTime(int, int, int, int, int, int, void*) {}

void MP_Tick(void* data) {
    auto& c = Context(data);
    if (c.console) c.wireless->SetNow(c.seat, c.console->GetSysTimestamp());
}
void MP_Begin(void* data) { MP_Tick(data); auto& c = Context(data); c.wireless->Begin(c.seat); }
void MP_End(void* data) { MP_Tick(data); auto& c = Context(data); c.wireless->End(c.seat); }
int MP_SendPacket(u8* p, int n, u64 t, void* data) { MP_Tick(data); auto& c = Context(data); return c.wireless->SendPacket(c.seat,p,n,t); }
int MP_RecvPacket(u8* p, u64* t, void* data) { MP_Tick(data); auto& c = Context(data); return c.wireless->RecvPacket(c.seat,p,t); }
int MP_SendCmd(u8* p, int n, u64 t, void* data) { MP_Tick(data); auto& c = Context(data); return c.wireless->SendCmd(c.seat,p,n,t); }
int MP_SendReply(u8* p, int n, u64 t, u16 aid, void* data) { MP_Tick(data); auto& c = Context(data); return c.wireless->SendReply(c.seat,p,n,t,aid); }
int MP_SendAck(u8* p, int n, u64 t, void* data) { MP_Tick(data); auto& c = Context(data); return c.wireless->SendAck(c.seat,p,n,t); }
int MP_RecvHostPacket(u8* p, u64* t, void* data) { MP_Tick(data); auto& c = Context(data); return c.wireless->RecvHostPacket(c.seat,p,t); }
u16 MP_RecvReplies(u8* p, u64 t, u16 mask, void* data) { MP_Tick(data); auto& c = Context(data); return c.wireless->RecvReplies(c.seat,p,t,mask); }
int Net_SendPacket(u8*, int, void*) { return 0; }
int Net_RecvPacket(u8*, void*) { return 0; }
void Camera_Start(int, void*) {}
void Camera_Stop(int, void*) {}
void Camera_CaptureFrame(int, u32* p, int w, int h, bool, void*) { if (p && w > 0 && h > 0) std::fill_n(p, size_t(w)*h, 0); }
void Mic_Start(void*) {}
void Mic_Stop(void*) {}
int Mic_ReadInput(s16*, int, void*) { return 0; }
AACDecoder* AAC_Init() { return nullptr; }
void AAC_DeInit(AACDecoder*) {}
bool AAC_Configure(AACDecoder*, int, int) { return false; }
bool AAC_DecodeFrame(AACDecoder*, const void*, int, void*, int) { return false; }
bool Addon_KeyDown(KeyType, void*) { return false; }
void Addon_RumbleStart(u32, void*) {}
void Addon_RumbleStop(void*) {}
float Addon_MotionQuery(MotionQueryType, void*) { return 0; }
DynamicLibrary* DynamicLibrary_Load(const char*) { return nullptr; }
void DynamicLibrary_Unload(DynamicLibrary*) {}
void* DynamicLibrary_LoadFunction(DynamicLibrary*, const char*) { return nullptr; }
}
