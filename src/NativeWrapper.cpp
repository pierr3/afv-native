#include "afv-native/atisClient.h"
#include <atomic>
#include <event2/event.h>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace atisapi {
#ifdef WIN32
#define AFV_API __declspec(dllexport)
#else
#define AFV_API
#endif

    struct event_base *ev_base;

    std::mutex afvMutex;
    std::unique_ptr<afv_native::ATISClient> client;
    std::unique_ptr<std::thread> eventThread;
    std::atomic<bool> keepAlive {false};
    std::atomic<bool> isInitialized {false};
} // namespace atisapi

using namespace atisapi;

#ifdef __cplusplus
extern "C" {

AFV_API void Initialize(char *atisFile, char *clientName) {
#ifdef WIN32
    WORD    wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);
#endif

    ev_base = event_base_new();
    client = std::make_unique<afv_native::ATISClient>(ev_base, atisFile, clientName);

    keepAlive = true;
    eventThread = std::make_unique<std::thread>([] {
        while (keepAlive) {
            event_base_loop(ev_base, EVLOOP_NONBLOCK);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    isInitialized = true;
}

AFV_API void Shutdown() {
    keepAlive = false;
        if (eventThread->joinable()) {
            eventThread->join();
    }
    client.reset();
    isInitialized = false;
#ifdef WIN32
    WSACleanup();
#endif
}

AFV_API bool IsInitialized() {
    return isInitialized;
}

AFV_API void SetCredentials(const char *username, const char *password) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setCredentials(std::string(username), std::string(password));
}

AFV_API void SetCallsign(const char *callsign) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setCallsign(std::string(callsign));
}

AFV_API bool IsVoiceConnected() {
    return client->isVoiceConnected();
}

AFV_API bool IsAPIConnected() {
    return client->isAPIConnected();
}

AFV_API bool ConnectApi() {
    return client->connect();
}

AFV_API void DisconnectApi() {
    client->disconnect();
}

AFV_API void StartAudio() {
    client->startAudio();
}

AFV_API void StopAudio() {
    client->stopAudio();
}

AFV_API bool IsPlaying() {
    return client->isPlaying();
}

AFV_API void SetClientPosition(double lat, double lon, double amslm, double aglm) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setClientPosition(lat, lon, amslm, aglm);
}

AFV_API void SetFrequency(unsigned int frequency) {
    std::lock_guard<std::mutex> lock(afvMutex);
    client->setFrequency(frequency);
}

AFV_API void RaiseClientEvent(void (*callback)(afv_native::ClientEventType evt, int data)) {
    client->ClientEventCallback.addCallback(nullptr, [callback](afv_native::ClientEventType evt, void *data) {
        int v = -1;
            if (data != nullptr) {
                v = *reinterpret_cast<int *>(data);
        }
        callback(evt, v);
    });
}
}
#endif