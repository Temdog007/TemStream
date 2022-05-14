#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
class WorkPool
{
  private:
	ConcurrentQueue<std::function<bool()>> workList;

	static shared_ptr<WorkPool> globalWorkPool;

  public:
	WorkPool();
	~WorkPool();

	void clear();

	void add(std::function<bool()> &&);

	static void setGlobalWorkPool(shared_ptr<WorkPool>);
	static void handleWorkInAnotherThread();
	static void addWork(std::function<bool()> &&);

	template <typename _Rep, typename _Period> bool handleWork(const std::chrono::duration<_Rep, _Period> &maxWaitTime)
	{
		std::optional<std::function<bool()>> work;
		work = workList.pop(maxWaitTime);
		if (!work)
		{
			return false;
		}
		bool canPutBack = (*work)();
		if (canPutBack)
		{
			workList.push(std::move(*work));
		}
		return true;
	}
};
namespace Work
{
extern void checkFile(TemStreamGui &, const Message::Source &, const String &);
extern void sendImage(const String &, const Message::Source &);
extern void loadSurface(const Message::Source &, const ByteList &);
extern void startRecordingAudio(const Message::Source &, const std::optional<String> &, const float silenceThreshold);
extern void startRecordingWindowAudio(const Message::Source &, const WindowProcess &, const float silenceThreshold);
} // namespace Work
} // namespace TemStream
