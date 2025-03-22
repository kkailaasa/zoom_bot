#include "Zoom.h"
#include "rawdata/zoom_rawdata_api.h"
#include "rawdata/rawdata_audio_helper_interface.h"
#include "rawdata/rawdata_renderer_interface.h"
#include "rawdata/rawdata_video_source_helper_interface.h"

using namespace ZOOMSDK;

SDKError Zoom::config(int ac, char** av) {
    auto status = m_config.read(ac, av);

    if (status) {
        Log::error("failed to read configuration");
        return SDKERR_INTERNAL_ERROR;
    }

    return SDKERR_SUCCESS;
}

SDKError Zoom::init() {
    InitParam initParam;

    auto host = m_config.zoomHost().c_str();

    initParam.strWebDomain = host;
    initParam.strSupportUrl = host;

    initParam.emLanguageID = LANGUAGE_English;

    initParam.enableLogByDefault = true;
    initParam.enableGenerateDump = true;

    // Add a sleep before initializing the SDK
    sleep(1);

    auto err = InitSDK(initParam);

    if (hasError(err)) {
        Log::error("InitSDK failed");

        return err;
    }

    Log::success("InitSDK successful");

    // Add a sleep after initializing the SDK
    sleep(1);

    return createServices();
}


SDKError Zoom::createServices() {
    auto err = CreateMeetingService(&m_meetingService);
    if (hasError(err)) {
        return err;
    }

    Log::success("Meeting service created");

    err = CreateSettingService(&m_settingService);
    if (hasError(err)) {
        return err;
    }

    Log::success("Setting service created");

    auto meetingServiceEvent = new MeetingServiceEvent();
    meetingServiceEvent->setOnMeetingJoin(onJoin);

    err = m_meetingService->SetEvent(meetingServiceEvent);
    if (hasError(err)) {
        return err;
    }

    Log::success("Meeting service event set");

    return CreateAuthService(&m_authService);
}

SDKError Zoom::auth() {
    SDKError err{SDKERR_UNINITIALIZE};

    auto id = m_config.clientId();
    auto secret = m_config.clientSecret();

    if (id.empty()) {
        Log::error("Client ID cannot be blank");
        return err;
    }

    if (secret.empty()) {
        Log::error("Client Secret cannot be blank");
        return err;
    }

    err = m_authService->SetEvent(new AuthServiceEvent(m_onAuth));
    if (hasError(err)) {
        return err;
    }

    Log::success("Auth service event set");

    generateJWT(m_config.clientId(), m_config.clientSecret());

    AuthContext ctx;
    ctx.jwt_token =  m_jwt.c_str();

    return m_authService->SDKAuth(ctx);
}

void Zoom::generateJWT(const string& key, const string& secret) {

    m_iat = std::chrono::system_clock::now();
    m_exp = m_iat + std::chrono::hours{24};

    m_jwt = jwt::create()
            .set_type("JWT")
            .set_issued_at(m_iat)
            .set_expires_at(m_exp)
            .set_payload_claim("appKey", claim(key))
            .set_payload_claim("tokenExp", claim(m_exp))
            .sign(algorithm::hs256{secret});
}

SDKError Zoom::join() {
    SDKError err{SDKERR_UNINITIALIZE};

    auto mid = m_config.meetingId();
    auto password = m_config.password();
    auto displayName = m_config.displayName();


    if (mid.empty()) {
        Log::error("Meeting ID cannot be blank");
        return err;
    }

    if (password.empty()) {
        Log::error("Meeting Password cannot be blank");
        return err;
    }

    if (displayName.empty()) {
        Log::error("Display Name cannot be blank");
        return err;
    }

    // Always configure to use only PulseAudio for audio recording
    Log::info("Configuring to use only PulseAudio for audio recording");
    m_config.setAudioFileOverride("");

    auto meetingNumber = stoull(mid);
    auto userName = displayName.c_str();
    auto psw = password.c_str();

    JoinParam joinParam;
    joinParam.userType = ZOOM_SDK_NAMESPACE::SDK_UT_WITHOUT_LOGIN;

    JoinParam4WithoutLogin& param = joinParam.param.withoutloginuserJoin;

    param.meetingNumber = meetingNumber;
    param.userName = userName;
    param.psw = psw;
    param.vanityID = nullptr;
    param.customer_key = nullptr;
    param.webinarToken = nullptr;
    param.isVideoOff = false;
    param.isAudioOff = true;  // Join with audio muted first, then unmute later

    if (!m_config.zak().empty()) {
        Log::success("used ZAK token");
        param.userZAK = m_config.zak().c_str();
    }

    if (!m_config.joinToken().empty()) {
        Log::success("used App Privilege token");
        param.app_privilege_token = m_config.joinToken().c_str();
    }

    // Configure audio settings
    auto* audioSettings = m_settingService->GetAudioSettings();
    if (audioSettings) {
        audioSettings->EnableAutoJoinAudio(true);
    }

    return m_meetingService->Join(joinParam);
}

SDKError Zoom::start() {
    SDKError err;

    StartParam startParam;
    startParam.userType = SDK_UT_NORMALUSER;

    StartParam4NormalUser  normalUser;
    normalUser.vanityID = nullptr;
    normalUser.customer_key = nullptr;
    normalUser.isAudioOff = false;
    normalUser.isVideoOff = false;

    err = m_meetingService->Start(startParam);
    if (hasError(err)) {
        return err;
    }

    Log::success("Meeting started");

    return SDKERR_SUCCESS;
}

SDKError Zoom::leave() {
    if (!m_meetingService) {
        return SDKERR_UNINITIALIZE;
    }

    auto status = m_meetingService->GetMeetingStatus();
    if (status == MEETING_STATUS_IDLE) {
        return SDKERR_WRONG_USAGE;
    }

    return  m_meetingService->Leave(LEAVE_MEETING);
}

SDKError Zoom::clean() {
    // If recording process is running, stop it
    if (m_recordingPid > 0) {
        Log::info("Stopping PulseAudio recording process (" + to_string(m_recordingPid) + ")");
        kill(m_recordingPid, SIGTERM);
        m_recordingPid = 0;
    }

    if (m_meetingService) {
        DestroyMeetingService(m_meetingService);
    }

    if (m_settingService) {
        DestroySettingService(m_settingService);
    }

    if (m_authService) {
        DestroyAuthService(m_authService);
    }

    if (m_audioHelper) {
        m_audioHelper->unSubscribe();
    }

    if (m_videoHelper) {
        m_videoHelper->unSubscribe();
    }

    delete m_renderDelegate;
    return CleanUPSDK();
}

bool Zoom::tryAudioSubscription(bool mixedAudio) {
    // Get the audio helper interface with a timeout check
    m_audioHelper = GetAudioRawdataHelper();
    if (!m_audioHelper) {
        Log::error("Failed to get audio rawdata helper");
        return false;
    }

    Log::success("Got audio rawdata helper");

    // Check meeting status before subscribing - if we're not in a meeting, don't block
    auto meetingStatus = m_meetingService->GetMeetingStatus();
    if (meetingStatus != MEETING_STATUS_INMEETING) {
        Log::error("Cannot subscribe to audio: Meeting not in INMEETING state. Current status: " + to_string(meetingStatus));
        return false;
    }

    auto transcribe = m_config.transcribe();
    m_audioSource = new ZoomSDKAudioRawDataDelegate(mixedAudio, transcribe);
    m_audioSource->setDir(m_config.audioDir());
    m_audioSource->setFilename(m_config.audioFile());

    Log::info(string("Attempting audio subscription with mixedAudio=") + (mixedAudio ? "true" : "false"));

    // Try to subscribe with a short timeout
    SDKError err = m_audioHelper->subscribe(m_audioSource);
    if (hasError(err, "subscribe to raw audio")) {
        // Failed, try again with a shorter delay
        Log::info("First audio subscription attempt failed, retrying in 1 second...");
        sleep(1);
        err = m_audioHelper->subscribe(m_audioSource);
        if (hasError(err, "retry subscribe to raw audio")) {
            Log::error("Audio subscription failed after retry");
            return false;
        }
    }

    Log::success("Audio subscription successful");
    return true;
}

bool Zoom::startPulseAudioRecording() {
    // Don't start twice
    if (m_recordingPid > 0) {
        Log::info("PulseAudio recording already started with PID: " + to_string(m_recordingPid));
        return true;
    }

    string outputFile = m_config.audioDir() + "/meeting-audio.mp3";
    
    Log::info("Starting PulseAudio recording to " + outputFile);
    
    // Fork a process to run parecord in the background
    pid_t pid = fork();
    
    if (pid == -1) {
        // Fork failed
        Log::error("Failed to fork process for PulseAudio recording");
        return false;
    } else if (pid == 0) {
        // Child process
        // Redirect stderr to /dev/null to avoid terminal spam
        freopen("/dev/null", "w", stderr);
        
        // Exec parecord command
        execlp("parecord", "parecord", "--channels=2", "--rate=44100", "--format=s16le", 
               "-d", "SpeakerOutput.monitor", outputFile.c_str(), NULL);
        // If exec returns, there was an error
        Log::error("Failed to exec parecord");
        exit(1);
    } else {
        // Parent process
        m_recordingPid = pid;
        Log::info("PulseAudio recording started with PID: " + to_string(m_recordingPid));
        
        // Wait a moment to make sure recording started
        sleep(1);
        
        // Check if recording process is still running
        if (m_recordingPid > 0 && kill(m_recordingPid, 0) == 0) {
            Log::success("PulseAudio recording is active");
            Log::info("startPulseAudioRecording method completed successfully, returning true");
            return true;
        } else {
            Log::error("Failed to start PulseAudio recording - process died immediately");
            m_recordingPid = 0;
            return false;
        }
    }
}

SDKError Zoom::startRawRecording() {
    Log::info("--- startRawRecording method called ---");
    auto recCtl = m_meetingService->GetMeetingRecordingController();
    Log::info("Got recording controller in startRawRecording");

    // Request recording privileges
    Log::info("Checking if recording is allowed...");
    SDKError err = recCtl->CanStartRawRecording();

    if (hasError(err, "Raw recording allowed")) {
        Log::info("Requesting local recording privilege");
        err = recCtl->RequestLocalRecordingPrivilege();

        if (hasError(err)) {
            Log::error("Failed to request recording privilege");
            return err;
        }
    }

    // Try to start SDK recording
    Log::info("Attempting to start raw recording...");
    err = recCtl->StartRawRecording();

    if (hasError(err)) {
        Log::info("SDK recording not available");
        return err;
    } else {
        Log::success("Raw recording started successfully");
    }
    
    // Set up video if configured
    bool videoConfigurationSuccessful = false;
    
    if (m_config.useRawVideo()) {
        Log::info("Setting up video recording");
        if (!m_renderDelegate) {
            m_renderDelegate = new ZoomSDKRendererDelegate();
            m_videoSource = new ZoomSDKVideoSource();
        }

        err = createRenderer(&m_videoHelper, m_renderDelegate);
        if (!hasError(err, "create raw video renderer")) {
            m_renderDelegate->setDir(m_config.videoDir());
            m_renderDelegate->setFilename(m_config.videoFile());
            videoConfigurationSuccessful = true;

            auto participantCtl = m_meetingService->GetMeetingParticipantsController();
            // Try to subscribe to video for active participants
            if (participantCtl && participantCtl->GetParticipantsList()) {
                unsigned int count = participantCtl->GetParticipantsList()->GetCount();
                Log::info("Number of participants: " + to_string(count));

                if (count > 0) {
                    auto uid = participantCtl->GetParticipantsList()->GetItem(0);
                    m_videoHelper->setRawDataResolution(ZoomSDKResolution_720P);
                    m_videoHelper->subscribe(uid, RAW_DATA_TYPE_VIDEO);
                }
            }
        }
    }
    
    Log::info("--- startRawRecording method completed ---");
    return SDKERR_SUCCESS;
}

SDKError Zoom::stopRawRecording() {
    // Set recording flag to false to stop writing audio files
    if (m_audioSource) {
        m_audioSource->setRecordingStarted(false);
    }
    
    // Stop PulseAudio recording if running
    if (m_recordingPid > 0) {
        Log::info("Stopping PulseAudio recording process (" + to_string(m_recordingPid) + ")");
        kill(m_recordingPid, SIGTERM);
        m_recordingPid = 0;
    }
    
    auto recCtrl = m_meetingService->GetMeetingRecordingController();
    auto err = recCtrl->StopRawRecording();
    hasError(err, "stop raw recording");

    return err;
}

bool Zoom::isMeetingStart() {
    return m_config.isMeetingStart();
}

bool Zoom::hasError(const SDKError e, const string& action) {
    auto isError = e != SDKERR_SUCCESS;

    if (isError) {
        stringstream ss;
        ss << "failed to " << action << " with status " << e;

        if (e == 32) {
            ss << " (This typically means the audio system isn't ready or the meeting state doesn't allow audio access yet)";
        }

        Log::error(ss.str());
    }

    return isError;
}

/**
 * Callback fires when the bot joins the meeting
 */
void Zoom::handleOnJoin() {
    Log::info("--- Starting onJoin callback ---");
    auto *reminderController = m_meetingService->GetMeetingReminderController();
    reminderController->SetEvent(new MeetingReminderEvent());

    // Make sure audio is unmuted and connected after a delay
    auto *audioController = m_meetingService->GetMeetingAudioController();
    if (audioController)
    {
        // Ensure audio is connected first
        Log::info("Joining VoIP audio...");
        audioController->JoinVoip();
        sleep(3); // Wait for audio to connect
        
        // Then unmute
        Log::info("Unmuting audio...");
        audioController->UnMuteAudio(0);
        Log::info("Unmuted audio after joining");
    }

    // Setup recording controller events
    Log::info("Setting up recording controller...");
    auto recordingCtrl = m_meetingService->GetMeetingRecordingController();
    Log::info("Got recording controller");
    
    // Set up the callback for when recording privilege changes
    Log::info("Setting up recording privilege callback...");
    function<void(bool)> onRecordingPrivilegeChanged = [this](bool canRec)
    {
        Log::info("Recording privilege changed: " + string(canRec ? "granted" : "denied"));
        if (canRec) {
            Log::info("Recording privilege granted, starting recording...");
            startRawRecording();
            
            // After raw recording is started, also start PulseAudio recording
            Log::info("Starting PulseAudio recording after Zoom recording enabled...");
            bool pulseAudioStartSuccess = startPulseAudioRecording();
            Log::info("PulseAudio recording start result: " + string(pulseAudioStartSuccess ? "success" : "failure"));
        } else {
            Log::info("Recording privilege denied, stopping recording...");
            stopRawRecording();
        }
    };

    Log::info("Creating recording event handler...");
    auto recordingEvent = new MeetingRecordingCtrlEvent(onRecordingPrivilegeChanged);
    
    Log::info("Setting recording event handler...");
    recordingCtrl->SetEvent(recordingEvent);
    Log::info("Recording event handler set");
    
    // Request recording privilege - will trigger the callback when granted
    Log::info("Requesting recording privilege...");
    SDKError err = recordingCtrl->RequestLocalRecordingPrivilege();
    if (hasError(err, "request recording privilege")) {
        // If we can't request recording privilege, start PulseAudio directly
        Log::info("Failed to request recording privilege, starting PulseAudio directly...");
        bool pulseAudioStartSuccess = startPulseAudioRecording();
        Log::info("PulseAudio recording start result: " + string(pulseAudioStartSuccess ? "success" : "failure"));
    }
    
    Log::info("--- onJoin callback completed ---");
}