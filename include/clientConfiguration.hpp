#pragma once

#include <main.hpp>

#define CONFIGURATION_ARCHIVE(archive)                                                                                 \
	auto cc = toCustomColors();                                                                                        \
	auto ff = toFontFiles();                                                                                           \
	auto cred = toCredentials();                                                                                       \
	auto hostname = toAddress();                                                                                       \
	archive(cereal::make_nvp("customColors", cc), cereal::make_nvp("fontFiles", ff),                                   \
			cereal::make_nvp("credentials", cred), CEREAL_NVP(hostname), cereal::make_nvp("port", address.port),       \
			CEREAL_NVP(fontSize), CEREAL_NVP(defaultVolume), CEREAL_NVP(defaultSilenceThreshold),                      \
			CEREAL_NVP(fontIndex), CEREAL_NVP(showLogs), CEREAL_NVP(showStreams), CEREAL_NVP(showDisplays),            \
			CEREAL_NVP(showAudio), CEREAL_NVP(showFont), CEREAL_NVP(showStats), CEREAL_NVP(showColors),                \
			CEREAL_NVP(colors))
namespace TemStream
{
using ColorList = std::array<ImVec4, ImGuiCol_COUNT>;
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
		fromCustomColors(std::move(cc));
		fromFontFiles(std::move(ff));
		fromCredentials(std::move(cred));
		fromAddress(std::move(hostname));
	}

	std::unordered_map<std::string, ColorList> toCustomColors() const;
	void fromCustomColors(std::unordered_map<std::string, ColorList> &&);

	std::vector<std::string> toFontFiles() const;
	void fromFontFiles(std::vector<std::string> &&);

	std::variant<std::string, std::pair<std::string, std::string>> toCredentials() const;
	void fromCredentials(std::variant<std::string, std::pair<std::string, std::string>> &&);

	std::string toAddress() const;
	void fromAddress(std::string &&);
};
} // namespace TemStream