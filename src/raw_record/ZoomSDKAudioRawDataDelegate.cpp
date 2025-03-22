#include "ZoomSDKAudioRawDataDelegate.h"


ZoomSDKAudioRawDataDelegate::ZoomSDKAudioRawDataDelegate(bool useMixedAudio = true, bool transcribe = false) : m_useMixedAudio(useMixedAudio), m_transcribe(transcribe){
    server.start();
}

void ZoomSDKAudioRawDataDelegate::onMixedAudioRawDataReceived(AudioRawData *data) {
    if (!m_useMixedAudio) return;

    // write to socket (always do this for transcription regardless of recording state)
    if (m_transcribe) {
        server.writeBuf(data->GetBuffer(), data->GetBufferLen());
        return;
    }

    // Check if recording has started before writing to file
    if (!m_recordingStarted) {
        return;
    }

    // or write to file
    if (m_dir.empty())
        return Log::error("Output Directory cannot be blank");

    if (m_filename.empty())
        m_filename = "test.pcm";

    // Indicate that recording has started
    setRecordingStarted(true);

    stringstream path;
    path << m_dir << "/" << m_filename;

    writeToFile(path.str(), data);
}



void ZoomSDKAudioRawDataDelegate::onOneWayAudioRawDataReceived(AudioRawData* data, uint32_t node_id) {
    if (m_useMixedAudio) return;
    
    // Check if recording has started before writing to file
    if (!m_recordingStarted) {
        return;
    }

    stringstream path;
    path << m_dir << "/node-" << node_id << ".pcm";
    writeToFile(path.str(), data);
}

void ZoomSDKAudioRawDataDelegate::onShareAudioRawDataReceived(AudioRawData* data) {
    // Logging removed to reduce console spam
    // The shared audio data is received but not processed further in this implementation
}


void ZoomSDKAudioRawDataDelegate::writeToFile(const string &path, AudioRawData *data)
{
    static std::ofstream file;
	file.open(path, std::ios::out | std::ios::binary | std::ios::app);

	if (!file.is_open())
        return Log::error("failed to open audio file path: " + path);
	
    file.write(data->GetBuffer(), data->GetBufferLen());

    file.close();
	file.flush();
}

void ZoomSDKAudioRawDataDelegate::setDir(const string &dir)
{
    m_dir = dir;
}

void ZoomSDKAudioRawDataDelegate::setFilename(const string &filename)
{
    m_filename = filename;
}

void ZoomSDKAudioRawDataDelegate::setRecordingStarted(bool started)
{
    if (started && !m_recordingStarted) {
        Log::success("Recording started, audio files will now be written");
    } else if (!started && m_recordingStarted) {
        Log::info("Recording stopped, audio files will no longer be written");
    }
    m_recordingStarted = started;
}
