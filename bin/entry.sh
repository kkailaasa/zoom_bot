#!/usr/bin/env bash

# directory for CMake output
BUILD=build

# directory for application output
mkdir -p out

# Track processes to terminate gracefully
RECORDING_PID=""

# Trap SIGTERM and SIGINT to terminate child processes gracefully
trap cleanup SIGTERM SIGINT

cleanup() {
  echo "Cleaning up and saving recordings..."

  # Stop PulseAudio recording if running
  if [ -n "$RECORDING_PID" ] && kill -0 $RECORDING_PID 2>/dev/null; then
    echo "Stopping audio recording process ($RECORDING_PID)"
    kill -TERM $RECORDING_PID
    wait $RECORDING_PID 2>/dev/null
  fi

  # Make sure we leave the meeting gracefully
  if [ -f "./$BUILD/zoomsdk" ]; then
    echo "Leaving Zoom meeting..."
    kill -TERM $(pidof zoomsdk) 2>/dev/null
  fi

  echo "Cleanup complete."
  exit 0
}

start-audio-recording() {
  local output_file="out/meeting-audio.wav"

  echo "Starting PulseAudio recording to $output_file"

  # Record stereo, 16-bit, 44.1kHz audio from the virtual speaker monitor
  parecord --channels=2 --rate=44100 --format=s16le -d SpeakerOutput.monitor "$output_file" &
  RECORDING_PID=$!

  echo "Recording started with PID: $RECORDING_PID"

  # Also start a backup recording in raw PCM format in case WAV has issues
  pacat -r -d SpeakerOutput.monitor > out/meeting-audio.raw &

  # Wait a moment to make sure recording started
  sleep 1

  # Check if recording process is still running
  if kill -0 $RECORDING_PID 2>/dev/null; then
    echo "✅ Recording is active"
  else
    echo "❌ Failed to start recording"
  fi
}

setup-pulseaudio() {
  # Enable dbus
  if [[  ! -d /var/run/dbus ]]; then
    mkdir -p /var/run/dbus
    dbus-uuidgen > /var/lib/dbus/machine-id
    dbus-daemon --config-file=/usr/share/dbus-1/system.conf --print-address
  fi

  usermod -G pulse-access,audio root || echo "Warning: Could not add root to audio groups"

  # Cleanup to be "stateless" on startup, otherwise pulseaudio daemon can't start
  rm -rf /var/run/pulse /var/lib/pulse /root/.config/pulse/
  mkdir -p ~/.config/pulse/ && cp -r /etc/pulse/* "$_"

  pulseaudio -D --exit-idle-time=-1 --system --disallow-exit

  # Wait for PulseAudio to fully start
  sleep 2

  # Verify PulseAudio is running
  if pulseaudio --check; then
    echo "PulseAudio is running"
  else
    echo "Warning: PulseAudio failed to start correctly"
    pulseaudio -D --exit-idle-time=-1 --system --disallow-exit --verbose
  fi

  # Create a virtual speaker output
  pactl load-module module-null-sink sink_name=SpeakerOutput sink_properties=device.description="Virtual_Speaker" || echo "Warning: Failed to create null sink"
  pactl set-default-sink SpeakerOutput || echo "Warning: Failed to set default sink"
  pactl set-default-source SpeakerOutput.monitor || echo "Warning: Failed to set default source"

  # List available sinks and sources for debugging
  echo "Available PulseAudio sinks:"
  pactl list short sinks

  echo "Available PulseAudio sources:"
  pactl list short sources

  # Create zoomus.conf in all possible locations
  mkdir -p ~/.config.us/
  echo -e "[General]\nsystem.audio.type=default" > ~/.config.us/zoomus.conf

  mkdir -p ~/.config/
  echo -e "[General]\nsystem.audio.type=default" > ~/.config/zoomus.conf

  echo -e "[General]\nsystem.audio.type=default" > ~/.config/pulse/zoomus.conf

  # Verify zoomus.conf files exist
  echo "Created zoomus.conf files at:"
  ls -la ~/.config.us/zoomus.conf || echo "~/.config.us/zoomus.conf not found"
  ls -la ~/.config/zoomus.conf || echo "~/.config/zoomus.conf not found"
  ls -la ~/.config/pulse/zoomus.conf || echo "~/.config/pulse/zoomus.conf not found"
}

build() {
  # Check if the Zoom SDK is present
  if [ ! -f "lib/zoomsdk/libmeetingsdk.so" ]; then
    echo "❌ Error: libmeetingsdk.so is missing in lib/zoomsdk/"
    exit 1
  fi

  # Check both files exist (don't try to copy if both already exist)
  if [ -f "lib/zoomsdk/libmeetingsdk.so" ] && [ ! -f "lib/zoomsdk/libmeetingsdk.so.1" ]; then
    echo "Creating libmeetingsdk.so.1 from libmeetingsdk.so"
    cp "lib/zoomsdk/libmeetingsdk.so" "lib/zoomsdk/libmeetingsdk.so.1"
  fi

  # Configure CMake if this is the first run
  if [ ! -d "$BUILD" ]; then
    echo "Configuring CMake for first build..."
    cmake -B "$BUILD" -S . --preset debug || {
      echo "❌ CMake configuration failed"
      exit 1
    }
  fi

  # Set up and start pulseaudio
  echo "Setting up PulseAudio..."
  setup-pulseaudio

  # Start audio recording via PulseAudio
  echo "Starting audio recording..."
  start-audio-recording

  # Build the Source Code
  echo "Building source code..."
  cmake --build "$BUILD" || {
    echo "❌ Build failed"
    exit 1
  }
}

run() {
    if [ -f "./$BUILD/zoomsdk" ]; then
        echo "✅ Build successful, starting application..."
        exec "./$BUILD/zoomsdk"
    else
        echo "❌ Error: Build executable not found at ./$BUILD/zoomsdk"
        exit 1
    fi
}

build && run;

exit $?