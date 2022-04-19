#include <main.hpp>

namespace TemStream
{
Audio::Audio(const MessageSource &source, const bool recording)
	: source(source), storedAudio(), currentAudio(), spec(), decoder(nullptr), id(0), code(SDLK_UNKNOWN), volume(1.f),
	  pushToTalk(false), recording(recording)
{
}
Audio::~Audio()
{
	SDL_CloseAudioDevice(id);
	if (isRecording() && encoder != nullptr)
	{
		free(encoder);
		encoder = nullptr;
	}
	else if (decoder != nullptr)
	{
		free(decoder);
		decoder = nullptr;
	}
}
void Audio::enqueueAudio(Bytes &bytes)
{
	SDL_LockAudioDevice(id);
	float *fdata = reinterpret_cast<float *>(bytes.data());
	const size_t fsize = bytes.size() / sizeof(float);
	for (size_t i = 0; i < fsize; ++i)
	{
		fdata[i] *= volume;
	}
	storedAudio.insert(storedAudio.end(), bytes.begin(), bytes.end());
	SDL_UnlockAudioDevice(id);
}
void Audio::useCurrentAudio(const std::function<void(const Bytes &)> &f) const
{
	f(currentAudio);
}
SDL_AudioSpec Audio::getAudioSpec()
{
	SDL_AudioSpec desired;
	desired.channels = 2;
	desired.format = AUDIO_F32;
	desired.freq = 48000;
	desired.samples = 4096;
	return desired;
}
std::shared_ptr<Audio> Audio::startRecording(const MessageSource &source, const char *name)
{
	auto a = std::shared_ptr<Audio>(new Audio(source, true));

	SDL_AudioSpec desired = getAudioSpec();
	desired.callback = (SDL_AudioCallback)Audio::recordCallback;
	desired.userdata = a.get();

	a->id = SDL_OpenAudioDevice(name, SDL_TRUE, &desired, &a->spec, 0);
	if (a->id == 0)
	{
		(*logger)(Logger::Error) << "Failed to open audio device: " << SDL_GetError() << std::endl;
		return nullptr;
	}

	const int size = opus_encoder_get_size(a->spec.channels);
	a->encoder = reinterpret_cast<OpusEncoder *>(malloc(size));
	const int error = opus_encoder_init(a->encoder, a->spec.freq, a->spec.channels, OPUS_APPLICATION_VOIP);
	if (error < 0)
	{
		(*logger)(Logger::Error) << "Failed to create audio encoder: " << opus_strerror(error) << std::endl;
		return nullptr;
	}

	SDL_PauseAudioDevice(a->id, SDL_FALSE);
	*logger << "Recording audio from device: " << (name == nullptr ? "(Default)" : name) << std::endl;
	return a;
}
std::shared_ptr<Audio> Audio::startPlayback(const MessageSource &source, const char *name)
{
	auto a = std::shared_ptr<Audio>(new Audio(source, false));

	SDL_AudioSpec desired = getAudioSpec();
	desired.callback = (SDL_AudioCallback)Audio::playbackCallback;
	desired.userdata = a.get();

	a->id = SDL_OpenAudioDevice(name, SDL_FALSE, &desired, &a->spec, 0);
	if (a->id == 0)
	{
		(*logger)(Logger::Error) << "Failed to open audio device: " << SDL_GetError() << std::endl;
		return nullptr;
	}

	const int size = opus_decoder_get_size(a->spec.channels);
	a->decoder = reinterpret_cast<OpusDecoder *>(malloc(size));
	const int error = opus_decoder_init(a->decoder, a->spec.freq, a->spec.channels);
	if (error < 0)
	{
		(*logger)(Logger::Error) << "Failed to create audio decoder: " << opus_strerror(error) << std::endl;
		return nullptr;
	}

	SDL_PauseAudioDevice(a->id, SDL_FALSE);
	*logger << "Playing audio from device: " << (name == nullptr ? "(Default)" : name) << std::endl;
	return a;
}
void Audio::recordCallback(Audio *a, const uint8_t *data, const int count)
{
	a->recordAudio(data, count);
}
void Audio::playbackCallback(Audio *a, uint8_t *data, const int count)
{
	a->playbackAudio(data, count);
}
void Audio::playbackAudio(uint8_t *data, const int count)
{
	const size_t toCopy = SDL_min((size_t)count, storedAudio.size());
	currentAudio.clear();
	if (toCopy > 0)
	{
		memcpy(data, storedAudio.data(), toCopy);
		const auto start = storedAudio.begin();
		const auto end = storedAudio.begin() + toCopy;
		currentAudio.insert(currentAudio.end(), start, end);
		storedAudio.erase(start, end);
	}
}
void Audio::recordAudio(const uint8_t *data, const int count)
{
	currentAudio.clear();
	currentAudio.insert(currentAudio.end(), data, data + count);
	storedAudio.insert(storedAudio.end(), data, data + count);
	if (storedAudio.size() > MB(1))
	{
		storedAudio.clear();
		return;
	}

	const int minDuration = audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_10_MS);
	int bytesRead = 0;
	uint8_t buffer[KB(4)];
	while (bytesRead < count)
	{
		int frameSize = (count - bytesRead) / (spec.channels * sizeof(float));
		if (frameSize < minDuration)
		{
			break;
		}

		frameSize = closestValidFrameCount(spec.freq, frameSize);

		const int result = opus_encode_float(encoder, reinterpret_cast<float *>(storedAudio.data() + bytesRead),
											 frameSize, buffer, sizeof(buffer));
		if (result < 0)
		{
			(*logger)(Logger::Error) << "Failed to encode audio packet: " << opus_strerror(result)
									 << "; Frame size: " << frameSize << std::endl;
			break;
		}

		const size_t bytesUsed = (frameSize * spec.channels * sizeof(float));
		bytesRead += bytesUsed;

		MessagePacket *packet = new MessagePacket();
		packet->source = source;
		packet->message = AudioMessage{Bytes(buffer, buffer + result)};
		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = TemStreamEvent::SendSingleMessagePacket;
		e.user.data1 = packet;
		if (SDL_PushEvent(&e) != 1)
		{
			(*logger)(Logger::Error) << "Failed to add SDL event: " << SDL_GetError() << std::endl;
			delete packet;
		}
	}
	storedAudio.erase(storedAudio.begin(), storedAudio.begin() + bytesRead);
}
int Audio::closestValidFrameCount(const int frequency, const int frames)
{
	const int values[] = {OPUS_FRAMESIZE_120_MS, OPUS_FRAMESIZE_100_MS, OPUS_FRAMESIZE_80_MS,
						  OPUS_FRAMESIZE_60_MS,	 OPUS_FRAMESIZE_40_MS,	OPUS_FRAMESIZE_20_MS,
						  OPUS_FRAMESIZE_10_MS,	 OPUS_FRAMESIZE_5_MS,	OPUS_FRAMESIZE_2_5_MS};
	const int arrayLength = sizeof(values) / sizeof(int);
	if (frames >= audioLengthToFrames(frequency, values[0]))
	{
		return audioLengthToFrames(frequency, values[0]);
	}
	if (frames <= audioLengthToFrames(frequency, values[arrayLength - 1]))
	{
		return audioLengthToFrames(frequency, values[arrayLength - 1]);
	}
	for (int i = 1; i < arrayLength; ++i)
	{
		const int prev = audioLengthToFrames(frequency, values[i - 1]);
		const int current = audioLengthToFrames(frequency, values[i]);
		if (prev >= frames && frames >= current)
		{
			return audioLengthToFrames(frequency, values[i]);
		}
	}
	return audioLengthToFrames(frequency, values[arrayLength - 1]);
}
int Audio::audioLengthToFrames(const int frequency, const int duration)
{
	switch (duration)
	{
	case OPUS_FRAMESIZE_2_5_MS:
		return frequency / 400;
	case OPUS_FRAMESIZE_5_MS:
		return frequency / 200;
	case OPUS_FRAMESIZE_10_MS:
		return frequency / 100;
	case OPUS_FRAMESIZE_20_MS:
		return frequency / 50;
	case OPUS_FRAMESIZE_40_MS:
		return frequency / 25;
	case OPUS_FRAMESIZE_60_MS:
		return audioLengthToFrames(frequency, OPUS_FRAMESIZE_20_MS) * 3;
	case OPUS_FRAMESIZE_80_MS:
		return audioLengthToFrames(frequency, OPUS_FRAMESIZE_40_MS) * 2;
	case OPUS_FRAMESIZE_100_MS:
		return frequency / 10;
	case OPUS_FRAMESIZE_120_MS:
		return audioLengthToFrames(frequency, OPUS_FRAMESIZE_60_MS) * 2;
	default:
		return 0;
	}
}
} // namespace TemStream