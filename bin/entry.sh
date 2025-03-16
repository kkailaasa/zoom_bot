#!/usr/bin/env bash

# directory for CMake output
BUILD=build

# directory for application output
mkdir -p out

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

  # Create a virtual speaker output
  # Allow these commands to fail gracefully
  pactl load-module module-null-sink sink_name=SpeakerOutput || echo "Warning: Failed to create null sink"
  pactl set-default-sink SpeakerOutput || echo "Warning: Failed to set default sink"
  pactl set-default-source SpeakerOutput.monitor || echo "Warning: Failed to set default source"

  # Make config file
  echo -e "[General]\nsystem.audio.type=default" > ~/.config/zoomus.conf
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

  # Set up and start pulseaudio (allow failure)
  echo "Setting up PulseAudio..."
  setup-pulseaudio &> /dev/null || echo "⚠️ Warning: PulseAudio setup failed, continuing anyway"

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