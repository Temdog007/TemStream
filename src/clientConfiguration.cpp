/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <main.hpp>

namespace TemStream
{
Configuration::Configuration()
	: styles(), currentStyle("classic"), newStyleName("newStyle"), fontFiles(),
	  credentials(std::make_pair("User", "Password")), address(), fontSize(16.f), defaultVolume(100),
	  defaultSilenceThreshold(0), fontIndex(1), maxLogs(10000), width(800), height(600), fullscreen(false),
	  showLogs(false), showConnections(true), showDisplays(false), showAudio(false), showFont(false), showStats(false),
	  showStyleEditor(false), isEncrypted(true)
{
	ImGuiStyle style;

	ImGui::StyleColorsClassic(&style);
	styles.try_emplace("classic", style);

	ImGui::StyleColorsDark(&style);
	styles.try_emplace("dark", style);

	ImGui::StyleColorsLight(&style);
	styles.try_emplace("light", style);

	Colors::StyleDeepDark(style);
	styles.try_emplace("deep_dark", style);

	Colors::StyleGold(style);
	styles.try_emplace("gold", style);

	Colors::StyleGreen(style);
	styles.try_emplace("green", style);

	Colors::StyleRed(style);
	styles.try_emplace("red", style);
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
} // namespace TemStream