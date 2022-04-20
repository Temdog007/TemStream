#pragma once

#include <main.hpp>

namespace TemStream
{
class ClientPeer;
struct WindowProcess
{
	String name;
	int32_t id;
};
class Audio
{
  private:
	union {
		std::array<float, MB(1) / sizeof(float)> fbuffer;
		std::array<char, MB(1)> buffer;
	};
	List<char> recordBuffer;
	const MessageSource source;
	String name;
	Deque<char> storedAudio;
	Bytes currentAudio;
	SDL_AudioSpec spec;
	union {
		OpusDecoder *decoder;
		OpusEncoder *encoder;
	};
	SDL_AudioDeviceID id;
	SDL_KeyCode code;
	float volume;
	const bool recording;

	void recordAudio(const uint8_t *, int);
	void playbackAudio(uint8_t *, int);

	static SDL_AudioSpec getAudioSpec();

	static void recordCallback(Audio *, const uint8_t *, int);
	static void playbackCallback(Audio *, uint8_t *, int);

	static int audioLengthToFrames(const int frequency, const int duration);

	static int closestValidFrameCount(const int frequency, const int frames);

  protected:
	Audio(const MessageSource &, bool);

	static std::shared_ptr<Audio> startRecording(Audio *, int);

  public:
	Audio() = delete;
	Audio(const Audio &) = delete;
	Audio(Audio &&) = delete;
	virtual ~Audio();

	void enqueueAudio(const Bytes &);

	bool encodeAndSendAudio(ClientPeer &);

	bool isRecording() const
	{
		return recording;
	}

	float getVolume() const
	{
		return volume;
	}

	void setVolume(float volume)
	{
		this->volume = volume;
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

	void clearAudio();

	const MessageSource &getSource() const
	{
		return source;
	}

	const String &getName() const
	{
		return name;
	}

	void useCurrentAudio(const std::function<void(const Bytes &)> &) const;

	static std::optional<List<WindowProcess>> getListOfWindowsWithAudio();
	static std::shared_ptr<Audio> startRecordingWindow(const MessageSource &, const WindowProcess &);

	static std::shared_ptr<Audio> startRecording(const MessageSource &, const char *);
	static std::shared_ptr<Audio> startPlayback(const MessageSource &, const char *);

	class Lock
	{
	  private:
		const SDL_AudioDeviceID id;

	  public:
		Lock(SDL_AudioDeviceID);
		~Lock();
	};
};
} // namespace TemStream