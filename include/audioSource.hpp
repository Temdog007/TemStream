#pragma once

#include <main.hpp>

namespace TemStream
{
class ClientConnection;
class AudioSource
{
  public:
	enum Type
	{
		Playback,
		Record,
		RecordWindow
	};

  private:
	union {
		std::array<float, MB(1) / sizeof(float)> fbuffer;
		std::array<char, MB(1)> buffer;
	};
	const Message::Source source;
	String name;
	ByteList recordBuffer;
	ByteList storedAudio;
	ByteList currentAudio;
	SDL_AudioSpec spec;
	union {
		OpusDecoder *decoder;
		OpusEncoder *encoder;
	};
	SDL_AudioDeviceID id;
	SDL_KeyCode code;
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

	constexpr static int audioLengthToFrames(const int frequency, const int duration);

	constexpr static int closestValidFrameCount(const int frequency, const int frames);

	friend class Allocator<AudioSource>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

  protected:
	AudioSource(const Message::Source &, Type, float);

	void close();

	static unique_ptr<AudioSource> startRecording(AudioSource *, int);

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

	void enqueueAudio(const ByteList &);

	void encodeAndSendAudio(ClientConnection &);

	Type getType() const
	{
		return type;
	}

	bool isRecording() const;

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

	void clearAudio();

	const Message::Source &getSource() const
	{
		return source;
	}

	const String &getName() const
	{
		return name;
	}

	ByteList getCurrentAudio() const;

	bool isLoudEnough(float *, int) const;

	static std::optional<WindowProcesses> getWindowsWithAudio();
	static unique_ptr<AudioSource> startRecordingWindow(const Message::Source &, const WindowProcess &, float);

	static unique_ptr<AudioSource> startRecording(const Message::Source &, const char *, float);
	static unique_ptr<AudioSource> startPlayback(const Message::Source &, const char *, float);
};
} // namespace TemStream