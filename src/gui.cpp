#include <main.hpp>

#include "colors.hpp"

#include "fonts/Cousine.cpp"
#include "fonts/DroidSans.cpp"
#include "fonts/Karla.cpp"
#include "fonts/NotoEmoji.cpp"
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

bool colorIsLight(const ImVec4 &bg)
{
	return bg.x + bg.y + bg.z > 1.5f;
}

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
	return a.reallocate(old, newSize);
}

namespace TemStream
{

const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp;
void handleWorkThread(TemStreamGui *gui);

TemStreamGui::TemStreamGui(ImGuiIO &io, Configuration &c)
	: strBuffer(), audio(connectionMutex), video(connectionMutex), connections(connectionMutex), queryData(nullptr),
	  allUTF32(getAllUTF32()), lastVideoCheck(std::chrono::system_clock::now()), io(io), configuration(c),
	  window(nullptr), renderer(nullptr)
{
}

TemStreamGui::~TemStreamGui()
{
	SDL_DestroyRenderer(renderer);
	renderer = nullptr;
	SDL_DestroyWindow(window);
	window = nullptr;
	IMG_Quit();
	SDL_Quit();
}

void TemStreamGui::decodeVideoPackets()
{
	using namespace std::chrono_literals;

	const auto now = std::chrono::system_clock::now();
	if (now - lastVideoCheck > 1s)
	{
		for (auto iter = decodingMap.begin(); iter != decodingMap.end();)
		{
			if (displays.find(iter->first) == displays.end())
			{
				(*logger)(Logger::Trace) << "Removed " << iter->first << " from decoding map" << std::endl;
				iter = decodingMap.erase(iter);
			}
			else
			{
				++iter;
			}
		}
		lastVideoCheck = now;
	}
	// This needs to be big enough to handle video files.
	if (auto result = videoPackets.clearIfGreaterThan(1000))
	{
		(*logger)(Logger::Warning) << "Dropping " << *result << " received video frames" << std::endl;
	}

	auto result = videoPackets.pop(0ms);
	if (!result)
	{
		return;
	}

	struct DecodePacket
	{
		const Message::Source &source;
		TemStreamGui &gui;
		void operator()(Message::Frame &packet)
		{
			auto &decodingMap = gui.decodingMap;
			auto iter = decodingMap.find(source);
			if (iter == decodingMap.end())
			{
				auto decoder = VideoSource::createDecoder();
				if (!decoder)
				{
					return;
				}
				decoder->setWidth(packet.width);
				decoder->setHeight(packet.height);
				auto pair = decodingMap.try_emplace(source, std::move(decoder));
				if (!pair.second)
				{
					return;
				}
				iter = pair.first;
				gui.lastVideoCheck = std::chrono::system_clock::now();
				(*logger)(Logger::Trace) << "Added " << iter->first << " to decoding map" << std::endl;
			}
			auto &decoder = iter->second;
			// if (decoder->getHeight() != packet.height || decoder->getWidth() != packet.width)
			// {
			// 	auto newDecoder = VideoSource::createDecoder();
			// 	if (!newDecoder)
			// 	{
			// 		return;
			// 	}
			// 	decoder->setWidth(packet.width);
			// 	decoder->setHeight(packet.height);
			// 	decoder.swap(newDecoder);
			// }

			if (!decoder->decode(packet.bytes))
			{
				return;
			}

			SDL_Event e;
			e.type = SDL_USEREVENT;
			e.user.code = TemStreamEvent::HandleFrame;

			auto frame = allocateAndConstruct<VideoSource::Frame>();
			frame->bytes = std::move(packet.bytes);
			frame->width = decoder->getWidth();
			frame->height = decoder->getHeight();
			frame->format = SDL_PIXELFORMAT_IYUV;
			e.user.data1 = frame;

			Message::Source *newSource = allocateAndConstruct<Message::Source>(source);
			e.user.data2 = newSource;
			if (!tryPushEvent(e))
			{
				destroyAndDeallocate(frame);
				destroyAndDeallocate(newSource);
			}
		}
		void operator()(Message::LargeFile &lf)
		{
			std::visit(*this, lf);
		}
		void operator()(const uint64_t fileSize)
		{
			auto iter = gui.pendingVideo.find(source);
			if (iter == gui.pendingVideo.end())
			{
				auto [rIter, inserted] = gui.pendingVideo.try_emplace(source);
				if (!inserted)
				{
					return;
				}
				iter = rIter;
			}
			iter->second.clear();
			iter->second.reallocate(fileSize);
		}
		void operator()(const ByteList &bytes)
		{
			auto iter = gui.pendingVideo.find(source);
			if (iter == gui.pendingVideo.end())
			{
				return;
			}
			iter->second.append(bytes);
		}
		void operator()(std::monostate)
		{
			auto iter = gui.pendingVideo.find(source);
			if (iter == gui.pendingVideo.end())
			{
				return;
			}

			const ByteList &videoFile = iter->second;
			(*logger)(Logger::Trace) << "Got video file: " << printMemory(videoFile.size()) << std::endl;
			StringStream ss;
			ss << source << "_temp" << VideoExtension;
			cv::String filename(ss.str());
			{
				std::ofstream file(filename, std::ios::binary | std::ios::out);
				std::copy(videoFile.begin(), videoFile.end(), std::ostreambuf_iterator<char>(file));
			}
			auto cap = tem_shared<cv::VideoCapture>(filename);
			if (!cap->isOpened())
			{
				(*logger)(Logger::Error) << "Failed to open video file from server" << std::endl;
				return;
			}

			auto nextFrame = tem_shared<TimePoint>(std::chrono::system_clock::now());

			WorkPool::workPool.addWork(
				[nextFrame = std::move(nextFrame), cap = std::move(cap), source = source]() mutable {
					if (!cap->isOpened())
					{
						return false;
					}
					const auto now = std::chrono::system_clock::now();
					if (now < *nextFrame)
					{
						return true;
					}
					cv::Mat image;
					if (!cap->read(image) || image.empty())
					{
						return false;
					}

					SDL_Event e;
					e.type = SDL_USEREVENT;
					e.user.code = TemStreamEvent::HandleFrame;

					auto frame = allocateAndConstruct<VideoSource::Frame>();
					frame->width = static_cast<uint32_t>(cap->get(cv::CAP_PROP_FRAME_WIDTH));
					frame->height = static_cast<uint32_t>(cap->get(cv::CAP_PROP_FRAME_HEIGHT));
					frame->bytes = ByteList(image.data, image.total() * image.elemSize());
					frame->format = SDL_PIXELFORMAT_BGR24;
					e.user.data1 = frame;

					auto sourcePtr = allocateAndConstruct<Message::Source>(source);
					e.user.data2 = sourcePtr;

					if (!tryPushEvent(e))
					{
						destroyAndDeallocate(frame);
						destroyAndDeallocate(sourcePtr);
					}

					const auto delay = std::chrono::duration<double, std::milli>(1000.0 / cap->get(cv::CAP_PROP_FPS));
					*nextFrame = now + delay;
					return true;
				});
		}
	};

	auto &[source, packet] = *result;
	std::visit(DecodePacket{source, *this}, packet);
}

void TemStreamGui::pushFont()
{
	if (configuration.fontIndex >= io.Fonts->Fonts.size())
	{
		configuration.fontIndex = 0;
	}
	ImGui::PushFont(io.Fonts->Fonts[configuration.fontIndex]);
}

bool TemStreamGui::init()
{
#if TEMSTREAM_USE_CUSTOM_ALLOCATOR
	SDL_MemoryFunctions memFuncs;
	memFuncs.GetFromSDL();
	SDL_SetMemoryFunctions((SDL_malloc_func)allocatorMalloc, (SDL_calloc_func)allocatorCalloc,
						   (SDL_realloc_func)allocatorReallocate, (SDL_free_func)allocatorFree);
#endif

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
	{
		logSDLError("SDL error");
		return false;
	}

#if TEMSTREAM_USE_OPENCV
	const int flags = IMG_INIT_PNG;
#else
	const int flags = IMG_INIT_JPG | IMG_INIT_PNG;
#endif
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

	WorkPool::workPool.addWork([this]() {
		this->decodeVideoPackets();
		return true;
	});

	WorkPool::workPool.addWork([this]() {
		audio.removeIfNot([this](const auto &source, const auto &a) {
			if (auto con = this->getConnection(source))
			{
				a->encodeAndSendAudio(*con);
				return true;
			}
			else
			{
				return false;
			}
		});
		return true;
	});

	return true;
}

void TemStreamGui::connect(const Address &address)
{
	*logger << "Connecting to server: " << address << std::endl;
	WorkPool::workPool.addWork([address = address, this]() {
		auto s = TcpSocket::create(address);
		if (s == nullptr)
		{
			(*logger)(Logger::Error) << "Failed to connect to server: " << address << std::endl;
			return false;
		}

		*logger << "Connected to server: " << address << std::endl;

		auto clientConnection = tem_shared<ClientConnection>(*this, address, std::move(s));
		{
			Message::Packet packet;
			packet.payload.emplace<Message::Credentials>(configuration.credentials);
			if (!clientConnection->sendPacket(packet, true))
			{
				(*logger)(Logger::Error) << "Authentication failure for server: " << address
										 << "; Failed to send message" << std::endl;
			}
		}
		// Wait for response to get server type
		auto &packets = clientConnection->getPackets();
		while (true)
		{
			if (!clientConnection->readAndHandle(3000))
			{
				(*logger)(Logger::Error) << "Authentication failure for server: " << address << "; No repsonse"
										 << std::endl;
				return false;
			}
			if (packets.size() > 0)
			{
				break;
			}
		}

		using namespace std::chrono_literals;
		{
			auto packet = packets.pop(1s);
			if (!packet)
			{
				// This shouldn't ever happen
				(*logger)(Logger::Error) << "Authentication failure for server: " << address
										 << "; Internal error: " << std::endl;
				return false;
			}

			if (auto ptr = std::get_if<Message::VerifyLogin>(&packet->payload))
			{
				clientConnection->setVerifyLogin(std::move(*ptr));
			}
			else
			{
				(*logger)(Logger::Error) << "Authentication failure for server: " << address
										 << "; Bad message: " << packet->payload.index() << std::endl;
				return false;
			}
		}

		*logger << "Server information: " << clientConnection->getInfo() << std::endl;
		if (this->addConnection(clientConnection))
		{
			(*logger)(Logger::Trace) << "Adding connection to list: " << clientConnection->getInfo() << std::endl;
			WorkPool::workPool.addWork([this, clientConnection]() {
				if (TemStreamGui::handleClientConnection(*clientConnection))
				{
					return true;
				}
				else
				{
					clientConnection->close();
					this->dirty = true;
					return false;
				}
			});
		}
		else
		{
			(*logger)(Logger::Error) << "Failed to add connection" << clientConnection->getInfo() << std::endl;
		}
		return false;
	});
}

bool TemStreamGui::handleClientConnection(ClientConnection &con)
{
	return con.isOpened() && con.readAndHandle(0) && con.flushPackets() && con->flush();
}

bool TemStreamGui::addConnection(const shared_ptr<ClientConnection> &connection)
{
	return connections.add(connection->getSource(), connection);
}

shared_ptr<ClientConnection> TemStreamGui::getConnection(const Message::Source &source)
{
	auto ptr = connections.find(source, nullptr);
	if (ptr == nullptr)
	{
		return nullptr;
	}
	if (ptr->isOpened())
	{
		return ptr;
	}
	else
	{
		connections.remove(source);
		return nullptr;
	}
}

String TemStreamGui::getUsername(const Message::Source &source)
{
	if (auto ptr = getConnection(source))
	{
		return ptr->getInfo().peerInformation.name;
	}
	return String();
}

bool TemStreamGui::hasConnection(const Message::Source &source)
{
	return getConnection(source) != nullptr;
}

size_t TemStreamGui::getConnectionCount()
{
	size_t count = 0;
	connections.removeIfNot([&count](const auto &, const auto &con) {
		if (con->isOpened())
		{
			++count;
			return true;
		}
		else
		{
			return false;
		}
	});
	return count;
}

void TemStreamGui::removeConnection(const Message::Source &source)
{
	connections.remove(source);
}

ImVec2 TemStreamGui::drawMainMenuBar()
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
				ImGui::Checkbox("Font Display", &configuration.showFont);
				int value = configuration.fontSize;
				constexpr int minFontSize = 12;
				constexpr int maxFontSize = 96;
				if (ImGui::InputInt("Font Size", &value) && minFontSize <= value && value <= maxFontSize)
				{
					configuration.fontSize = std::clamp(value, minFontSize, maxFontSize);
					SDL_Event e;
					e.type = SDL_USEREVENT;
					e.user.code = TemStreamEvent::ReloadFont;
					tryPushEvent(e);
				}
				if (ImGui::InputInt("Font Type", &configuration.fontIndex))
				{
					configuration.fontIndex = std::clamp(configuration.fontIndex, 0, io.Fonts->Fonts.size() - 1);
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Displays", "Ctrl+D", nullptr, !configuration.showDisplays))
			{
				configuration.showDisplays = true;
			}
			if (ImGui::MenuItem("Connections", "Ctrl+W", nullptr, !configuration.showConnections))
			{
				configuration.showConnections = true;
			}
			if (ImGui::MenuItem("Audio", "Ctrl+A", nullptr, !configuration.showAudio))
			{
				configuration.showAudio = true;
			}
			if (ImGui::MenuItem("Video", "Ctrl+H", nullptr, !configuration.showVideo))
			{
				configuration.showVideo = true;
			}
			if (ImGui::MenuItem("Logs", "Ctrl+L", nullptr, !configuration.showLogs))
			{
				configuration.showLogs = true;
			}
			if (ImGui::MenuItem("Stats", "Ctrl+I", nullptr, !configuration.showStats))
			{
				configuration.showStats = true;
			}
			if (ImGui::MenuItem("Style", "Ctrl+T", nullptr, !configuration.showColors))
			{
				configuration.showColors = true;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("Credentials"))
		{
			static const char *Options[]{"Token", "Username And Password"};
			int selected = configuration.credentials.index();
			if (ImGui::Combo("Credential Type", &selected, Options, IM_ARRAYSIZE(Options)))
			{
				switch (selected)
				{
				case variant_index<Message::Credentials, String>():
					configuration.credentials.emplace<String>("token12345");
					break;
				case variant_index<Message::Credentials, Message::UsernameAndPassword>():
					configuration.credentials.emplace<Message::UsernameAndPassword>("User", "Password");
					break;
				default:
					break;
				}
			}
			std::visit(RenderCredentials(), configuration.credentials);
			ImGui::EndMenu();
		}
		ImGui::Separator();

		if (ImGui::BeginMenu("Memory"))
		{
			static int m = 256;
			if (ImGui::InputInt("Change Memory (in MB)", &m, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				// .flags, .buttonid, .text
				const SDL_MessageBoxButtonData buttons[] = {
					{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"},
					{0, 1, "Yes"},
				};
				SDL_MessageBoxData data{};
				data.flags = SDL_MESSAGEBOX_INFORMATION;
				data.window = window;
				data.title = "Do you want to reset the app?";
				data.message = "Changing the memory size requires reseting the application. Do you want to reset?";
				data.buttons = buttons;
				data.numbuttons = SDL_arraysize(buttons);
				int i;
				if (SDL_ShowMessageBox(&data, &i) == 0 && i == 1)
				{
					appDone = true;
					const int pid = fork();
					if (pid < 0)
					{
						perror("fork");
					}
					else if (pid == 0)
					{
						const auto s = std::to_string(m);
						i = execl(ApplicationPath, ApplicationPath, "--memory", s.c_str(), NULL);
						(*logger)(Logger::Error) << "Failed to reset the application: " << i << std::endl;
					}
				}
				else
				{
					logSDLError("Failed to show message box");
				}
			}
			ImGui::EndMenu();
		}

		size = ImGui::GetWindowSize();
		ImGui::EndMainMenuBar();
	}
	return size;
}

void TemStreamGui::renderConnection(const Message::Source &source, shared_ptr<ClientConnection> &ptr)
{
	auto &con = *ptr;

	PushedID id(static_cast<String>(source).c_str());

	bool closed = true;
	if (ImGui::BeginPopupModal("Moderation", &closed))
	{
		const auto &info = con.getServerInformation();
		ImGui::Text("Peers: %zu", info.peers.size());
		if (ImGui::Button("Refresh"))
		{
			Message::Packet packet;
			packet.source = source;
			packet.payload.emplace<Message::RequestServerInformation>();
			con.sendPacket(packet, true);
		}
		if (ImGui::BeginChild("Peer List"))
		{
			if (ImGui::BeginTable("Peer Table", 2, TableFlags))
			{
				ImGui::TableSetupColumn("Name [Permissions]");
				ImGui::TableSetupColumn("Ban");
				ImGui::TableHeadersRow();

				for (const auto &peer : info.peers)
				{
					String s;
					{
						StringStream ss;
						ss << peer;
						s = std::move(ss.str());
					}
					const PushedID id(s.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%s", s.c_str());

					ImGui::TableNextColumn();
					if (ImGui::Button("Ban User"))
					{
						Message::Packet packet;
						packet.source = source;
						Message::BanUser banUser;
						banUser.name = peer.name;
						packet.payload.emplace<Message::BanUser>(std::move(banUser));
						con.sendPacket(packet, true);
					}
				}
				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
		ImGui::EndPopup();
	}

	ImGui::TableNextColumn();
	ImGui::Text("%s", source.serverName.c_str());

	ImGui::TableNextColumn();
	const auto &info = con.getInfo();
	{
		StringStream ss;
		ss << info.serverType;
		ImGui::Text("%s", ss.str().c_str());
	}

	int selected;
	{
		auto iter = actionSelections.find(source);
		if (iter == actionSelections.end())
		{
			auto pair = actionSelections.try_emplace(source, 0);
			iter = pair.first;
		}
		selected = iter->second;
	}

	static const char *actions[]{"-------", "Upload", "Disconnect", "Replay", "Moderate"};

	ImGui::TableNextColumn();
	if (ImGui::Combo("Action", &selected, actions, IM_ARRAYSIZE(actions)))
	{
		switch (selected)
		{
		case 1:
			if (!info.peerInformation.hasWriteAccess())
			{
				selected = 0;
			}
			break;
		case 3:
			if (!info.peerInformation.hasReplayAccess())
			{
				selected = 0;
			}
			break;
		case 4:
			if (!info.peerInformation.isModerator())
			{
				selected = 0;
			}
			break;
		default:
			break;
		}
	}

	ImGui::TableNextColumn();
	switch (selected)
	{
	case 1: {
		if (!info.peerInformation.hasWriteAccess())
		{
			ImGui::Text("?");
			selected = 0;
			break;
		}
		const auto dur = con.nextSendInterval();
		if (dur.has_value())
		{
			ImGui::Text("Wait %0.2f", dur->count());
			break;
		}

		bool wrote = false;
		switch (info.serverType)
		{
		case ServerType::Audio:
			if (audio.use(source, [](auto &a) {
					ImGui::TextWrapped("%s\n%s", a->isRecording() ? "Sending" : "Receiving", a->getName().c_str());
				}))
			{
				wrote = true;
			}
			break;
		case ServerType::Video: {
			if (video.use(source, [](auto &v) { ImGui::TextWrapped("Sending\n%s", v->getName().c_str()); }))
			{
				wrote = true;
			}
		}
		break;
		default:
			break;
		}
		if (!wrote && ImGui::Button("Upload"))
		{
			queryData = getQuery(info.serverType, source);
		}
	}
	break;
	case 2:
		if (ImGui::Button("Disconnect"))
		{
			con.close();
			dirty = true;
		}
		break;
	case 3: {
		if (!info.peerInformation.hasReplayAccess())
		{
			selected = 0;
			break;
		}

		auto iter = displays.find(source);
		if (iter == displays.end() || !iter->second.isReplay())
		{
			if (ImGui::Button("Start"))
			{
				startReplay(con);
			}
		}
		else
		{
			if (ImGui::Button("Stop"))
			{
				displays.erase(iter);
			}
		}
	}
	break;
	case 4:
		if (!info.peerInformation.isModerator())
		{
			selected = 0;
			break;
		}
		if (ImGui::Button("Moderate"))
		{
			ImGui::OpenPopup("Moderation");
			Message::Packet packet;
			packet.source = source;
			packet.payload.emplace<Message::RequestServerInformation>();
			con.sendPacket(packet, true);
		}
		break;
	default:
		ImGui::Text("?");
		break;
	}

	actionSelections[source] = selected;
}

void TemStreamGui::draw()
{
	const auto size = drawMainMenuBar();
	const size_t connectionCount = getConnectionCount();

	if (ImGui::Begin("##afdasy4", nullptr,
					 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
						 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
						 ImGuiWindowFlags_NoScrollbar))
	{

		const auto &style = ImGui::GetStyle();
		const auto &bg = style.Colors[ImGuiCol_WindowBg];
		char text[KB(1)];
		snprintf(text, sizeof(text), "Connections: %zu", connectionCount);

		ImColor color;
		if (connectionCount == 0)
		{
			color = Colors::GetRed(colorIsLight(bg));
		}
		else
		{
			color = Colors::GetGreen(colorIsLight(bg));
		}
		const auto textSize = ImGui::CalcTextSize(text);
		auto draw = ImGui::GetForegroundDrawList();
		const float x = size.x - (textSize.x + size.x * 0.01f);
		const float radius = size.y * 0.5f;
		draw->AddCircleFilled(ImVec2(x - radius, radius), radius, color);
		draw->AddText(ImVec2(x, 0), color, text);
	}
	ImGui::End();

	if (configuration.showAudio)
	{
		if (ImGui::Begin("TemStream Audio", &configuration.showAudio, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SliderInt("Default Volume", &configuration.defaultVolume, 0, 100);
			ImGui::SliderInt("Default Silence Threshold", &configuration.defaultSilenceThreshold, 0, 100);
			if (!audio.empty() && ImGui::BeginTable("Audio", 6, TableFlags))
			{
				ImGui::TableSetupColumn("Device Name");
				ImGui::TableSetupColumn("Source/Destination");
				ImGui::TableSetupColumn("Recording");
				ImGui::TableSetupColumn("Level");
				ImGui::TableSetupColumn("Muted");
				ImGui::TableSetupColumn("Stop");
				ImGui::TableHeadersRow();

				audio.removeIfNot([this](const auto &source, const auto &a) {
					PushedID id(a->getName().c_str());

					ImGui::TableNextColumn();
					if (a->getType() == AudioSource::Type::RecordWindow)
					{
						ImGui::Text("%s", a->getName().c_str());
					}
					else
					{
						if (ImGui::Button(a->getName().c_str()))
						{
							audioTarget = source;
						}
					}

					ImGui::TableNextColumn();
					ImGui::Text("%s", source.serverName.c_str());

					const bool isLight = colorIsLight(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
					if (a->isRecording())
					{
						ImGui::TableNextColumn();
						ImGui::TextColored(Colors::GetGreen(isLight), "Yes");

						float v = a->getSilenceThreshold();
						ImGui::TableNextColumn();
						if (ImGui::SliderFloat("Silence Threshold", &v, 0.f, 1.f))
						{
							a->setSilenceThreshold(v);
						}
					}
					else
					{
						ImGui::TableNextColumn();
						ImGui::TextColored(Colors::GetRed(isLight), "No");

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
					bool result = true;
					if (ImGui::Button("Stop"))
					{
						result = false;
					}
					return result;
				});
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	configuration.showVideo &= !video.empty();
	if (configuration.showVideo)
	{
		if (ImGui::Begin("TemStream Video", &configuration.showVideo, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (ImGui::BeginTable("Video", 2, TableFlags))
			{
				ImGui::TableSetupColumn("Device Name");
				ImGui::TableSetupColumn("Stop");
				ImGui::TableHeadersRow();
				video.removeIfNot([this](const auto &, auto &v) {
					if (!v->isRunning())
					{
						return false;
					}
					String name;
					const auto &data = v->getInfo();
					if (data.name.empty())
					{
						StringStream ss;
						ss << v->getSource();
						name = ss.str();
					}
					else
					{
						name = data.name;
					}

					PushedID id(name.c_str());

					ImGui::TableNextColumn();
					ImGui::Text("%s", name.c_str());

					ImGui::TableNextColumn();
					bool result = true;
					if (ImGui::Button("Stop"))
					{
						v->setRunning(false);
						result = false;
					}

					return result;
				});
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	if (configuration.showFont)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("TemStream Font", &configuration.showFont))
		{
			UTF8Converter cvt;
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

	if (queryData != nullptr)
	{
		bool opened = true;
		if (ImGui::Begin("Send data to server", &opened,
						 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (queryData->draw())
			{
				queryData->execute();
				opened = false;
			}
		}
		ImGui::End();
		if (!opened)
		{
			queryData = nullptr;
		}
	}

	configuration.showDisplays &= !displays.empty();
	if (configuration.showDisplays)
	{
		if (ImGui::Begin("TemStream Displays", &configuration.showDisplays, ImGuiWindowFlags_AlwaysAutoResize))
		{
			for (auto &pair : displays)
			{
				if (ImGui::CollapsingHeader(pair.first.serverName.c_str()))
				{
					pair.second.drawFlagCheckboxes();
				}
			}
		}
		ImGui::End();
	}

	if (configuration.showConnections)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("TemStream Connections", &configuration.showConnections))
		{
			if (ImGui::BeginTable("Connections", 4, TableFlags))
			{
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Type");
				ImGui::TableSetupColumn("Action");
				ImGui::TableSetupColumn("Run");
				ImGui::TableHeadersRow();

				connections.forEach([this](const auto &source, auto &ptr) { renderConnection(source, ptr); });
				ImGui::EndTable();
			}
			if (ImGui::CollapsingHeader("Connect to stream"))
			{
				drawAddress(configuration.address);
				if (ImGui::Button("Connect"))
				{
					connect(configuration.address);
				}
			}
		}
		ImGui::End();
	}

	if (configuration.showLogs)
	{
		SetWindowMinSize(window);
		if (ImGui::Begin("TemStream Logs", &configuration.showLogs))
		{
			ImGui::Checkbox("Auto Scroll", &configuration.autoScrollLogs);
			ImGui::InputInt("Max Logs", &configuration.maxLogs);

			InMemoryLogger &mLogger = static_cast<InMemoryLogger &>(*logger);
			auto &style = ImGui::GetStyle();
			const bool isLight = colorIsLight(style.Colors[ImGuiCol_WindowBg]);
			auto &filter = configuration.showLogsFilter;
			ImGui::Checkbox("Errors", &filter.errors);
			ImGui::SameLine();
			ImGui::Checkbox("Warnings", &filter.warnings);
			ImGui::SameLine();
			ImGui::Checkbox("Basic", &filter.info);
			ImGui::SameLine();
			ImGui::Checkbox("Trace", &filter.trace);
			if (ImGui::BeginChild("Logs"))
			{
				mLogger.viewLogs([&style, isLight, &filter](const Logger::Log &log) {
					switch (log.first)
					{
					case Logger::Trace:
						if (filter.trace)
						{
							ImGui::TextColored(Colors::GetCyan(isLight), "%s", log.second.c_str());
						}
						break;
					case Logger::Info:
						if (filter.info)
						{
							ImGui::TextColored(style.Colors[ImGuiCol_Text], "%s", log.second.c_str());
						}
						break;
					case Logger::Warning:
						if (filter.warnings)
						{
							ImGui::TextColored(Colors::GetYellow(isLight), "%s", log.second.c_str());
						}
						break;
					case Logger::Error:
						if (filter.errors)
						{
							ImGui::TextColored(Colors::GetRed(isLight), "%s", log.second.c_str());
						}
						break;
					default:
						break;
					}
				});
			}
			if (configuration.autoScrollLogs)
			{
				ImGui::SetScrollHereY(1.f);
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	if (configuration.showStats)
	{
		if (ImGui::Begin("TemStream Stats", &configuration.showStats, ImGuiWindowFlags_AlwaysAutoResize))
		{
			SDL_version v;
			SDL_GetVersion(&v);
			ImGui::Text("TemStream %u.%u.%u", TemStream_VERSION_MAJOR, TemStream_VERSION_MINOR,
						TemStream_VERSION_PATCH);
			ImGui::Text("SDL %u.%u.%u", v.major, v.minor, v.patch);
			ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
			ImGui::Text("FPS: %2.2f", io.Framerate);
#if TEMSTREAM_USE_CUSTOM_ALLOCATOR
			if (ImGui::CollapsingHeader("Memory"))
			{
				const auto total = globalAllocatorData.getTotal();
				const auto used = globalAllocatorData.getUsed();
				const float percent = (float)used / (float)total;
				ImGui::ProgressBar(percent);
				const auto totalStr = printMemory(total);
				const auto usedStr = printMemory(used);
				ImGui::Text("%s / %s", usedStr.c_str(), totalStr.c_str());
				ImGui::Text("Allocations: %zu", globalAllocatorData.getNum());
			}
#endif
		}
		ImGui::End();
	}

	if (configuration.showColors)
	{
		if (ImGui::Begin("Select a style", &configuration.showColors, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static const char *styles[]{"Light", "Classic", "Dark", "Deep Dark", "Red", "Green", "Gold"};
			static int selected = 0;
			auto &style = ImGui::GetStyle();
			if (ImGui::Combo("Style", &selected, styles, IM_ARRAYSIZE(styles)))
			{
				switch (selected)
				{
				case 0:
					ImGui::StyleColorsLight(&style);
					break;
				case 1:
					ImGui::StyleColorsClassic(&style);
					break;
				case 2:
					ImGui::StyleColorsDark(&style);
					break;
				case 3:
					Colors::StyleDeepDark(style);
					break;
				case 4:
					Colors::StyleRed(style);
					break;
				case 5:
					Colors::StyleGreen(style);
					break;
				case 6:
					Colors::StyleGold(style);
					break;
				default:
					break;
				}
				std::copy(style.Colors, style.Colors + ImGuiCol_COUNT, configuration.colors.begin());
			}

			ImGui::Separator();
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			if (ImGui::CollapsingHeader("Custom Styles"))
			{
				if (configuration.customColors.empty())
				{
					ImGui::Text("None");
				}
				else
				{
					for (const auto &pair : configuration.customColors)
					{
						if (ImGui::Button(pair.first.c_str()))
						{
							configuration.colors = pair.second;
							std::copy(configuration.colors.begin(), configuration.colors.end(), style.Colors);
							break;
						}
					}
				}
			}

			ImGui::Separator();
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			if (ImGui::CollapsingHeader("Create Custom Style"))
			{
				static int selected = 0;
				static char name[32];
				if (ImGui::Button("<"))
				{
					selected = std::max(0, selected - 1);
				}
				ImGui::SameLine();
				ImGui::SliderInt(ImGuiColNames[selected], &selected, 0, ImGuiCol_COUNT - 1);
				ImGui::SameLine();
				if (ImGui::Button(">"))
				{
					selected = std::min(selected + 1, ImGuiCol_COUNT - 1);
				}
				ImGui::ColorPicker4("Color", &style.Colors[selected].x);

				ImGui::InputText("Name", name, sizeof(name));
				if (ImGui::Button("Save"))
				{
					ColorList colors;
					std::copy(style.Colors, style.Colors + ImGuiCol_COUNT, colors.begin());
					configuration.customColors.emplace(name, std::move(colors));
					memset(name, 0, sizeof(name));
					selected = 0;
				}
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
			if (ImGui::InputText("Directory", &dir, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (fs::is_directory(dir))
				{
					fileDirectory.emplace(dir);
				}
			}

			std::optional<String> newDir;
			if (ImGui::BeginChild("Files"))
			{
				if (ImGui::BeginTable("", 2, TableFlags))
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
			}
			ImGui::EndChild();
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
		audio.use(*audioTarget, [this](auto &a) {
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
				unique_ptr<AudioSource> newAudio = nullptr;
				if (recording)
				{
					newAudio = AudioSource::startRecording(a->getSource(), name,
														   configuration.defaultSilenceThreshold / 100.f);
				}
				else
				{
					newAudio = AudioSource::startPlayback(a->getSource(), name, configuration.defaultVolume / 100.f);
				}
				if (newAudio != nullptr)
				{
					a.swap(newAudio);
				}
			}
		});
	}
}

ServerType TemStreamGui::getSelectedQuery(const IQuery *queryData)
{
	if (queryData == nullptr)
	{
		return ServerType::UnknownServerType;
	}
	if (dynamic_cast<const QueryText *>(queryData) != nullptr)
	{
		return ServerType::Text;
	}
	if (dynamic_cast<const QueryChat *>(queryData) != nullptr)
	{
		return ServerType::Chat;
	}
	if (dynamic_cast<const QueryImage *>(queryData) != nullptr)
	{
		return ServerType::Image;
	}
	if (dynamic_cast<const QueryAudio *>(queryData) != nullptr)
	{
		return ServerType::Audio;
	}
	if (dynamic_cast<const QueryVideo *>(queryData) != nullptr)
	{
		return ServerType::Video;
	}
	return ServerType::UnknownServerType;
}

unique_ptr<IQuery> TemStreamGui::getQuery(const ServerType i, const Message::Source &source)
{
	switch (i)
	{
	case ServerType::Text:
		return tem_unique<QueryText>(*this, source);
	case ServerType::Chat:
		return tem_unique<QueryChat>(*this, source);
	case ServerType::Image:
		return tem_unique<QueryImage>(*this, source);
	case ServerType::Audio:
		return tem_unique<QueryAudio>(*this, source);
	case ServerType::Video:
		return tem_unique<QueryVideo>(*this, source);
	default:
		return nullptr;
	}
}

bool TemStreamGui::sendPacket(Message::Packet &&packet, const bool handleLocally)
{
	auto peer = getConnection(packet.source);
	if (peer)
	{
		if (!peer->sendPacket(packet))
		{
			return false;
		}
		if (handleLocally)
		{
			peer->addPacket(std::move(packet));
		}
		return true;
	}
	else
	{
		(*logger)(Logger::Error) << "Cannot send data to server: " << packet.source.serverName << std::endl;
		return false;
	}
}

bool TemStreamGui::sendPackets(MessagePackets &&packets, const bool handleLocally)
{
	auto pair = toMoveIterator(packets);
	for (auto iter = pair.first; iter != pair.second; ++iter)
	{
		if (!sendPacket(std::move(*iter), handleLocally))
		{
			return false;
		}
	}
	return true;
}

bool TemStreamGui::MessageHandler::operator()(Message::Video &v)
{
	auto pair = std::make_pair(source, std::move(v));
	gui.videoPackets.push(std::move(pair));
	gui.dirty = true;
	return true;
}

bool TemStreamGui::MessageHandler::operator()(Message::ServerInformation &info)
{
	auto con = gui.getConnection(source);
	if (con == nullptr)
	{
		return true;
	}

	con->setServerInformation(std::move(info));
	return true;
}

bool TemStreamGui::addAudio(unique_ptr<AudioSource> &&ptr)
{
	return audio.add(ptr->getSource(), std::move(ptr));
}

bool TemStreamGui::addVideo(shared_ptr<VideoSource> ptr)
{
	return video.add(ptr->getSource(), ptr);
}

bool TemStreamGui::useAudio(const Message::Source &source, const std::function<void(AudioSource &)> &func,
							const bool create)
{
	const bool used = audio.use(source, [&func](auto &a) { func(*a); });
	if (used)
	{
		return true;
	}
	else if (!create)
	{
		return false;
	}
	auto ptr = AudioSource::startPlayback(source, nullptr, configuration.defaultVolume / 100.f);
	if (ptr)
	{
		return audio.add(source, std::move(ptr)) && useAudio(source, func, false);
	}
	return false;
}

bool TemStreamGui::startReplay(const Message::Source &source)
{
	auto con = getConnection(source);
	if (!con)
	{
		(*logger)(Logger::Error) << "Requested replay of disconnected server" << std::endl;
		return false;
	}

	return startReplay(*con);
}

bool TemStreamGui::startReplay(ClientConnection &con)
{
	Message::Packet packet;
	packet.source = con.getSource();
	packet.payload.emplace<Message::GetTimeRange>();
	return con.sendPacket(packet);
}

bool TemStreamGui::getReplays(const Message::Source &source, const int64_t timestamp)
{
	auto con = getConnection(source);
	if (!con)
	{
		(*logger)(Logger::Error) << "Requested replay of disconnected server" << std::endl;
		return false;
	}

	return getReplays(*con, timestamp);
}

bool TemStreamGui::getReplays(ClientConnection &con, const int64_t timestamp)
{
	Message::Packet packet;
	packet.source = con.getSource();
	Message::GetReplay r{timestamp};
	packet.payload.emplace<Message::GetReplay>(std::move(r));
	return con->sendPacket(packet);
}

bool TemStreamGui::hasReplayAccess(const Message::Source &source)
{
	auto con = getConnection(source);
	if (!con)
	{
		return false;
	}

	return con->getInfo().peerInformation.hasReplayAccess();
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
		cfg.MergeMode = false;
		io.Fonts->AddFontFromMemoryCompressedTTF(Fonts[i], FontSizes[i], configuration.fontSize, &cfg, ranges);
		cfg.MergeMode = true;
		io.Fonts->AddFontFromMemoryCompressedTTF(NotoEmoji_compressed_data, NotoEmoji_compressed_size,
												 configuration.fontSize, &cfg, ranges);
	}
	for (const auto &file : configuration.fontFiles)
	{
		cfg.MergeMode = false;
		io.Fonts->AddFontFromFileTTF(file.c_str(), configuration.fontSize);
		cfg.MergeMode = true;
		io.Fonts->AddFontFromMemoryCompressedTTF(NotoEmoji_compressed_data, NotoEmoji_compressed_size,
												 configuration.fontSize, &cfg, ranges);
	}
	io.Fonts->Build();
	ImGui_ImplSDLRenderer_DestroyFontsTexture();
}

void TemStreamGui::handleMessage(Message::Packet &&m)
{
	if (std::visit(MessageHandler{*this, m.source}, m.payload))
	{
		return;
	}
	auto iter = displays.find(m.source);
	if (iter == displays.end())
	{
		auto pair = displays.try_emplace(m.source, StreamDisplay(*this, m.source));
		if (!pair.second)
		{
			return;
		}
		iter = pair.first;
	}

	if (!std::visit(iter->second, m.payload))
	{
		displays.erase(iter);
	}
	dirty = true;
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

int runApp(Configuration &configuration)
{
	IMGUI_CHECKVERSION();
#if TEMSTREAM_USE_CUSTOM_ALLOCATOR
	ImGui::SetAllocatorFunctions((ImGuiMemAllocFunc)allocatorImGuiAlloc, (ImGuiMemFreeFunc)allocatorImGuiFree, nullptr);
#endif

	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();

	{
		auto &style = ImGui::GetStyle();
		std::copy(configuration.colors.begin(), configuration.colors.end(), style.Colors);
	}

	TemStreamGui gui(io, configuration);
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

	const bool multiThread = std::thread::hardware_concurrency() > 1;
	(*logger)(Logger::Trace) << "Threads available: " << std::thread::hardware_concurrency() << std::endl;
	if (multiThread)
	{
		WorkPool::handleWorkInAnotherThread();
	}

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
				if (auto ptr = dynamic_cast<QueryText *>(gui.queryData.get()))
				{
					const size_t size = strlen(event.drop.file);
					String s(event.drop.file, event.drop.file + size);
					ptr->setText(std::move(s));
				}
				else
				{
					(*logger)(Logger::Error) << "Text can only be sent to a text server" << std::endl;
				}
				SDL_free(event.drop.file);
			}
			break;
			case SDL_DROPFILE: {
				if (isTTF(event.drop.file))
				{
					*logger << "Adding new font: " << event.drop.file << std::endl;
					gui.configuration.fontFiles.emplace_back(event.drop.file);
					gui.configuration.fontIndex = IM_ARRAYSIZE(Fonts) + gui.configuration.fontFiles.size() - 1;
					gui.LoadFonts();
				}
				else
				{
					if (gui.queryData == nullptr)
					{
						(*logger)(Logger::Error) << "No server to send file to..." << std::endl;
						break;
					}
					Message::Source source = gui.queryData->getSource();
					String s(event.drop.file);
					WorkPool::workPool.addWork([&gui, s = std::move(s), source = std::move(source)]() {
						Work::checkFile(gui, source, s);
						return false;
					});
				}
				SDL_free(event.drop.file);
			}
			break;
			case SDL_USEREVENT:
				switch (event.user.code)
				{
				case TemStreamEvent::SendSingleMessagePacket: {
					Message::Packet *packet = reinterpret_cast<Message::Packet *>(event.user.data1);
					const bool b = reinterpret_cast<size_t>(event.user.data2) != 0;
					gui.sendPacket(std::move(*packet), b);
					destroyAndDeallocate(packet);
				}
				break;
				case TemStreamEvent::HandleMessagePacket: {
					Message::Packet *packet = reinterpret_cast<Message::Packet *>(event.user.data1);
					gui.handleMessage(std::move(*packet));
					destroyAndDeallocate(packet);
				}
				break;
				case TemStreamEvent::SendMessagePackets: {
					MessagePackets *packets = reinterpret_cast<MessagePackets *>(event.user.data1);
					const bool b = reinterpret_cast<size_t>(event.user.data2) != 0;
					gui.sendPackets(std::move(*packets), b);
					destroyAndDeallocate(packets);
				}
				break;
				case TemStreamEvent::HandleMessagePackets: {
					MessagePackets *packets = reinterpret_cast<MessagePackets *>(event.user.data1);
					auto pair = toMoveIterator(std::move(*packets));
					std::for_each(pair.first, pair.second,
								  [&gui](Message::Packet &&packet) { gui.handleMessage(std::move(packet)); });
					destroyAndDeallocate(packets);
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
					SDL_Surface *surfacePtr = reinterpret_cast<SDL_Surface *>(event.user.data1);
					SDL_SurfaceWrapper surface(surfacePtr);
					Message::Source *sourcePtr = reinterpret_cast<Message::Source *>(event.user.data2);
					auto source = unique_ptr<Message::Source>(sourcePtr);
					auto iter = gui.displays.find(*source);
					if (iter == gui.displays.end())
					{
						if (!gui.hasConnection(*source))
						{
							break;
						}

						auto [iterR, result] = gui.displays.try_emplace(*source, StreamDisplay(gui, *source));
						if (!result)
						{
							break;
						}
						iter = iterR;
					}
					iter->second.setSurface(*surface);
				}
				break;
				case TemStreamEvent::AddAudio: {
					auto audio = reinterpret_cast<AudioSource *>(event.user.data1);
					auto ptr = unique_ptr<AudioSource>(audio);
					String name = ptr->getName();
					if (gui.addAudio(std::move(ptr)))
					{
						*logger << "Using audio device: " << name << std::endl;
					}
				}
				break;
				case TemStreamEvent::HandleFrame: {
					VideoSource::Frame *framePtr = reinterpret_cast<VideoSource::Frame *>(event.user.data1);
					auto frame = unique_ptr<VideoSource::Frame>(framePtr);
					Message::Source *sourcePtr = reinterpret_cast<Message::Source *>(event.user.data2);
					auto source = unique_ptr<Message::Source>(sourcePtr);
					if (!gui.hasConnection(*source))
					{
						break;
					}
					auto iter = gui.displays.find(*source);
					if (iter == gui.displays.end())
					{
						auto [newIter, added] = gui.displays.try_emplace(*source, StreamDisplay(gui, *source));
						if (!added)
						{
							break;
						}
						iter = newIter;
					}
					iter->second.updateTexture(*frame);
				}
				break;
				default:
					break;
				}
				break;
			case SDL_AUDIODEVICEADDED:
				(*logger)(Logger::Trace) << "Audio " << (event.adevice.iscapture ? "capture" : "playback")
										 << " device added" << std::endl;
				break;
			case SDL_AUDIODEVICEREMOVED:
				(*logger)(Logger::Trace) << "Audio " << (event.adevice.iscapture ? "capture" : "playback")
										 << " device removed" << std::endl;
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
					gui.configuration.showAudio = !gui.configuration.showAudio;
					break;
				case SDLK_d:
					gui.configuration.showDisplays = !gui.configuration.showDisplays;
					break;
				case SDLK_w:
					gui.configuration.showConnections = !gui.configuration.showConnections;
					break;
				case SDLK_l:
					gui.configuration.showLogs = !gui.configuration.showLogs;
					break;
				case SDLK_h:
					gui.configuration.showVideo = !gui.configuration.showVideo;
					break;
				case SDLK_i:
					gui.configuration.showStats = !gui.configuration.showStats;
					break;
				case SDLK_t:
					gui.configuration.showColors = !gui.configuration.showColors;
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		if (gui.dirty)
		{
			if (gui.getConnectionCount() == 0)
			{
				gui.displays.clear();
			}

			// Only keep displays, audios, videos if connection is present
			for (auto iter = gui.displays.begin(); iter != gui.displays.end();)
			{
				if (gui.hasConnection(iter->first))
				{
					++iter;
				}
				else
				{
					(*logger)(Logger::Trace)
						<< "Removed stream display '" << iter->first << "' because its connection is gone" << std::endl;
					iter = gui.displays.erase(iter);
				}
			}

			gui.audio.removeIfNot(
				[&gui](const auto &source, const auto &a) { return gui.hasConnection(source) && a->isActive(); });

			gui.video.removeIfNot([&gui](const auto &source, const auto &v) {
				if (!v->isRunning())
				{
					return false;
				}
				else if (!gui.hasConnection(source))
				{
					v->setRunning(false);
					return false;
				}
				return true;
			});

			gui.dirty = false;
		}

		if (!multiThread)
		{
			using namespace std::chrono_literals;
			WorkPool::workPool.handleWork(0ms);
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

	gui.audio.clear();
	gui.video.forEach([](const auto &, auto &value) { value->setRunning(false); });
	gui.video.clear();
	WorkPool::workPool.clear();

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	logger = nullptr;
	return EXIT_SUCCESS;
}
TemStreamGuiLogger::TemStreamGuiLogger(TemStreamGui &gui) : gui(gui)
{
}
TemStreamGuiLogger::~TemStreamGuiLogger()
{
	saveLogs();
}
void TemStreamGuiLogger::saveLogs()
{
	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "TemStream_log_%zu.txt", time(nullptr));
	std::ofstream file(buffer);
	viewLogs([&file](const Log &log) { file << log.first << ": " << log.second; });
	clear();
}
void TemStreamGuiLogger::Add(const Level level, const String &s, const bool b)
{
	checkError(level);
	InMemoryLogger::Add(level, s, b);
	if (size() > static_cast<size_t>(gui.configuration.maxLogs))
	{
		saveLogs();
	}
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
		std::sort(files.begin(), files.end());
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << e.what() << std::endl;
		cleanSwap(files);
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
void TemStreamGui::RenderCredentials::operator()(String &s) const
{
	ImGui::InputText("Token", &s);
}
void TemStreamGui::RenderCredentials::operator()(std::pair<String, String> &pair) const
{
	ImGui::InputText("Username", &pair.first);
	ImGui::InputText("Password", &pair.second, ImGuiInputTextFlags_Password);
}
} // namespace TemStream
