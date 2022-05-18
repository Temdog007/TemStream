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
	static List<std::thread> handleWorkAsync();
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
