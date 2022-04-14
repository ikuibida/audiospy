# audiospy

Simple cross-platform client-server program which records audio data from microphone on one PC and plays it in real-time on another PC.
It consumes very small amount of system resources: it can run 24/7 without any CPU usage unless the client is connected.
When the client is connected the server's CPU usage is very low.

Contents:
* [Features](#features)
* [Build & Install](#build-install)
* [Run](#run)


## Features

* ALSA (Linux)
* CoreAudio (macOS)
* DirectSound (Windows)
* OSS (FreeBSD)
* PulseAudio (Linux)
* WASAPI (Windows)
* Currently only uncompressed PCM data is supported


## Build & Install

Requirements:

* GNU make
* gcc or clang
* libalsa-devel (for ALSA)
* libpulse-devel (for Pulse Audio)

Download all needed repositories:

	git clone https://github.com/stsaz/ffbase
	git clone https://github.com/stsaz/ffaudio
	git clone https://github.com/stsaz/ffos
	git clone https://github.com/stsaz/audiospy
	cd audiospy

Build on Linux with PulseAudio:

	make FFAUDIO_API=pulse

Build on Linux with ALSA:

	make FFAUDIO_API=alsa

Build on Linux for Windows with WASAPI:

	make OS=windows FFAUDIO_API=wasapi

Build on FreeBSD with OSS:

	gmake FFAUDIO_API=oss


## Run

1. Start the server which listens on TCP port 64000

		./audiospy_sv 64000

	This process records from the default audio device and transfers the data to any connected client.

2. Start the client which connects to the server with IP address 127.0.0.1 and TCP port 64000

		./audiospy_cl 127.0.0.1 64000

	After a successful connection this process receives the audio data from the server and plays it via the default audio device.


## License

This software can be used only for good and not for evil.
