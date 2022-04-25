#pragma once

#include <main.hpp>

#include "imguiName.hpp"

#define CONFIGURATION_ARCHIVE(archive)                                                                                 \
	auto cc = toCustomColors();                                                                                        \
	auto ff = toFontFiles();                                                                                           \
	auto hostname = toAddress();                                                                                       \
	archive(cereal::make_nvp("customColors", cc), cereal::make_nvp("fontFiles", ff), CEREAL_NVP(hostname),             \
			cereal::make_nvp("port", address.port), CEREAL_NVP(fontSize), CEREAL_NVP(defaultVolume),                   \
			CEREAL_NVP(defaultSilenceThreshold), CEREAL_NVP(fontIndex), CEREAL_NVP(showLogs), CEREAL_NVP(showStreams), \
			CEREAL_NVP(showDisplays), CEREAL_NVP(showAudio), CEREAL_NVP(showVideo), CEREAL_NVP(showFont),              \
			CEREAL_NVP(showStats), CEREAL_NVP(showColors), CEREAL_NVP(showLogsFilter), CEREAL_NVP(colors))
namespace TemStream
{
class ColorList
{
  private:
	std::array<ImVec4, ImGuiCol_COUNT> data;

  public:
	template <class Archive> void save(Archive &archive) const
	{
		for (size_t i = 0; i < data.size(); ++i)
		{
			archive(cereal::make_nvp(ImGuiColNames[i], data[i]));
		}
	}
	template <class Archive> void load(Archive &archive)
	{
		for (size_t i = 0; i < data.size(); ++i)
		{
			archive(cereal::make_nvp(ImGuiColNames[i], data[i]));
		}
	}

	auto begin()
	{
		return data.begin();
	}
	auto end()
	{
		return data.end();
	}
};
struct Configuration
{
	ColorList colors;
	Map<String, ColorList> customColors;
	StringList fontFiles;
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
	bool showVideo;
	bool showFont;
	bool showStats;
	bool showColors;
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
		// Don't save credentials
		CONFIGURATION_ARCHIVE(archive);
	}

	template <class Archive> void load(Archive &archive)
	{
		// Don't load credentials
		CONFIGURATION_ARCHIVE(archive);
		fromCustomColors(std::move(cc));
		fromFontFiles(std::move(ff));
		fromAddress(std::move(hostname));
	}

	std::unordered_map<std::string, ColorList> toCustomColors() const;
	void fromCustomColors(std::unordered_map<std::string, ColorList> &&);

	std::vector<std::string> toFontFiles() const;
	void fromFontFiles(std::vector<std::string> &&);

	std::string toAddress() const;
	void fromAddress(std::string &&);
};
} // namespace TemStream