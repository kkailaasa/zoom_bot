#ifndef MEETING_SDK_LINUX_SAMPLE_MEETINGRECORDCTRLEVENT_H
#define MEETING_SDK_LINUX_SAMPLE_MEETINGRECORDCTRLEVENT_H

#include <iostream>
#include <functional>
#include "meeting_service_components/meeting_recording_interface.h"
#include "../util/Log.h"

using namespace std;
using namespace ZOOMSDK;

/**
 * Custom MeetingRecordingCtrl Event handler
 */
class MeetingRecordingCtrlEvent : public IMeetingRecordingCtrlEvent {
    function<void(bool)> m_onRecordingPrivilegeChanged;

public:
    MeetingRecordingCtrlEvent(function<void(bool)> onPrivilegeChanged) : m_onRecordingPrivilegeChanged(onPrivilegeChanged) {
        Log::info("MeetingRecordingCtrlEvent constructor called");
    }

    ~MeetingRecordingCtrlEvent() override {
        Log::info("MeetingRecordingCtrlEvent destructor called");
    }

    /**
     * Fires when the status of local recording changes
     * @param status local recording status
     */
    void onRecordingStatus(RecordingStatus status) override {
        Log::info("onRecordingStatus: " + to_string(status));
    }

    /**
     * Fires when the status of cloud recording changes
     * @param status cloud recording status
     */
    void onCloudRecordingStatus(RecordingStatus status) override {
        Log::info("onCloudRecordingStatus: " + to_string(status));
    }

    /**
     * Fires when recording privilege changes
     * @param bCanRec true if the user can record
     */
    void onRecordPrivilegeChanged(bool bCanRec) override;

    /**
     * fires when the local recording privilege changes
     * @param status status of the local recording privliege request
     */
    void onLocalRecordingPrivilegeRequestStatus(RequestLocalRecordingStatus status) override {
        Log::info("onLocalRecordingPrivilegeRequestStatus: " + to_string(status));
    }

    /**
     * Fires when a user requests local recording privilege
     * @param handler data when local recording privilege is requested
     */
    void onLocalRecordingPrivilegeRequested(IRequestLocalRecordingPrivilegeHandler* handler) override {
        Log::info("onLocalRecordingPrivilegeRequested called");
        if (handler) {
            Log::info("Granting local recording privilege request");
            handler->GrantLocalRecordingPrivilege();
        }
    }

    // Implement missing pure virtual methods that are required by the interface
    void onRequestCloudRecordingResponse(RequestStartCloudRecordingStatus status) override {
        Log::info("onRequestCloudRecordingResponse: " + to_string(status));
    }

    void onStartCloudRecordingRequested(IRequestStartCloudRecordingHandler* handler) override {
        Log::info("onStartCloudRecordingRequested called");
        if (handler) {
            handler->Deny(false);
        }
    }

    void onCloudRecordingStorageFull(time_t gracePeriodDate) override {
        Log::info("onCloudRecordingStorageFull: " + to_string(gracePeriodDate));
    }

    void onEnableAndStartSmartRecordingRequested(IRequestEnableAndStartSmartRecordingHandler* handler) override {
        Log::info("onEnableAndStartSmartRecordingRequested called");
        if (handler) {
            handler->Decline(false);
        }
    }

    void onSmartRecordingEnableActionCallback(ISmartRecordingEnableActionHandler* handler) override {
        Log::info("onSmartRecordingEnableActionCallback called");
    }

    void onTranscodingStatusChanged(TranscodingStatus status, const zchar_t* path) override {
        Log::info("onTranscodingStatusChanged: " + to_string(static_cast<int>(status)));
    }
};

#endif //MEETING_SDK_LINUX_SAMPLE_MEETINGRECORDCTRLEVENT_H
