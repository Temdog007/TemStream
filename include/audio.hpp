#pragma once

#include <main.hpp>

namespace TemStream
{
class ClientConnection;
class Audio
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

	static void recordCallback(Audio *, uint8_t *, int);
	static void playbackCallback(Audio *, uint8_t *, int);

	constexpr static int audioLengthToFrames(const int frequency, const int duration);

	constexpr static int closestValidFrameCount(const int frequency, const int frames);

	friend class Allocator<Audio>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

  protected:
	Audio(const Message::Source &, Type, float);

	void close();

	static unique_ptr<Audio> startRecording(Audio *, int);

	class Lock
	{
	  private:
		const SDL_AudioDeviceID id;

	  public:
		Lock(SDL_AudioDeviceID);
		~Lock();
	};

  public:
	Audio() = delete;
	Audio(const Audio &) = delete;
	Audio(Audio &&) = delete;
	virtual ~Audio();

	void enqueueAudio(const ByteList &);

	bool encodeAndSendAudio(ClientConnection &);

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
	static unique_ptr<Audio> startRecordingWindow(const Message::Source &, const WindowProcess &, float);

	static unique_ptr<Audio> startRecording(const Message::Source &, const char *, float);
	static unique_ptr<Audio> startPlayback(const Message::Source &, const char *, float);
};
} // namespace TemStream
