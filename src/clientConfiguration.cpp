#include <main.hpp>

namespace TemStream
{
Configuration::Configuration()
	: fontFiles(), credentials(std::make_pair("User", "Password")), address(), fontSize(24.f), fontIndex(1),
	  showLogs(false), showStreams(true), showDisplays(false), showAudio(true), showFont(false), showStats(false)
{
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
			cereal::BinaryInputArchive ar(file);
			ar(configuration);
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
			cereal::BinaryOutputArchive ar(file);
			ar(configuration);
		}
	}
	catch (const std::exception &e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to save configuration", e.what(), nullptr);
	}
}
} // namespace TemStream