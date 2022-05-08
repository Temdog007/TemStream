#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char authenticationFile[] = "authentication.txt";

const uint32_t WriteAccess = 1 << 0;
const uint32_t ReplayAccess = 1 << 1;
const uint32_t Moderator = 1 << 2;
const uint32_t Owner = 1 << 3;

bool checkFile(const std::function<bool(const std::vector<std::string> &)> &func)
{
	std::ifstream file(authenticationFile);
	if (!file.is_open())
	{
		return false;
	}

	std::vector<std::string> values;
	for (std::string line; std::getline(file, line);)
	{
		values.clear();
		std::istringstream ss(line);
		for (std::string word; std::getline(ss, word, ' ');)
		{
			values.emplace_back(std::move(word));
		}
		if (func(values))
		{
			return true;
		}
	}
	return false;
}

uint32_t getFlags(const std::string &s)
{
	uint32_t f = 0;
	if (s == "write")
	{
		f |= WriteAccess;
	}
	if (s == "replay")
	{
		f |= ReplayAccess;
	}
	if (s == "mod")
	{
		f |= Moderator;
	}
	if (s == "owner")
	{
		f |= Owner;
	}
	return f;
}
} // namespace

extern "C" bool VerifyToken(const char *token, char (&username)[32], std::uint32_t &flags)
{
	return checkFile([token, &username, &flags](const std::vector<std::string> &v) mutable {
		if (v.size() < 2)
		{
			return false;
		}

		if (v[0] != token)
		{
			return false;
		}

		std::strncpy(username, v[1].c_str(), 32);
		for (size_t i = 2; i < v.size(); ++i)
		{
			flags |= getFlags(v[i]);
		}
		return true;
	});
}

extern "C" bool VerifyUsernameAndPassword(const char *username, const char *password, std::uint32_t &flags)
{
	return checkFile([username, password, &flags](const std::vector<std::string> &v) {
		if (v.size() < 2)
		{
			return false;
		}

		if (v[0] != username || v[1] != password)
		{
			return false;
		}

		for (size_t i = 2; i < v.size(); ++i)
		{
			flags |= getFlags(v[i]);
		}
		return true;
	});
}