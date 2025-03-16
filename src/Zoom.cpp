#include "Zoom.h"

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

    // Add a sleep after initializing the SDK
    sleep(1);

    return createServices();
}


SDKError Zoom::createServices() {
    auto err = CreateMeetingService(&m_meetingService);
    if (hasError(err)) return err;

    err = CreateSettingService(&m_settingService);
    if (hasError(err)) return err;

    auto meetingServiceEvent = new MeetingServiceEvent();
    meetingServiceEvent->setOnMeetingJoin(onJoin);

    err = m_meetingService->SetEvent(meetingServiceEvent);
    if (hasError(err)) return err;

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
    if (hasError(err)) return err;

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

    if (m_config.useRawAudio()) {
        auto* audioSettings = m_settingService->GetAudioSettings();
        if (!audioSettings) return SDKERR_INTERNAL_ERROR;

        audioSettings->EnableAutoJoinAudio(true);

        // Audio device selection is different in this SDK version
        // so we'll skip that part to avoid API incompatibilities
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
    hasError(err, "start meeting");

    return err;
}

SDKError Zoom::leave() {
    if (!m_meetingService)
        return SDKERR_UNINITIALIZE;

    auto status = m_meetingService->GetMeetingStatus();
    if (status == MEETING_STATUS_IDLE)
        return SDKERR_WRONG_USAGE;

    return  m_meetingService->Leave(LEAVE_MEETING);
}

SDKError Zoom::clean() {
    if (m_meetingService)
        DestroyMeetingService(m_meetingService);

    if (m_settingService)
        DestroySettingService(m_settingService);

    if (m_authService)
        DestroyAuthService(m_authService);

    if (m_audioHelper)
        m_audioHelper->unSubscribe();

    if (m_videoHelper)
        m_videoHelper->unSubscribe();

    delete m_renderDelegate;
    return CleanUPSDK();
}

bool Zoom::tryAudioSubscription(bool mixedAudio) {
    m_audioHelper = GetAudioRawdataHelper();
    if (!m_audioHelper) {
        Log::error("Failed to get audio rawdata helper");
        return false;
    }

    auto transcribe = m_config.transcribe();
    if (m_audioSource) {
        delete m_audioSource;
        m_audioSource = nullptr;
    }

    m_audioSource = new ZoomSDKAudioRawDataDelegate(mixedAudio, transcribe);
    m_audioSource->setDir(m_config.audioDir());
    m_audioSource->setFilename(m_config.audioFile());

    Log::info(string("Attempting audio subscription with mixedAudio=") + (mixedAudio ? "true" : "false"));

    SDKError err = m_audioHelper->subscribe(m_audioSource);
    if (hasError(err, "subscribe to raw audio")) {
        // Failed, let's wait and try again
        sleep(2);
        err = m_audioHelper->subscribe(m_audioSource);
        return !hasError(err, "retry subscribe to raw audio");
    }

    Log::success("Audio subscription successful");
    return true;
}

SDKError Zoom::startRawRecording() {
    auto recCtl = m_meetingService->GetMeetingRecordingController();

    // The IMeetingRawArchivingController doesn't seem to be fully implemented in this SDK version
    // So we'll skip this part for compatibility

    SDKError err = recCtl->CanStartRawRecording();

    if (hasError(err)) {
        Log::info("requesting local recording privilege");
        return recCtl->RequestLocalRecordingPrivilege();
    }

    err = recCtl->StartRawRecording();
    if (hasError(err, "start raw recording"))
        return err;

    if (m_config.useRawVideo()) {
        if (!m_renderDelegate) {
            m_renderDelegate = new ZoomSDKRendererDelegate();
            m_videoSource = new ZoomSDKVideoSource();
        }

        err = createRenderer(&m_videoHelper, m_renderDelegate);
        if (hasError(err, "create raw video renderer"))
            return err;

        m_renderDelegate->setDir(m_config.videoDir());
        m_renderDelegate->setFilename(m_config.videoFile());

        auto participantCtl = m_meetingService->GetMeetingParticipantsController();

        // Try to get participant list and find active participants
        if (participantCtl && participantCtl->GetParticipantsList()) {
            unsigned int count = participantCtl->GetParticipantsList()->GetCount();
            Log::info("Number of participants: " + to_string(count));

            if (count > 0) {
                auto uid = participantCtl->GetParticipantsList()->GetItem(0);
                m_videoHelper->setRawDataResolution(ZoomSDKResolution_720P);
                err = m_videoHelper->subscribe(uid, RAW_DATA_TYPE_VIDEO);
                if (hasError(err, "subscribe to raw video")) {
                    // Try with other participants if available
                    for (unsigned int i = 1; i < count && i < 5; i++) {
                        uid = participantCtl->GetParticipantsList()->GetItem(i);
                        if (uid) {
                            Log::info("Trying to subscribe to video for participant " + to_string(i));
                            err = m_videoHelper->subscribe(uid, RAW_DATA_TYPE_VIDEO);
                            if (!hasError(err, "subscribe to raw video for alternative participant"))
                                break;
                        }
                    }
                }
            } else {
                Log::error("No participants found for video subscription");
            }
        } else {
            Log::error("Failed to get participants list");
        }
    }

    if (m_config.useRawAudio()) {
        // Wait before attempting to subscribe to audio
        sleep(3);

        // Try with both mixedAudio=true and mixedAudio=false
        bool originalMixedAudio = !m_config.separateParticipantAudio();

        // First try with original setting
        if (!tryAudioSubscription(originalMixedAudio)) {
            // If that fails, try with the opposite setting
            Log::info("Trying with flipped mixed audio setting");
            if (!tryAudioSubscription(!originalMixedAudio)) {
                Log::error("Both mixed audio settings failed");
            }
        }
    }

    return SDKERR_SUCCESS;
}

SDKError Zoom::stopRawRecording() {
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

    if(!action.empty()) {
        if (isError) {
            stringstream ss;
            ss << "failed to " << action << " with status " << e;
            Log::error(ss.str());
        } else {
            Log::success(action);
        }
    }
    return isError;
}