#pragma once

#include <main.hpp>

#define CONFIGURATION_ARCHIVE(archive)                                                                                 \
	archive(fontFiles, credentials, address, fontSize, fontIndex, showLogs, showStreams, showDisplays, showAudio,      \
			showFont, showStats)
namespace TemStream
{
struct Configuration
{
	List<String> fontFiles;
	Message::Credentials credentials;
	Address address;
	float fontSize;
	int fontIndex;
	bool showLogs;
	bool showStreams;
	bool showDisplays;
	bool showAudio;
	bool showFont;
	bool showStats;

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