#include <main.hpp>

#include "fonts/Cousine.cpp"
#include "fonts/DroidSans.cpp"
#include "fonts/Karla.cpp"
#include "fonts/ProggyClean.cpp"
#include "fonts/ProggyTiny.cpp"
#include "fonts/Roboto.cpp"
#include "fonts/Ubuntuu.cpp"

const void *Fonts[]{Cousine_compressed_data,	 DroidSans_compressed_data,	 Karla_compressed_data,
					ProggyClean_compressed_data, ProggyTiny_compressed_data, Roboto_compressed_data,
					Ubuntuu_compressed_data};
const unsigned int FontSizes[]{Cousine_compressed_size,		DroidSans_compressed_size,	Karla_compressed_size,
							   ProggyClean_compressed_size, ProggyTiny_compressed_size, Roboto_compressed_size,
							   Ubuntuu_compressed_size};

const ImWchar MinCharacters = 0x1;
const ImWchar MaxCharacters = 0x1FFFF;

uint8_t *allocatorMalloc(const size_t size)
{
	TemStream::Allocator<uint8_t> a;
	return a.allocate(size);
}

void allocatorFree(uint8_t *old)
{
	TemStream::Allocator<uint8_t> a;
	a.deallocate(old, 1);
}

uint8_t *allocatorCalloc(const size_t num, const size_t size)
{
	uint8_t *data = allocatorMalloc(size * num);
	memset(data, 0, size);
	return data;
}

uint8_t *allocatorImGuiAlloc(const size_t size, void *)
{
	return allocatorMalloc(size);
}

void allocatorImGuiFree(uint8_t *ptr, void *)
{
	return allocatorFree(ptr);
}

uint8_t *allocatorReallocate(uint8_t *old, const size_t newSize)
{
	TemStream::Allocator<uint8_t> a;
	const size_t oldSize = a.getBlockSize(old);
	uint8_t *data = allocatorMalloc(newSize);
	memcpy(data, old, oldSize);
	a.deallocate(old, 1);
	return data;
}

namespace TemStream
{
void handleWorkThread(TemStreamGui *gui);

TemStreamGui::TemStreamGui(ImGuiIO &io)
	: connectToServer(), peerInfo({"User", false}), peerMutex(), peer(nullptr), queryData(nullptr),
#if THREADS_AVAILABLE
	  workThread(handleWorkThread, this),
#endif
	  fontFiles(), io(io), window(nullptr), renderer(nullptr), allUTF32(getAllUTF32()), fontSize(24.f), fontIndex(1),
	  showLogs(false), showStreams(false), showDisplays(false), showAudio(false), showFont(false), showStats(false)
{
}

TemStreamGui::~TemStreamGui()
{
#if THREADS_AVAILABLE
	appDone = true;
	workThread.join();
#endif
	SDL_DestroyRenderer(renderer);
	renderer = nullptr;
	SDL_DestroyWindow(window);
	window = nullptr;
	IMG_Quit();
	SDL_Quit();
}

uint32_t updatePeer(uint32_t interval, TemStreamGui *gui)
{
	gui->update();
	return interval;
}

void TemStreamGui::update()
{
	LOCK(peerMutex);

	if (peer == nullptr)
	{
		return;
	}

	if (!peer->readAndHandle(0))
	{
		(*logger)(Logger::Trace) << "TemStreamGui::update: error" << std::endl;
		onDisconnect(peer->gotServerInformation());
		peer = nullptr;
	}
	for (const auto &a : audio)
	{
		if (!a.second->encodeAndSendAudio(*peer))
		{
			(*logger)(Logger::Trace) << "TemStreamGui::update: error" << std::endl;
			onDisconnect(peer->gotServerInformation());
			peer = nullptr;
		}
	}
}

uint32_t handleWork(uint32_t interval, TemStreamGui *gui)
{
	gui->doWork();
	return interval;
}

void handleWorkThread(TemStreamGui *gui)
{
	using namespace std::chrono_literals;
	while (!appDone)
	{
		gui->doWork();
		std::this_thread::sleep_for(100ms);
	}
}

void TemStreamGui::doWork()
{
	auto task = workQueue.getWork();
	if (!task.has_value())
	{
		return;
	}

	std::visit(workQueue, *task);
}

void TemStreamGui::onDisconnect(const bool gotInformation)
{
	const char *message = nullptr;
	if (gotInformation)
	{
		message = "Lost connection to server";
	}
	else
	{
		message = "Failed to connect to server";
	}
	(*logger)(Logger::Error) << message << std::endl;
}

void TemStreamGui::pushFont()
{
	if (fontIndex >= io.Fonts->Fonts.size())
	{
		fontIndex = 0;
	}
	ImGui::PushFont(io.Fonts->Fonts[fontIndex]);
}

bool TemStreamGui::init()
{
#if USE_CUSTOM_ALLOCATOR
	SDL_MemoryFunctions memFuncs;
	memFuncs.GetFromSDL();
	SDL_SetMemoryFunctions((SDL_malloc_func)allocatorMalloc, (SDL_calloc_func)allocatorCalloc,
						   (SDL_realloc_func)allocatorReallocate, (SDL_free_func)allocatorFree);
#endif

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		logSDLError("SDL error");
		return false;
	}

	const int flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP;
	if (IMG_Init(flags) != flags)
	{
		(*logger)(Logger::Error) << "Image error: " << IMG_GetError() << std::endl;
		return false;
	}

	window = SDL_CreateWindow("TemStream", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,
							  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == nullptr)
	{
		logSDLError("Failed to create window");
		return false;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr)
	{
		logSDLError("Failed to create renderer");
		return false;
	}

	if (SDL_AddTimer(1, (SDL_TimerCallback)updatePeer, this) == 0)
	{
		logSDLError("Failed to add timer");
		return false;
	}

#if !THREADS_AVAILABLE
	if (SDL_AddTimer(100, (SDL_TimerCallback)handleWork, this) == 0)
	{
		logSDLError("Failed to add timer");
		return false;
	}
#endif

	return true;
}

bool TemStreamGui::connect(const Address &address)
{
	*logger << "Connecting to server: " << address << std::endl;
	auto s = address.makeTcpSocket();
	if (s == nullptr)
	{
		(*logger)(Logger::Error) << "Failed to connect to server: " << address << std::endl;
		return false;
	}

	LOCK(peerMutex);
	peer = tem_unique<ClientConnetion>(address, std::move(s));
	*logger << "Connected to server: " << address << std::endl;
	Message::Packet packet;
	packet.payload.emplace<PeerInformation>(peerInfo);
	return (*peer)->sendPacket(packet);
}

ImVec2 TemStreamGui::drawMainMenuBar(const bool connectedToServer)
{
	ImVec2 size;
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", "Ctrl+O", nullptr, !fileDirectory.has_value()))
			{
				fileDirectory.emplace();
			}
			if (ImGui::MenuItem("Exit", "", nullptr))
			{
				TemStream::appDone = true;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::BeginMenu("Font"))
			{
				ImGui::Checkbox("Font Display", &showFont);
				int value = fontSize;
				if (ImGui::SliderInt("Font Size", &value, 12, 96, "%d",
									 ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput))
				{
					fontSize = value;
				}
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					SDL_Event e;
					e.type = SDL_USEREVENT;
					e.user.code = TemStreamEvent::ReloadFont;
					tryPushEvent(e);
				}
				ImGui::SliderInt("Font Type", &fontIndex, 0, io.Fonts->Fonts.size() - 1, "%d",
								 ImGuiSliderFlags_NoInput);
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Displays", "Ctrl+D", nullptr, !showDisplays))
			{
				showDisplays = true;
			}
			if (ImGui::MenuItem("Displays", "Ctrl+D", nullptr, !showStreams))
			{
				showStreams = true;
			}
			if (ImGui::MenuItem("Audio", "Ctrl+A", nullptr, !showAudio))
			{
				showAudio = true;
			}
			if (ImGui::MenuItem("Logs", "Ctrl+L", nullptr, !showLogs))
			{
				showLogs = true;
			}
			if (ImGui::MenuItem("Stats", "Ctrl+I", nullptr, !showStats))
			{
				showStats = true;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("Connect"))
		{
			if (ImGui::MenuItem("Connect to server", "", nullptr, !connectedToServer))
			{
				connectToServer = Address();
			}
			if (ImGui::MenuItem("Disconnect from server", "", nullptr, connectedToServer))
			{
				connectToServer = std::nullopt;
				LOCK(peerMutex);
				peer = nullptr;
			}
			else
			{
				ImGui::Separator();
				if (connectedToServer)
				{
					ImGui::TextColored(Colors::Lime, "Logged in as: %s\n", peerInfo.name.c_str());

					{
						LOCK(peerMutex);
						const auto &info = peer->getInfo();
						ImGui::TextColored(Colors::Yellow, "Server: %s\n", info.name.c_str());

						const auto &addr = peer->getAddress();
						ImGui::TextColored(Colors::Yellow, "Address: %s:%d\n", addr.hostname.c_str(), addr.port);
					}

					ImGui::Separator();
					if (ImGui::MenuItem("Send Data", "Ctrl+P", nullptr, queryData == nullptr))
					{
						queryData = tem_unique<QueryText>(*this);
					}
				}
			}
			ImGui::EndMenu();
		}

		size = ImGui::GetWindowSize();
		ImGui::EndMainMenuBar();
	}
	return size;
}

void TemStreamGui::draw()
{
	const bool connectedToServer = isConnected();

	const auto size = drawMainMenuBar(connectedToServer);

	if (ImGui::Begin("##afdasy4", nullptr,
					 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
						 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
						 ImGuiWindowFlags_NoScrollbar))
	{
		ImColor color;
		const char *text;
		if (connectedToServer)
		{
			color = Colors::Lime;
			text = "Connected";
		}
		else
		{
			color = Colors::Red;
			text = "Disconnected";
		}
		const auto textSize = ImGui::CalcTextSize(text);
		auto draw = ImGui::GetForegroundDrawList();
		const float x = size.x - (textSize.x + size.x * 0.01f);
		const float radius = size.y * 0.5f;
		draw->AddCircleFilled(ImVec2(x - radius, radius), radius, color);
		draw->AddText(ImVec2(x, 0), color, text);
	}
	ImGui::End();

	if (!audio.empty() && showAudio)
	{
		if (ImGui::Begin("Audio", &showAudio, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (ImGui::BeginTable("Audio", 6, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Device Name");
				ImGui::TableSetupColumn("Source/Destination");
				ImGui::TableSetupColumn("Recording");
				ImGui::TableSetupColumn("Volume");
				ImGui::TableSetupColumn("Muted");
				ImGui::TableSetupColumn("Stop");
				ImGui::TableHeadersRow();

				for (auto iter = audio.begin(); iter != audio.end();)
				{
					auto &a = iter->second;
					const bool recording = a->isRecording();

					ImGui::TableNextColumn();
					if (ImGui::Button(a->getName().c_str()))
					{
						audioTarget = iter->first;
					}

					ImGui::TableNextColumn();
					iter->first.print(strBuffer);
					ImGui::Text("%s", strBuffer.data());

					if (recording)
					{
						ImGui::TableNextColumn();
						ImGui::TextColored(Colors::Lime, "Yes");

						ImGui::TableNextColumn();
						ImGui::TextColored(Colors::Yellow, "N\\A");
					}
					else
					{
						ImGui::TableNextColumn();
						ImGui::TextColored(Colors::Red, "No");

						float v = a->getVolume();
						ImGui::TableNextColumn();
						if (ImGui::SliderFloat("Volume", &v, 0.f, 1.f))
						{
							a->setVolume(v);
						}
					}

					ImGui::TableNextColumn();
					const bool isMuted = a->isMuted();
					if (isMuted)
					{
						if (ImGui::SmallButton("Yes"))
						{
							a->setMuted(false);
							a->clearAudio();
						}
					}
					else
					{
						if (ImGui::SmallButton("No"))
						{
							a->setMuted(true);
							a->clearAudio();
						}
					}

					ImGui::TableNextColumn();
					if (ImGui::Button("Stop"))
					{
						iter = audio.erase(iter);
					}
					else
					{
						++iter;
					}
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	if (showFont)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("Font", &showFont))
		{
			std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t, Allocator<char32_t>, Allocator<char>> cvt;
			const String s = cvt.to_bytes(allUTF32);
			const size_t size = 1000;
			for (size_t i = 0; i < s.size(); i += size)
			{
				if (ImGui::BeginChild(1 + (i / size)))
				{
					const String k(s.begin() + i, (i + size > s.size()) ? s.end() : (s.begin() + i + size));
					ImGui::TextWrapped("%s", k.c_str());
				}
				ImGui::EndChild();
			}
		}
		ImGui::End();
	}

	if (connectToServer.has_value())
	{
		bool opened = true;
		if (ImGui::Begin("Connect to server", &opened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::InputText("Hostname", &connectToServer->hostname);
			ImGui::InputInt("Port", &connectToServer->port);
			ImGui::InputText("Name", &peerInfo.name);
			if (ImGui::Button("Connect"))
			{
				connect(*connectToServer);
				opened = false;
			}
		}
		ImGui::End();
		if (!opened)
		{
			connectToServer = std::nullopt;
		}
	}

	if (queryData != nullptr)
	{
		bool opened = true;
		if (ImGui::Begin("Send data to server", &opened,
						 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			static const char *options[]{"Text", "Image", "Audio", "Video"};
			int selected = getSelectedQuery();
			if (ImGui::Combo("Data Type", &selected, options, IM_ARRAYSIZE(options)))
			{
				switch (selected)
				{
				case 0:
					queryData = tem_unique<QueryText>(*this);
					break;
				case 1:
					queryData = tem_unique<QueryImage>(*this);
					break;
				case 2:
					queryData = tem_unique<QueryAudio>(*this);
					break;
				case 3:
					queryData = tem_unique<QueryVideo>(*this);
					break;
				default:
					break;
				}
			}
			if (queryData->draw())
			{
				if (isConnected())
				{
					queryData->execute();
				}
				else
				{
					(*logger)(Logger::Error) << "Cannot send data if not connected to a server" << std::endl;
				}
				opened = false;
			}
		}
		ImGui::End();
		if (!opened)
		{
			queryData = nullptr;
		}
	}

	if (!displays.empty() && showDisplays)
	{
		if (ImGui::Begin("Displays", &showDisplays, ImGuiWindowFlags_AlwaysAutoResize))
		{
			for (auto &pair : displays)
			{
				pair.first.print(strBuffer);
				ImGui::PushID(strBuffer.data());
				ImGui::Text("%s", strBuffer.data());
				pair.second.drawFlagCheckboxes();
				ImGui::PopID();
			}
		}
		ImGui::End();
	}

	if (!streams.empty() && showStreams)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("Streams", &showStreams))
		{
			if (ImGui::BeginTable("Streams", 4, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Author");
				ImGui::TableSetupColumn("Type");
				ImGui::TableSetupColumn("Status");
				ImGui::TableHeadersRow();

				for (const auto &stream : streams)
				{
					const auto &source = stream.first;

					ImGui::TableNextColumn();
					ImGui::Text("%s", source.destination.c_str());

					ImGui::TableNextColumn();
					ImGui::Text("%s", source.author.c_str());

					ImGui::TableNextColumn();
					ImGui::Text("%u", stream.second.getType());

					ImGui::TableNextColumn();
					if (peerInfo.name == source.author)
					{
						ImGui::Text("Owner");
					}
					else if (subscriptions.find(source) == subscriptions.end())
					{
						if (ImGui::Button("Unsubscribed"))
						{
							Message::Packet packet;
							packet.source.author = peerInfo.name;
							Message::StreamUpdate su;
							su.action = Message::StreamUpdate::Subscribe;
							su.source = source;
							su.type = stream.second.getType();
							packet.payload.emplace<Message::StreamUpdate>(std::move(su));
							sendPacket(std::move(packet), false);
						}
					}
					else
					{
						if (ImGui::Button("Subscribe"))
						{
							Message::Packet packet;
							packet.source.author = peerInfo.name;
							Message::StreamUpdate su;
							su.action = Message::StreamUpdate::Unsubscribe;
							su.source = source;
							su.type = stream.second.getType();
							packet.payload.emplace<Message::StreamUpdate>(std::move(su));
							sendPacket(std::move(packet), false);
						}
					}
				}

				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	if (showLogs)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("Logs", &showLogs))
		{
			InMemoryLogger &mLogger = static_cast<InMemoryLogger &>(*logger);
			mLogger.viewLogs([](const Logger::Log &log) {
				switch (log.first)
				{
				case Logger::Trace:
					ImGui::TextColored(Colors::Cyan, "%s", log.second.c_str());
					break;
				case Logger::Info:
					ImGui::TextColored(Colors::White, "%s", log.second.c_str());
					break;
				case Logger::Warning:
					ImGui::TextColored(Colors::Yellow, "%s", log.second.c_str());
					break;
				case Logger::Error:
					ImGui::TextColored(Colors::Red, "%s", log.second.c_str());
					break;
				default:
					break;
				}
			});
		}
		ImGui::End();
	}

	if (showStats)
	{
		if (ImGui::Begin("Stats", &showStats, ImGuiWindowFlags_AlwaysAutoResize))
		{
			SDL_version v;
			SDL_GetVersion(&v);
			ImGui::Text("TemStream %u.%u.%u", TemStream_VERSION_MAJOR, TemStream_VERSION_MINOR,
						TemStream_VERSION_PATCH);
			ImGui::Text("SDL %u.%u.%u", v.major, v.minor, v.patch);
			ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
			if (ImGui::CollapsingHeader("Memory"))
			{
				auto total = globalAllocatorData.getTotal();
				auto used = globalAllocatorData.getUsed();
				const char *type = "bytes";
				if (used >= MB(1))
				{
					total /= MB(1);
					used /= MB(1);
					type = "MB";
				}
				else if (used >= KB(1))
				{
					total /= KB(1);
					used /= KB(1);
					type = "KB";
				}
				const float percent = (float)used / (float)total;
				ImGui::ProgressBar(percent);
				ImGui::Text("%zu / %zu %s", used, total, type);
			}
		}
		ImGui::End();
	}

	if (fileDirectory.has_value())
	{
		bool opened = true;
		SetWindowMinSize(window);
		if (ImGui::Begin("Open a file", &opened))
		{
			if (ImGui::Button("^"))
			{
				fs::path path(fileDirectory->getDirectory());
				fileDirectory.emplace(path.parent_path().c_str());
			}
			ImGui::SameLine();

			String dir = fileDirectory->getDirectory();
			if (ImGui::InputText("Directory", &dir))
			{
				if (fs::is_directory(dir))
				{
					fileDirectory.emplace(dir);
				}
			}

			std::optional<String> newDir;
			if (ImGui::BeginTable("", 2, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Type");
				ImGui::TableHeadersRow();

				for (const auto &file : fileDirectory->getFiles())
				{
					if (fs::is_directory(file))
					{
						ImGui::TableNextColumn();
						if (ImGui::Selectable(file.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
							ImGui::IsMouseDoubleClicked(0))
						{
							newDir.emplace(file);
						}
						ImGui::TableNextColumn();
						ImGui::TextColored(Colors::Yellow, "Directory");
					}
					else if (fs::is_regular_file(file))
					{
						ImGui::TableNextColumn();
						if (ImGui::Selectable(file.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
							ImGui::IsMouseDoubleClicked(0))
						{
							SDL_Event e;
							e.type = SDL_DROPFILE;
							e.drop.timestamp = SDL_GetTicks();
							e.drop.windowID = SDL_GetWindowID(window);
							e.drop.file = reinterpret_cast<char *>(SDL_malloc(file.size() + 1));
							strcpy(e.drop.file, file.c_str());
							if (!tryPushEvent(e))
							{
								SDL_free(e.drop.file);
							}
							opened = false;
						}
						ImGui::TableNextColumn();
						ImGui::Text("Text");
					}
				}
				ImGui::EndTable();
			}
			if (newDir.has_value())
			{
				fileDirectory.emplace(*newDir);
			}
		}
		ImGui::End();
		if (!opened)
		{
			fileDirectory = std::nullopt;
		}
	}

	if (audioTarget.has_value())
	{
		auto iter = audio.find(*audioTarget);
		if (iter != audio.end())
		{
			auto &a = iter->second;
			const bool recording = a->isRecording();
			std::optional<const char *> opt = std::nullopt;
			bool opened = true;
			const int offset = snprintf(strBuffer.data(), strBuffer.size(), "Select audio device for ");
			audioTarget->print(strBuffer, offset);
			if (ImGui::Begin(strBuffer.data(), &opened, ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (ImGui::Button("Default audio device"))
				{
					audioTarget = std::nullopt;
					opt = std::make_optional<const char *>(nullptr);
					goto checkResult;
				}
				{
					const int count = SDL_GetNumAudioDevices(recording);
					for (int i = 0; i < count; ++i)
					{
						auto name = SDL_GetAudioDeviceName(i, recording);
						if (ImGui::Button(name))
						{
							audioTarget = std::nullopt;
							opt = name;
							goto checkResult;
						}
					}
				}
			}
		checkResult:
			ImGui::End();
			if (!opened)
			{
				audioTarget = std::nullopt;
			}
			if (opt.has_value())
			{
				const char *name = *opt;
				shared_ptr<Audio> newAudio = nullptr;
				if (recording)
				{
					newAudio = Audio::startRecording(a->getSource(), name);
				}
				else
				{
					newAudio = Audio::startPlayback(a->getSource(), name);
				}
				if (newAudio != nullptr)
				{
					a = std::move(newAudio);
				}
			}
		}
	}
}

int TemStreamGui::getSelectedQuery() const
{
	if (queryData == nullptr)
	{
		return -1;
	}
	if (dynamic_cast<QueryText *>(queryData.get()) != nullptr)
	{
		return 0;
	}
	if (dynamic_cast<QueryImage *>(queryData.get()) != nullptr)
	{
		return 1;
	}
	if (dynamic_cast<QueryAudio *>(queryData.get()) != nullptr)
	{
		return 2;
	}
	if (dynamic_cast<QueryVideo *>(queryData.get()) != nullptr)
	{
		return 3;
	}
	return -1;
}

const char *getExtension(const char *filename)
{
	size_t len = strlen(filename);
	const char *c = filename + (len - 1);
	for (; *c != *filename; --c)
	{
		if (*c == '.')
		{
			return c + 1;
		}
	}
	return filename;
}

bool TemStreamGui::createStreamIfNeeded(const Message::Packet &packet)
{
	// Assume peer has already been checked

	// If stream not found, send create stream packet
	if (streams.find(packet.source) == streams.end())
	{
		Message::StreamUpdate su;
		su.source = packet.source;
		su.action = Message::StreamUpdate::Create;
		su.type = static_cast<uint32_t>(packet.payload.index());
		(*logger)(Logger::Trace) << "Creating stream " << su << std::endl;

		Message::Packet newPacket;
		newPacket.source = packet.source;
		newPacket.payload.emplace<Message::StreamUpdate>(std::move(su));
		return (*peer)->sendPacket(newPacket);
	}
	return true;
}

void TemStreamGui::sendPacket(Message::Packet &&packet, const bool handleLocally)
{
	LOCK(peerMutex);
	if (peer == nullptr)
	{
		(*logger)(Logger::Error) << "Cannot send data when not conncted to the server" << std::endl;
		return;
	}

	if (!createStreamIfNeeded(packet) || !(*peer)->sendPacket(packet))
	{
		(*logger)(Logger::Trace) << "TemStreamGui::sendPacket: error" << std::endl;
		onDisconnect(peer->gotServerInformation());
		peer = nullptr;
		return;
	}
	if (handleLocally)
	{
		peer->addPacket(std::move(packet));
	}
}

void TemStreamGui::sendPackets(MessagePackets &&packets, const bool handleLocally)
{
	LOCK(peerMutex);
	if (peer == nullptr)
	{
		(*logger)(Logger::Error) << "Cannot send data when not conncted to the server" << std::endl;
		return;
	}
	for (const auto &packet : packets)
	{
		if (!createStreamIfNeeded(packet) || !(*peer)->sendPacket(packet))
		{
			(*logger)(Logger::Trace) << "TemStreamGui::sendPackets: error" << std::endl;
			onDisconnect(peer->gotServerInformation());
			peer = nullptr;
			return;
		}
	}
	if (handleLocally)
	{
		peer->addPackets(std::move(packets));
	}
}

bool TemStreamGui::addAudio(shared_ptr<Audio> &&ptr)
{
	auto pair = audio.emplace(ptr->getSource(), std::move(ptr));
	return pair.second;
}

shared_ptr<Audio> TemStreamGui::getAudio(const Message::Source &source) const
{
	auto iter = audio.find(source);
	if (iter == audio.end())
	{
		return nullptr;
	}

	return iter->second;
}

bool TemStreamGui::isConnected()
{
	LOCK(peerMutex);
	return peer != nullptr;
}

void TemStreamGui::LoadFonts()
{
	io.Fonts->Clear();
	static ImWchar ranges[] = {MinCharacters, MaxCharacters, 0};
	static ImFontConfig cfg;
	cfg.OversampleH = cfg.OversampleV = 1;
	cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
	for (size_t i = 0; i < IM_ARRAYSIZE(Fonts); ++i)
	{
		io.Fonts->AddFontFromMemoryCompressedTTF(Fonts[i], FontSizes[i], fontSize, &cfg, ranges);
	}
	for (const auto &file : fontFiles)
	{
		io.Fonts->AddFontFromFileTTF(file.c_str(), fontSize);
	}
	io.Fonts->Build();
	ImGui_ImplSDLRenderer_DestroyFontsTexture();
}

void TemStreamGui::handleMessage(Message::Packet &&m)
{
	auto iter = displays.find(m.source);
	if (iter == displays.end())
	{
		StreamDisplay display(*this, m.source);
		auto pair = displays.emplace(m.source, std::move(display));
		if (!pair.second)
		{
			LOCK(peerMutex);
			onDisconnect(peer->gotServerInformation());
			(*logger)(Logger::Trace) << "TemStreamGui::handleMessage: error" << std::endl;
			peer = nullptr;
			return;
		}
		iter = pair.first;
	}

	if (!std::visit(iter->second, m.payload))
	{
		displays.erase(iter);
	}
}

String32 TemStreamGui::getAllUTF32()
{
	String32 s;
	for (ImWchar c = MinCharacters; c < MaxCharacters; ++c)
	{
		s += c;
	}
	return s;
}

void guiSignalHandler(int s)
{
	switch (s)
	{
	case SIGPIPE:
		(*logger)(Logger::Error) << "Broken pipe error occurred" << std::endl;
		break;
	default:
		break;
	}
}

int TemStreamGui::run()
{
	{
		struct sigaction action;
		action.sa_handler = &guiSignalHandler;
		sigfillset(&action.sa_mask);
		if (sigaction(SIGPIPE, &action, nullptr) == -1)
		{
			perror("sigaction");
			return EXIT_FAILURE;
		}
	}

	IMGUI_CHECKVERSION();
#if USE_CUSTOM_ALLOCATOR
	ImGui::SetAllocatorFunctions((ImGuiMemAllocFunc)allocatorImGuiAlloc, (ImGuiMemFreeFunc)allocatorImGuiFree, nullptr);
#endif

	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();

	ImGui::StyleColorsDark();

	TemStreamGui gui(io);
	logger = tem_unique<TemStreamGuiLogger>(gui);
	initialLogs();
	(*logger)(Logger::Info) << "Dear ImGui v" << ImGui::GetVersion() << std::endl;

	if (!gui.init())
	{
		return EXIT_FAILURE;
	}

	ImGui_ImplSDL2_InitForSDLRenderer(gui.window, gui.renderer);
	ImGui_ImplSDLRenderer_Init(gui.renderer);

	gui.LoadFonts();

	while (!appDone)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.type)
			{
			case SDL_QUIT:
				appDone = true;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(gui.window))
				{
					appDone = true;
				}
				break;
			case SDL_DROPTEXT: {
				const size_t size = strlen(event.drop.file);
				String s(event.drop.file, event.drop.file + size);
				gui.queryData = tem_unique<QueryText>(gui, std::move(s));
				SDL_free(event.drop.file);
			}
			break;
			case SDL_DROPFILE: {
				if (isTTF(event.drop.file))
				{
					*logger << "Adding new font: " << event.drop.file << std::endl;
					gui.fontFiles.emplace_back(event.drop.file);
					gui.fontIndex = IM_ARRAYSIZE(Fonts) + gui.fontFiles.size() - 1;
					gui.LoadFonts();
				}
				else
				{
					Work::CheckFile cf(String(event.drop.file), gui);
					(*gui).addWork(std::move(cf));
				}
				SDL_free(event.drop.file);
			}
			break;
			case SDL_USEREVENT:
				switch (event.user.code)
				{
				case TemStreamEvent::SendSingleMessagePacket: {
					Message::Packet *packet = reinterpret_cast<Message::Packet *>(event.user.data1);
					gui.sendPacket(std::move(*packet));
					deallocate(packet);
				}
				break;
				case TemStreamEvent::HandleMessagePacket: {
					Message::Packet *packet = reinterpret_cast<Message::Packet *>(event.user.data1);
					gui.handleMessage(std::move(*packet));
					deallocate(packet);
				}
				break;
				case TemStreamEvent::SendMessagePackets: {
					MessagePackets *packets = reinterpret_cast<MessagePackets *>(event.user.data1);
					gui.sendPackets(std::move(*packets));
					deallocate(packets);
				}
				break;
				case TemStreamEvent::HandleMessagePackets: {
					MessagePackets *packets = reinterpret_cast<MessagePackets *>(event.user.data1);
					const auto start = std::make_move_iterator(packets->begin());
					const auto end = std::make_move_iterator(packets->end());
					std::for_each(start, end,
								  [&gui](Message::Packet &&packet) { gui.handleMessage(std::move(packet)); });
					deallocate(packets);
				}
				break;
				case TemStreamEvent::ReloadFont:
					gui.LoadFonts();
					break;
				case TemStreamEvent::SetQueryData: {
					IQuery *query = reinterpret_cast<IQuery *>(event.user.data1);
					auto ptr = unique_ptr<IQuery>(query);
					gui.queryData.swap(ptr);
				}
				break;
				case TemStreamEvent::SetSurfaceToStreamDisplay: {
					SDL_Surface *surface = reinterpret_cast<SDL_Surface *>(event.user.data1);
					Message::Source *source = reinterpret_cast<Message::Source *>(event.user.data2);

					auto iter = gui.displays.find(*source);
					if (iter != gui.displays.end())
					{
						iter->second.setSurface(surface);
					}
					SDL_FreeSurface(surface);
					deallocate(source);
				}
				break;
				case TemStreamEvent::AddAudio: {
					Audio *audio = reinterpret_cast<Audio *>(event.user.data1);
					auto ptr = shared_ptr<Audio>(audio, Deleter<Audio>());
					gui.addAudio(std::move(ptr));
				}
				break;
				default:
					break;
				}
				break;
			case SDL_KEYDOWN:
				if (io.WantCaptureKeyboard)
				{
					break;
				}
				if ((event.key.keysym.mod & KMOD_CTRL) == 0)
				{
					break;
				}
				switch (event.key.keysym.sym)
				{
				case SDLK_o:
					if (!gui.fileDirectory.has_value())
					{
						gui.fileDirectory.emplace();
					}
					break;
				case SDLK_a:
					gui.showAudio = !gui.showAudio;
					break;
				case SDLK_d:
					gui.showDisplays = !gui.showDisplays;
					break;
				case SDLK_w:
					gui.showStreams = !gui.showStreams;
					break;
				case SDLK_l:
					gui.showLogs = !gui.showLogs;
					break;
				case SDLK_i:
					gui.showStats = !gui.showStats;
					break;
				case SDLK_p:
					if (gui.queryData == nullptr)
					{
						gui.queryData = tem_unique<QueryText>(gui);
					}
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		gui.pushFont();

		gui.draw();

		for (auto iter = gui.displays.begin(); iter != gui.displays.end();)
		{
			if (iter->second.draw())
			{
				++iter;
			}
			else
			{
				iter = gui.displays.erase(iter);
			}
		}

		ImGui::PopFont();

		ImGui::Render();

		SDL_SetRenderDrawColor(gui.renderer, 128u, 128u, 128u, 255u);
		SDL_RenderClear(gui.renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(gui.renderer);
	}

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	while (runningThreads > 0)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);
	}
	logger = nullptr;
	return EXIT_SUCCESS;
}
TemStreamGuiLogger::TemStreamGuiLogger(TemStreamGui &gui) : gui(gui)
{
}
TemStreamGuiLogger::~TemStreamGuiLogger()
{
	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "TemStream_log_%zu.txt", time(NULL));
	std::ofstream file(buffer);
	this->viewLogs([&file](const Log &log) { file << log.first << ": " << log.second; });
}
void TemStreamGuiLogger::Add(const Level level, const String &s, const bool b)
{
	checkError(level);
	InMemoryLogger::Add(level, s, b);
}
void TemStreamGuiLogger::checkError(const Level level)
{
	if (level == Level::Error && !gui.isShowingLogs())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "An error has occurred. Check logs for more detail",
								 gui.window);
		gui.setShowLogs(true);
	}
}
FileDisplay::FileDisplay() : directory(fs::current_path().c_str()), files()
{
	loadFiles();
}
FileDisplay::FileDisplay(const String &s) : directory(s), files()
{
	loadFiles();
}
FileDisplay::~FileDisplay()
{
}
void FileDisplay::loadFiles()
{
	try
	{
		for (auto file : fs::directory_iterator(directory))
		{
			files.emplace_back(file.path().c_str());
		}
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << e.what() << std::endl;
		files.clear();
	}
}
SDL_MemoryFunctions::SDL_MemoryFunctions()
	: mallocFunc(nullptr), callocFunc(nullptr), reallocFunc(nullptr), freeFunc(nullptr)
{
}
void SDL_MemoryFunctions::GetFromSDL()
{
	SDL_GetMemoryFunctions(&mallocFunc, &callocFunc, &reallocFunc, &freeFunc);
}
void SDL_MemoryFunctions::SetToSDL() const
{
	SDL_SetMemoryFunctions(mallocFunc, callocFunc, reallocFunc, freeFunc);
}
} // namespace TemStream
