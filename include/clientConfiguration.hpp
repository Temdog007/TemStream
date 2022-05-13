#pragma once

#include "imgui_serialize.hpp"
#include <main.hpp>

#define CONFIGURATION_ARCHIVE(archive)                                                                                 \
	archive(cereal::make_nvp("fontFiles", fontFiles), cereal::make_nvp("address", address), CEREAL_NVP(maxLogs),       \
			CEREAL_NVP(fontSize), CEREAL_NVP(defaultVolume), CEREAL_NVP(defaultSilenceThreshold),                      \
			CEREAL_NVP(fontIndex), CEREAL_NVP(showLogs), CEREAL_NVP(autoScrollLogs), CEREAL_NVP(showConnections),      \
			CEREAL_NVP(showDisplays), CEREAL_NVP(showAudio), CEREAL_NVP(showVideo), CEREAL_NVP(showFont),              \
			CEREAL_NVP(showStats), CEREAL_NVP(showStyleEditor), CEREAL_NVP(showLogsFilter), CEREAL_NVP(currentStyle),  \
			CEREAL_NVP(styles))
namespace TemStream
{
struct Configuration
{
	Map<std::string, ImGuiStyle> styles;
	std::string currentStyle;
	std::string newStyleName;
	std::vector<std::string> fontFiles;
	Message::Credentials credentials;
	STL_Address address;
	float fontSize;
	int defaultVolume;
	int defaultSilenceThreshold;
	int fontIndex;
	int maxLogs;
	bool showLogs;
	bool autoScrollLogs;
	bool showConnections;
	bool showDisplays;
	bool showAudio;
	bool showVideo;
	bool showFont;
	bool showStats;
	bool showStyleEditor;

	struct ShowLogsFilter
	{
		bool errors;
		bool warnings;
		bool info;
		bool trace;

		ShowLogsFilter() : errors(true), warnings(true), info(true), trace(false)
		{
		}
		~ShowLogsFilter()
		{
		}

		template <class Archive> void save(Archive &archive) const
		{
			archive(CEREAL_NVP(errors), CEREAL_NVP(warnings), CEREAL_NVP(info), CEREAL_NVP(trace));
		}

		template <class Archive> void load(Archive &archive)
		{
			archive(CEREAL_NVP(errors), CEREAL_NVP(warnings), CEREAL_NVP(info), CEREAL_NVP(trace));
		}
	} showLogsFilter;

	Configuration();
	~Configuration();

	template <class Archive> void save(Archive &archive) const
	{
		archive(cereal::make_nvp("credentialType", credentials.index()));
		struct Foo
		{
			Archive &archive;
			void operator()(const String &token)
			{
				// JSON requires STL string
				std::string s(token);
				archive(s);
			}
			void operator()(const Message::UsernameAndPassword &uap)
			{
				// JSON requires STL string
				// Don't save password
				std::string u(uap.first);
				archive(u);
			}
		};
		std::visit(Foo{archive}, credentials);
		CONFIGURATION_ARCHIVE(archive);
	}

	template <class Archive> void load(Archive &archive)
	{
		// Don't load password
		uint32_t index = 0;
		archive(cereal::make_nvp("credentialType", index));
		std::string s;
		archive(s);
		switch (index)
		{
		case variant_index<Message::Credentials, String>():
			credentials.emplace<String>(std::move(s));
			break;
		case variant_index<Message::Credentials, Message::UsernameAndPassword>():
			credentials.emplace<Message::UsernameAndPassword>(std::move(s), "");
			break;
		default:
			break;
		}
		CONFIGURATION_ARCHIVE(archive);
	}
};
} // namespace TemStream
