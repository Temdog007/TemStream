#include <main.hpp>

namespace TemStream
{
Configuration::Configuration()
	: colors(), fontFiles(), credentials(std::make_pair("User", "Password")), address(), fontSize(24.f),
	  defaultVolume(100), defaultSilenceThreshold(0), fontIndex(1), maxLogs(10000), showLogs(false), showStreams(true),
	  showDisplays(false), showAudio(false), showFont(false), showStats(false), showColors(false)
{
	ImGuiStyle style;
	ImGui::StyleColorsClassic(&style);
	std::copy(style.Colors, style.Colors + ImGuiCol_COUNT, colors.begin());
}
Configuration::~Configuration()
{
}
#if TEMSTREAM_CLIENT_JSON_CONFIG
const char ConfigurationFile[] = "TemStreamConfig.json";
#else
const char ConfigurationFile[] = "TemStreamConfig.data";
#endif
Configuration loadConfiguration(int, const char **)
{
	Configuration configuration;
	try
	{
		std::ifstream file(ConfigurationFile);
		if (file.is_open())
		{
#if TEMSTREAM_CLIENT_JSON_CONFIG
			cereal::JSONInputArchive ar(file);
			ar(configuration);
#else
			cereal::BinaryInputArchive ar(file);
			ar(configuration);
#endif
		}
	}
	catch (const std::exception &e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to load configuration", e.what(), nullptr);
	}
	return configuration;
}
void saveConfiguration(const Configuration &configuration)
{
	try
	{
		std::ofstream file(ConfigurationFile);
		if (file.is_open())
		{
#if TEMSTREAM_CLIENT_JSON_CONFIG
			cereal::JSONOutputArchive ar(file);
			ar(configuration);
#else
			cereal::BinaryOutputArchive ar(file);
			ar(configuration);
#endif
		}
	}
	catch (const std::exception &e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to save configuration", e.what(), nullptr);
	}
}
std::unordered_map<std::string, ColorList> Configuration::toCustomColors() const
{
	return std::unordered_map<std::string, ColorList>(customColors.begin(), customColors.end());
}
void Configuration::fromCustomColors(std::unordered_map<std::string, ColorList> &&c)
{
	auto pair = toMoveIterator(std::move(c));
	customColors.clear();
	for (auto iter = pair.first; iter != pair.second; ++iter)
	{
		customColors.emplace(*iter);
	}
}
std::vector<std::string> Configuration::toFontFiles() const
{
	return std::vector<std::string>(fontFiles.begin(), fontFiles.end());
}
void Configuration::fromFontFiles(std::vector<std::string> &&v)
{
	auto pair = toMoveIterator(std::move(v));
	fontFiles.clear();
	for (auto iter = pair.first; iter != pair.second; ++iter)
	{
		fontFiles.emplace_back(*iter);
	}
}
std::string Configuration::toAddress() const
{
	return std::string(address.hostname);
}
void Configuration::fromAddress(std::string &&s)
{
	address.hostname = std::move(s);
}
} // namespace TemStream