# Zoom Meeting SDK Headless Linux Sample (Modified)

This project has been cloned from https://github.com/zoom/meetingsdk-headless-linux-sample

Another implementation of this can be found in https://github.com/zoom/meetingsdk-linux-raw-recording-sample

[The Original Project](https://github.com/zoom/meetingsdk-headless-linux-sample) has not been maintained for sometime, so it does not work by itself - this one is a modified version of it to get it running.

## Setup Instructions

### Prerequisites
- Docker installed
- Zoom Meeting SDK credentials

### Zoom Marketplace Setup
1. Go to [marketplace.zoom.us](https://marketplace.zoom.us)
2. Sign in with your Zoom account
3. Go to **Develop** in the top right hand corner
4. Click on **Build App**
5. Choose **General App**
6. Copy and keep the client ID and client secret
7. Under OAuth Redirect URL, enter "http://localhost:4000"
8. Under OAuth Allow Lists, enter "http://localhost:4000"
9. Click on **Continue** until you reach the "Surface" section

### App Configuration
1. Choose "Meetings"
2. Enable "Zoom App SDK" and "Guest Mode"
3. Under "Zoom App SDK", add the following APIs:
   - openUrl
   - getRunningContext
   - getMeetingContext
   - getMeetingParticipants
   - getUserContext
   - getRecordingContext
   - getMeetingJoinUrl
   - getMeetingUUID
   - allowParticipantToRecord
   - getUserMediaVideo
   - getUserMediaAudio
   - setUserMediaVideo
   - setUserMediaAudio
   - setIncomingParticipantAudioState
   - getIncomingParticipantAudioState
   - setAudioState
   - setAudioSettings
   - getAudioSettings
   - getAudioState
   - leaveMeeting
   - joinMeeting
4. Under Embed, enable "Meeting SDK"
5. Enable "Use Device Auth"

### SDK Installation
1. Download the Zoom Meeting SDK tar file for Linux
2. Uncompress the tar file
3. Move the contents to the `lib/zoomsdk` folder of the repository

### Permissions & Scopes
1. Under scopes, enable "View user's zak token"
2. Move on to next and click on "Add App Now"
3. Enable option permissions and continue the authorization flow
   - Note: It will redirect to localhost:4000 which you can close

### Final Configuration
1. Configure the `config.toml` file with:
   - Zoom Meeting SDK Client ID
   - Zoom Meeting SDK Client Secret
   - Join URL for the meeting
   - Deepgram API Key

> **Note:** As this is in development, the bot can only join Zoom meetings hosted by the same Zoom account that created the Zoom Meeting SDK Client ID and Client Secret.

### Running the Application
```
chmod +x bin/entry.sh
```
```
docker compose up
```