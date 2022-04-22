#pragma once

#include <main.hpp>

namespace TemStream
{
class ClientConnetion;
struct WindowProcess
{
	String name;
	int32_t id;

	WindowProcess(const String &name, const int32_t id) : name(name), id(id)
	{
	}
	WindowProcess(String &&name, const int32_t id) : name(std::move(name)), id(id)
	{
	}
	~WindowProcess()
	{
	}

	bool operator==(const WindowProcess &p) const
	{
		return id == p.id && name == p.name;
	}
	bool operator!=(const WindowProcess &p) const
	{
		return !(*this == p);
	}
};
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
	List<char> recordBuffer;
	const Message::Source source;
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

	static int audioLengthToFrames(const int frequency, const int duration);

	static int closestValidFrameCount(const int frequency, const int frames);

	friend class Allocator<Audio>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

  protected:
	Audio(const Message::Source &, Type, float);

	void close();

	static unique_ptr<Audio> startRecording(Audio *, int);

  public:
	Audio() = delete;
	Audio(const Audio &) = delete;
	Audio(Audio &&) = delete;
	virtual ~Audio();

	void enqueueAudio(const Bytes &);

	bool encodeAndSendAudio(ClientConnetion &);

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

	void useCurrentAudio(const std::function<void(const Bytes &)> &) const;
	Bytes &&getCurrentAudio();

	void clampAudio(float *, int) const;

	static std::optional<Set<WindowProcess>> getWindowsWithAudio();
	static unique_ptr<Audio> startRecordingWindow(const Message::Source &, const WindowProcess &, float);

	static unique_ptr<Audio> startRecording(const Message::Source &, const char *, float);
	static unique_ptr<Audio> startPlayback(const Message::Source &, const char *, float);

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