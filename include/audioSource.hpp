/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <main.hpp>

namespace TemStream
{
using AudioBuffer = FixedSizeList<uint8_t, MB(1)>;
class ClientConnection;

/**
 * The audio source for playing audio or recording audio
 */
class AudioSource
{
  public:
	enum class Type
	{
		Playback,	 ///< Sending audio to a playback device
		Record,		 ///< Recording from device (i.e. microphone)
		RecordWindow ///< Recording audio from a process
	};

  private:
	union {
		std::array<float, MB(1) / sizeof(float)> fbuffer;
		std::array<char, MB(1)> buffer;
	};
	const Message::Source source;
	String name;

	// For recording
	ByteList currentAudio;
	AudioBuffer outgoing;

	AudioBuffer storedAudio;
	SDL_AudioSpec spec;
	union {
		OpusDecoder *decoder;
		OpusEncoder *encoder;
	};
	SDL_AudioDeviceID id;
	union {
		float volume;
		float silenceThreshold;
	};
	const Type type;

	void recordAudio(uint8_t *, int);
	void playbackAudio(uint8_t *, int);

	static SDL_AudioSpec getAudioSpec();

	static void recordCallback(AudioSource *, uint8_t *, int);
	static void playbackCallback(AudioSource *, uint8_t *, int);

	/**
	 * Opus can only encode certain intervals (See ::closestValidFrameCount) of audio at once. So when a large chunk of
	 * audio is ready to be encoded, it must be encoded in chunks. This function returns the best number of audio frames
	 * that can be encoded by Opus
	 *
	 * @param frequency bytes per second (i.e. 48000 Hz)
	 * @param frameSize Opus frame size
	 *
	 * @return The number of frames that can be encoded
	 */
	constexpr static int audioLengthToFrames(const int frequency, const int frameSize);

	/**
	 * Opus can only encode certain intervals at once:
	 *
	 * 120 ms, 100 ms, 80 ms, 60 ms, 40 ms, 20 ms, 10 ms, 5 ms, 2.5 ms.
	 *
	 * This function will given the closest number without going over
	 *
	 * @param frequency bytes per second (i.e. 48000 Hz)
	 * @param frames Number of audio frames
	 *
	 * @return The enum representing the interval count
	 */
	constexpr static int closestValidFrameCount(const int frequency, const int frames);

	friend class Allocator<AudioSource>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

  protected:
	AudioSource(const Message::Source &, Type, float);

	void close();

	static unique_ptr<AudioSource> startRecording(AudioSource *, int);

	/**
	 * Object will lock the audio device. Will unlock on deletion. Used to protect memory that
	 * is accessed during the audio callbacks.
	 */
	class Lock
	{
	  private:
		const SDL_AudioDeviceID id;

	  public:
		Lock(SDL_AudioDeviceID);
		~Lock();
	};

  public:
	AudioSource() = delete;
	AudioSource(const AudioSource &) = delete;
	AudioSource(AudioSource &&) = delete;
	virtual ~AudioSource();

	/**
	 * Add Opus packet to playback. Audio will be decoded and sent to the playback device
	 *
	 * @param bytes
	 */
	void enqueueAudio(const ByteList &bytes);

	void encodeAndSendAudio(ClientConnection &);

	Type getType() const
	{
		return type;
	}

	bool isRecording() const;

	/**
	 * Check if the audio source is still playing or recording audio
	 *
	 * @return True if is playing or recording audio
	 */
	bool isActive() const;

	float getVolume() const
	{
		return volume;
	}

	void setVolume(float volume)
	{
		this->volume = volume;
	}

	float getSilenceThreshold() const
	{
		return silenceThreshold;
	}

	void setSilenceThreshold(float silenceThreshold)
	{
		this->silenceThreshold = silenceThreshold;
	}

	bool isMuted() const
	{
		const auto status = SDL_GetAudioDeviceStatus(id);
		return status == SDL_AUDIO_PAUSED;
	}

	void setMuted(bool b)
	{
		SDL_PauseAudioDevice(id, b);
	}

	/**
	 * Remove all audio that has been recorded or about to be sent to the audio device.
	 */
	void clearAudio();

	const Message::Source &getSource() const
	{
		return source;
	}

	const String &getName() const
	{
		return name;
	}

	/**
	 * Get the last chunk of audio that was recorded or sent to the audio device
	 *
	 * @param bytes [out] Will contain the current audio bytes
	 */
	void getCurrentAudio(ByteList &bytes) const;

	/**
	 * Calculate the volume of the audio samples.
	 *
	 * @param samples
	 * @param size
	 *
	 * @return True if the average of all the samples is greater than the silenceThreshold
	 */
	bool isLoudEnough(const float *samples, const int size) const;

	static std::optional<WindowProcesses> getWindowsWithAudio();
	static unique_ptr<AudioSource> startRecordingWindow(const Message::Source &, const WindowProcess &, float);

	static unique_ptr<AudioSource> startRecording(const Message::Source &, const char *, float);
	static unique_ptr<AudioSource> startPlayback(const Message::Source &, const char *, float);
};
} // namespace TemStream
