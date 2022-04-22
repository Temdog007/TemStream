#include <main.hpp>

namespace TemStream
{
Configuration::Configuration()
	: colors(), fontFiles(), credentials(std::make_pair("User", "Password")), address(), fontSize(24.f),
	  defaultVolume(100), defaultSilenceThreshold(1), fontIndex(1), showLogs(false), showStreams(true),
	  showDisplays(false), showAudio(true), showFont(false), showStats(false), showColors(false)
{
	ImGuiStyle style;
	ImGui::StyleColorsLight(&style);
	std::copy(style.Colors, style.Colors + ImGuiCol_COUNT, colors.begin());
}
Configuration::~Configuration()
{
}
const char ConfigurationFile[] = "TemStream.data";
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
} // namespace TemStream