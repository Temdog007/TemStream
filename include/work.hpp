#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class WorkPool
{
  private:
	List<std::thread> threads;
	ConcurrentQueue<std::function<void()>> workList;
	std::atomic_int32_t ready;

	static void handleWork(WorkPool &);

	struct Readiness
	{
		std::atomic_int32_t &ready;
		Readiness(std::atomic_int32_t &);
		~Readiness();
	};

  public:
	WorkPool();
	~WorkPool();

	static WorkPool workPool;

	void waitForAll();

	void addWork(std::function<void()> &&);
};
namespace Work
{
// Since work can be in another thread, all arguments (expect GUI since its basically global)
// must be copied
extern void checkFile(TemStreamGui &, String);
extern void sendImage(String, Message::Source);
extern void loadSurface(Message::Source, ByteList);
extern void startRecordingAudio(const Message::Source, const std::optional<String>, const float silenceThreshold);
extern void startRecordingWindowAudio(const Message::Source, const WindowProcess, const float silenceThreshold);
} // namespace Work
} // namespace TemStream
