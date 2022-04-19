#include <main.hpp>

namespace TemStream
{
Audio::Audio(const MessageSource &source, const bool recording)
	: buffer(), recordBuffer(), source(source), storedAudio(), currentAudio(), spec(), decoder(nullptr), id(0),
	  code(SDLK_UNKNOWN), volume(1.f), pushToTalk(false), recording(recording)
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
void Audio::enqueueAudio(const Bytes &bytes)
{
	const int result = opus_decode_float(decoder, reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(),
										 fbuffer.data(), audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_120_MS), 0);
	if (result < 0)
	{
		(*logger)(Logger::Error) << "Failed to decode audio: " << opus_strerror(result) << std::endl;
	}
	else
	{
		const size_t framesRead = result * spec.channels;
		const size_t bytesRead = framesRead * sizeof(float);
		// (*logger)(Logger::Trace) << "Decoded bytes " << bytesRead << std::endl;
		for (size_t i = 0; i < framesRead; ++i)
		{
			fbuffer[i] = SDL_clamp(fbuffer[i] * volume, -1.f, 1.f);
		}
		SDL_LockAudioDevice(id);
		storedAudio.insert(storedAudio.end(), buffer.begin(), buffer.begin() + bytesRead);
		SDL_UnlockAudioDevice(id);
	}
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
	a->name = (name == nullptr ? "(Default audio device)" : name);
	*logger << "Recording audio from device: " << a->name << std::endl;
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
	a->name = (name == nullptr ? "(Default audio device)" : name);
	*logger << "Playing audio from device: " << a->name << std::endl;
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
	memset(data, spec.silence, count);
	if (storedAudio.size() > MB(1))
	{
		(*logger)(Logger::Warning) << "Audio delay is occurring for playback. Audio packets will be dropped."
								   << std::endl;
		storedAudio.clear();
	}
	const size_t toCopy = SDL_min((size_t)count, storedAudio.size());
	currentAudio.clear();
	if (toCopy > 0)
	{
		const auto start = storedAudio.begin();
		const auto end = storedAudio.begin() + toCopy;
		std::copy(start, end, data);
		currentAudio.insert(currentAudio.end(), start, end);
		storedAudio.erase(start, end);
	}
}
void Audio::recordAudio(const uint8_t *data, const int count)
{
	storedAudio.insert(storedAudio.end(), data, data + count);
	if (storedAudio.size() > MB(1))
	{
		(*logger)(Logger::Warning) << "Audio delay is occurring for recording. Audio packets will be dropped."
								   << std::endl;
		storedAudio.clear();
		return;
	}

	currentAudio.clear();
	currentAudio.insert(currentAudio.end(), data, data + count);
}
bool Audio::encodeAndSendAudio(ClientPeer &peer)
{
	if (!isRecording())
	{
		return true;
	}

	const int minDuration = audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_10_MS);
	while (!storedAudio.empty())
	{
		int frameSize = storedAudio.size() / (spec.channels * sizeof(float));
		if (frameSize < minDuration)
		{
			break;
		}

		frameSize = closestValidFrameCount(spec.freq, frameSize);
		const size_t bytesUsed = (frameSize * spec.channels * sizeof(float));

		const auto start = storedAudio.begin();
		const auto end = storedAudio.begin() + bytesUsed;
		recordBuffer.clear();
		recordBuffer.insert(recordBuffer.end(), start, end);
		storedAudio.erase(start, end);

		const int result = opus_encode_float(encoder, reinterpret_cast<float *>(recordBuffer.data()), frameSize,
											 reinterpret_cast<unsigned char *>(buffer.data()), buffer.size());
		if (result < 0)
		{
			(*logger)(Logger::Error) << "Failed to encode audio packet: " << opus_strerror(result)
									 << "; Frame size: " << frameSize << std::endl;
			break;
		}

		MessagePacket packet;
		packet.source = source;
		packet.message = AudioMessage{Bytes(buffer.begin(), buffer.begin() + result)};
		if (!peer->sendPacket(packet))
		{
			return false;
		}
		peer.addPacket(std::move(packet));
	}
	return true;
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