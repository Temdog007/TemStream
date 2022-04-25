#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
typedef LinkedList<std::future<void>> WorkList;
extern const std::launch TaskPolicy;
class Task
{
  private:
	static WorkList workList;

  public:
	Task() = delete;
	~Task() = delete;

	static void waitForAll();
	static void addTask(std::future<void> &&);
	static void cleanupTasks();

	// Since work can be in another thread, all arguments (expect GUI since its basically global)
	// must be copied
	static void checkFile(TemStreamGui &, String);
	static void sendImage(String, Message::Source);
	static void loadSurface(Message::Source, ByteList);
	static void startPlayback(const Message::Source, const std::optional<String>, const float volume);
	static void startRecordingAudio(const Message::Source, const std::optional<String>, const float silenceThreshold);
	static void startRecordingWindowAudio(const Message::Source, const WindowProcess, const float silenceThreshold);
};
} // namespace TemStream
