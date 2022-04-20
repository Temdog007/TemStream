#pragma once

#include <main.hpp>

namespace TemStream
{
class ClientPeer;
struct WindowProcess
{
	String name;
	int32_t id;

	bool operator<(const WindowProcess &p) const;
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

	template <class T, class... Args> friend T *allocate(Args &&...);

  protected:
	Audio(const MessageSource &, bool);

	static unique_ptr<Audio> startRecording(Audio *, int);

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

	static std::optional<Set<WindowProcess>> getWindowsWithAudio();
	static unique_ptr<Audio> startRecordingWindow(const MessageSource &, const WindowProcess &);

	static unique_ptr<Audio> startRecording(const MessageSource &, const char *);
	static unique_ptr<Audio> startPlayback(const MessageSource &, const char *);

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
namespace std
{
template <> struct hash<TemStream::WindowProcess>
{
	size_t operator()(const TemStream::WindowProcess &s) const
	{
		size_t value = hash<TemStream::String>()(s.name);
		TemStream::hash_combine(value, hash<int32_t>()(s.id));
		return value;
	}
};
} // namespace std