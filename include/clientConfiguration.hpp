#pragma once

#include <main.hpp>

#define CONFIGURATION_ARCHIVE(archive)                                                                                 \
	archive(colors, customColors, fontFiles, credentials, address, fontSize, defaultVolume, defaultSilenceThreshold,   \
			fontIndex, showLogs, showStreams, showDisplays, showAudio, showFont, showStats, showColors)
namespace TemStream
{
using ColorList = std::array<ImVec4, ImGuiCol_COUNT>;
struct Configuration
{
	ColorList colors;
	Map<String, ColorList> customColors;
	List<String> fontFiles;
	Message::Credentials credentials;
	Address address;
	float fontSize;
	int defaultVolume;
	int defaultSilenceThreshold;
	int fontIndex;
	bool showLogs;
	bool showStreams;
	bool showDisplays;
	bool showAudio;
	bool showFont;
	bool showStats;
	bool showColors;

	Configuration();
	~Configuration();

	template <class Archive> void save(Archive &archive) const
	{
		CONFIGURATION_ARCHIVE(archive);
	}

	template <class Archive> void load(Archive &archive)
	{
		CONFIGURATION_ARCHIVE(archive);
	}
};
} // namespace TemStream