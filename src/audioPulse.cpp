#include <main.hpp>

namespace TemStream
{
const char createNullSinkCommand[] = "pactl load-module module-null-sink sink_name=%s_sink "
									 "sink_properties=device.description=%s_sink";

const char createComboSinkCommand[] = "pactl load-module module-combine-sink sink_name=%s_combo "
									  "sink_properties=device.description=%s_combo slaves=%s_sink,%s";

const char remapSourceCommand[] = "pactl load-module module-remap-source source_name=%s_remapped "
								  "master=%s_sink.monitor source_properties=device.description=%s_remapped";

const char moveSinkCommand[] = "pactl move-sink-input %d %s_combo";

class SinkInput
{
  private:
	String processName;
	int32_t processId;
	int32_t sinkInputId;
	int32_t currentSinkId;

  public:
	SinkInput(Deque<String> &);
	~SinkInput();

	bool valid() const;

	std::optional<String> getSinkName() const;

	static std::optional<List<SinkInput>> getSinks();
	static std::optional<String> runCommand(const char *);
	static bool runCommand(const char *, Deque<String> &);

	friend std::optional<WindowProcesses> AudioSource::getWindowsWithAudio();
	friend unique_ptr<AudioSource> AudioSource::startRecordingWindow(const Message::Source &, const WindowProcess &s,
																	 float);

	friend std::ostream &operator<<(std::ostream &os, const SinkInput &si)
	{
		os << si.processName << "(process: " << si.processId << "; current sink: " << si.currentSinkId
		   << "; sink input id: " << si.sinkInputId << std::endl;
		return os;
	}
};
class Sink
{
  private:
	int32_t id;

  public:
	Sink(const int32_t id) : id(id)
	{
	}
	Sink(const Sink &) = delete;
	Sink(Sink &&s) : id(s.id)
	{
		s.id = -1;
	}
	~Sink()
	{
		if (id > 0)
		{
			unloadSink(id);
			id = -1;
		}
	}

	int operator*() const
	{
		return id;
	}

	static void unloadSink(int32_t);
};
class SinkInputAudio : public AudioSource
{
  private:
	Sink nullSink;
	Sink comboSink;
	Sink remapSink;

	friend class Allocator<SinkInputAudio>;

  public:
	SinkInputAudio(const Message::Source &, float, Sink &&nullSink, Sink &&comboSink, Sink &&remapSink);
	virtual ~SinkInputAudio();
};

SinkInput::SinkInput(Deque<String> &deque) : processName(), processId(-1), sinkInputId(-1), currentSinkId(-1)
{
	bool foundSink = false;
	bool foundName = false;
	bool foundPID = false;
	bool foundId = false;
	while (!deque.empty() && !(foundId && foundName && foundSink && foundPID))
	{
		// Look for Sink, application.name, and application.process.id
		auto s = std::move(deque.front());
		deque.pop_front();
		s = trim(s);
		if (!foundName)
		{
			const char target[] = "application.name = \"";
			const auto index = s.rfind(target);
			if (index != std::string::npos)
			{
				foundName = true;
				// Quotes will be at end. So, one before end
				processName = String(s.begin() + index + sizeof(target) - 1, s.end() - 1);
				continue;
			}
		}
		if (!foundPID)
		{
			const char target[] = "application.process.id = \"";
			const auto index = s.rfind(target);
			if (index != std::string::npos)
			{
				foundPID = true;
				const String num(s.begin() + index + sizeof(target) - 1, s.end());
				processId = (int32_t)strtol(num.c_str(), nullptr, 10);
				continue;
			}
		}
		if (!foundSink)
		{
			const char target[] = "Sink: ";
			const auto index = s.rfind(target);
			if (index != std::string::npos)
			{
				foundSink = true;
				const String num(s.begin() + index + sizeof(target) - 1, s.end());
				currentSinkId = (int32_t)strtol(num.c_str(), nullptr, 10);
				continue;
			}
		}
		if (!foundId)
		{
			const char target[] = "Sink Input #";
			const auto index = s.rfind(target);
			if (index != std::string::npos)
			{
				foundId = true;
				const String num(s.begin() + index + sizeof(target) - 1, s.end());
				sinkInputId = (int32_t)strtol(num.c_str(), nullptr, 10);
				continue;
			}
		}
	}
}
SinkInput::~SinkInput()
{
}
bool SinkInput::valid() const
{
	return !processName.empty() && processId != -1 && currentSinkId != -1 && sinkInputId != -1;
}
bool SinkInput::runCommand(const char *command, Deque<String> &s)
{
	FILE *file = popen(command, "r");
	if (file == nullptr)
	{
		perror("popen");
		(*logger)(Logger::Level::Error) << "Failed to start process: " << strerror(errno) << std::endl;
		return false;
	}

	char buffer[KB(4)];
	while (fgets(buffer, sizeof(buffer), file) != nullptr)
	{
		s.emplace_back(buffer);
	}
	return pclose(file) == EXIT_SUCCESS;
}

std::optional<String> SinkInput::runCommand(const char *command)
{
	FILE *file = popen(command, "r");
	if (file == nullptr)
	{
		perror("popen");
		(*logger)(Logger::Level::Error) << "Failed to start process: " << strerror(errno) << std::endl;
		return std::nullopt;
	}

	String s;
	char buffer[KB(4)];
	while (fgets(buffer, sizeof(buffer), file) != nullptr)
	{
		s += buffer;
	}
	return pclose(file) == EXIT_SUCCESS ? std::make_optional(std::move(s)) : std::nullopt;
}
std::optional<String> SinkInput::getSinkName() const
{
	FILE *file = popen("pactl list sinks short", "r");
	if (file == nullptr)
	{
		perror("popen");
		(*logger)(Logger::Level::Error) << "Failed to start process: " << strerror(errno) << std::endl;
		return std::nullopt;
	}

	char buffer[KB(1)] = {0};
	String s;
	while (fgets(buffer, sizeof(buffer), file) != nullptr)
	{
		char *start = nullptr;
		const int fid = (int)strtol(buffer, &start, 10);
		if (currentSinkId != fid)
		{
			continue;
		}
		while (isspace(*start) && *start != '\0')
		{
			++start;
		}

		char *end = start;
		while (!isspace(*end) && *end != '\0')
		{
			++end;
		}

		const size_t len = (size_t)(end - start);
		s.insert(s.end(), start, start + len);
	}

	pclose(file);
	return s;
}
std::optional<List<SinkInput>> SinkInput::getSinks()
{
	Deque<String> deque;
	if (!runCommand("pactl list sink-inputs", deque))
	{
		(*logger)(Logger::Level::Error) << "Failed to get list of sinks" << std::endl;
		return std::nullopt;
	}

	List<SinkInput> list;
	while (!deque.empty())
	{
		list.emplace_back(deque);
	}
	auto iter = std::remove_if(list.begin(), list.end(), [](const SinkInput &si) { return !si.valid(); });
	list.erase(iter, list.end());
	return list;
}
std::optional<WindowProcesses> AudioSource::getWindowsWithAudio()
{
	auto sinks = SinkInput::getSinks();
	if (sinks.has_value())
	{
		WindowProcesses set;
		auto pair = toMoveIterator(std::move(*sinks));
		for (auto iter = pair.first; iter != pair.second; ++iter)
		{
			SinkInput sinkInput(std::move(*iter));
			set.emplace(std::move(sinkInput.processName), sinkInput.processId);
		}
		return set;
	}
	return std::nullopt;
}
unique_ptr<AudioSource> AudioSource::startRecordingWindow(const Message::Source &source, const WindowProcess &wp,
														  const float silenceThreshold)
{
	const auto sinks = SinkInput::getSinks();
	if (!sinks.has_value())
	{
		return nullptr;
	}

	auto iter = std::find_if(sinks->begin(), sinks->end(), [&wp](const SinkInput &sink) {
		return wp.name == sink.processName && wp.id == sink.processId;
	});
	if (iter == sinks->end())
	{
		return nullptr;
	}

	SinkInput sinkInput(std::move(*iter));
	char sinkName[KB(2)];
	snprintf(sinkName, sizeof(sinkName), "%s_%d", sinkInput.processName.c_str(), sinkInput.processId);

	// Create null sink. This is where the audio from window will be sent.
	char commandBuffer[KB(8)];
	snprintf(commandBuffer, sizeof(commandBuffer), createNullSinkCommand, sinkName, sinkName);
	auto tempStr = SinkInput::runCommand(commandBuffer);
	if (!tempStr.has_value())
	{
		(*logger)(Logger::Level::Error) << "Failed to create new audio sink" << std::endl;
		return nullptr;
	}
	Sink nullSink = Sink((int32_t)strtol(tempStr->c_str(), nullptr, 10));

	const auto realSinkName = sinkInput.getSinkName();
	if (!realSinkName.has_value())
	{
		return nullptr;
	}

	// Re-map null sink so that it is a device that SDL can detect for reocrding
	snprintf(commandBuffer, sizeof(commandBuffer), remapSourceCommand, sinkName, sinkName, sinkName);
	tempStr = SinkInput::runCommand(commandBuffer);
	if (!tempStr.has_value())
	{
		(*logger)(Logger::Level::Error) << "Failed to remap audio source" << std::endl;
		return nullptr;
	}
	Sink remapSink = (int32_t)strtol(tempStr->c_str(), nullptr, 10);

	// Create combo sink. This is so audio from window will go to speaker and null sink.
	snprintf(commandBuffer, sizeof(commandBuffer), createComboSinkCommand, sinkName, sinkName, sinkName,
			 realSinkName->c_str());
	tempStr = SinkInput::runCommand(commandBuffer);
	if (!tempStr.has_value())
	{
		(*logger)(Logger::Level::Error) << "Failed to create new audio sink" << std::endl;
		return nullptr;
	}
	Sink comboSink = (int32_t)strtol(tempStr->c_str(), nullptr, 10);

	// Change window's audio destination to the combo sink
	snprintf(commandBuffer, sizeof(commandBuffer), moveSinkCommand, sinkInput.sinkInputId, sinkName);
	tempStr = SinkInput::runCommand(commandBuffer);
	if (!tempStr.has_value())
	{
		(*logger)(Logger::Level::Error) << "Failed to move process audio source" << std::endl;
		return nullptr;
	}

	// Wait for Pulse AudioSource or SDL to update. SDL will fail to find the device if this is done too soon
	// (Is there a better way to do this?)
	(*logger)(Logger::Level::Trace) << "Waiting 1 second for audio server to update" << std::endl;
	// std::this_thread::sleep_for(1s);
	SDL_Delay(1000u);

	snprintf(commandBuffer, sizeof(commandBuffer), "%s_remapped", sinkName);
	SinkInputAudio *audio = allocateAndConstruct<SinkInputAudio>(source, silenceThreshold, std::move(nullSink),
																 std::move(comboSink), std::move(remapSink));
	audio->name = commandBuffer;
	return AudioSource::startRecording(audio, OPUS_APPLICATION_AUDIO);
}
SinkInputAudio::SinkInputAudio(const Message::Source &source, const float silenceThreshold, Sink &&nullSink,
							   Sink &&comboSink, Sink &&remapSink)
	: AudioSource(source, Type::RecordWindow, silenceThreshold), nullSink(std::move(nullSink)),
	  comboSink(std::move(comboSink)), remapSink(std::move(remapSink))
{
}
SinkInputAudio::~SinkInputAudio()
{
	close();
}
void Sink::unloadSink(const int32_t id)
{
	char buffer[KB(1)] = {0};
	snprintf(buffer, sizeof(buffer), "pactl unload-module %d", id);
	if (system(buffer) == EXIT_SUCCESS)
	{
		(*logger)(Logger::Level::Trace) << "Unloaded sink: " << id << std::endl;
	}
	else
	{
		(*logger)(Logger::Level::Warning) << "Failed to unload sink: " << id << std::endl;
	}
}
} // namespace TemStream