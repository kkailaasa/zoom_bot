#include "MeetingRecordingCtrlEvent.h"

void MeetingRecordingCtrlEvent::onRecordPrivilegeChanged(bool bCanRec) {
    Log::info("onRecordPrivilegeChanged implementation called with canRec=" + string(bCanRec ? "true" : "false"));
    if (m_onRecordingPrivilegeChanged) {
        m_onRecordingPrivilegeChanged(bCanRec);
    } else {
        Log::error("Recording privilege callback is null");
    }
}
