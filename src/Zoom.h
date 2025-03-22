#ifndef MEETING_SDK_LINUX_SAMPLE_ZOOM_H
#define MEETING_SDK_LINUX_SAMPLE_ZOOM_H

#include <iostream>
#include <string>
#include <functional>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#include "jwt-cpp/jwt.h"

#include "zoom_sdk.h"
#include "meeting_service_interface.h"
#include "auth_service_interface.h"
#include "setting_service_interface.h"
#include "meeting_service_components/meeting_audio_interface.h"
#include "meeting_service_components/meeting_participants_ctrl_interface.h"
#include "meeting_service_components/meeting_recording_interface.h"
#include "meeting_service_components/meeting_reminder_ctrl_interface.h"

#include "zoom_sdk_platform.h"

#include "util/Log.h"
#include "Config.h"
#include "events/AuthServiceEvent.h"
#include "events/MeetingServiceEvent.h"
#include "events/MeetingRecordingCtrlEvent.h"
#include "events/MeetingReminderEvent.h"

#include "raw_record/ZoomSDKAudioRawDataDelegate.h"
#include "raw_record/ZoomSDKRendererDelegate.h"
#include "raw_send/ZoomSDKVideoSource.h"

using namespace std;
using namespace ZOOM_SDK_NAMESPACE;
using namespace jwt;

typedef chrono::time_point<chrono::system_clock> time_point;

class Zoom : public Singleton<Zoom>
{

    friend class Singleton<Zoom>;

    Config m_config;

    string m_jwt;

    time_point m_iat;
    time_point m_exp;

    IMeetingService *m_meetingService;
    ISettingService *m_settingService;
    IAuthService *m_authService;

    IZoomSDKRenderer *m_videoHelper;
    ZoomSDKRendererDelegate *m_renderDelegate;

    IZoomSDKAudioRawDataHelper *m_audioHelper;
    ZoomSDKAudioRawDataDelegate *m_audioSource;

    ZoomSDKVideoSource *m_videoSource;

    pid_t m_recordingPid = 0;

    SDKError createServices();
    void generateJWT(const string &key, const string &secret);

    // Helper method to try audio subscription with different settings
    bool tryAudioSubscription(bool mixedAudio);

    /**
     * Starts PulseAudio recording for external audio capture
     * @return true if recording started successfully
     */
    bool startPulseAudioRecording();

    /**
     * Callback fired when the SDK authenticates the credentials
     */
    function<void()> m_onAuth = [&]()
    {
        auto e = isMeetingStart() ? start() : join();
        string action = isMeetingStart() ? "start" : "join";

        if (hasError(e, action + " a meeting"))
            exit(e);
    };

    /**
     * Callback fires when the bot joins the meeting
     */
    function<void()> onJoin;

public:
    Zoom() : m_meetingService(nullptr),
             m_settingService(nullptr),
             m_authService(nullptr),
             m_videoHelper(nullptr),
             m_renderDelegate(nullptr),
             m_audioHelper(nullptr),
             m_audioSource(nullptr),
             m_videoSource(nullptr) 
    {
        // Initialize the onJoin function
        onJoin = [this]() {
            this->handleOnJoin();
        };
    }

    // Implementation of the onJoin callback
    void handleOnJoin();

    SDKError init();
    SDKError auth();
    SDKError config(int ac, char **av);

    SDKError join();
    SDKError start();
    SDKError leave();

    SDKError clean();

    SDKError startRawRecording();
    SDKError stopRawRecording();

    bool isMeetingStart();

    static bool hasError(SDKError e, const string &action = "");

    static Zoom &getInstance()
    {
        static Zoom instance;
        return instance;
    }

    Config &getConfig() { return m_config; }
};

#endif // MEETING_SDK_LINUX_SAMPLE_ZOOM_H