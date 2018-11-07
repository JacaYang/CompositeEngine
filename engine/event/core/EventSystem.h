#ifndef _CE_EVENT_SYSTEM_H_
#define _CE_EVENT_SYSTEM_H_

#include "Event.h"
#include "EventListener.h"

#include <unordered_map>
#include <queue>

class EventSystem
{
public:
	void RegisterListener(EventListener* listener, EventType type);

	void SendEvent(const Event& event, EventListener& listener);
	void EnqueueEvent(const Event& event);

	void DispatchEvents();

private:
	std::queue<Event*> eventQueue;
	std::unordered_map<EventType, std::vector<EventListener*>> registeredListeners;
};

#endif // _CE_EVENT_SYSTEM_H_
