#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class WorkPool
{
  private:
	List<std::thread> threads;
	ConcurrentQueue<std::function<void()>> workList;

  public:
	WorkPool();
	~WorkPool();

	static WorkPool workPool;

	void waitForAll();

	void addWork(std::function<void()> &&);

	static void handleWorkInAnotherThread();

	template <typename _Rep, typename _Period> bool handleWork(const std::chrono::duration<_Rep, _Period> &maxWaitTime)
	{
		std::optional<std::function<void()>> work;
		work = workList.pop(maxWaitTime);
		if (!work)
		{
			return false;
		}
		(*work)();
		return true;
	}
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
