#ifndef __event_HPP
#define __event_HPP

// 이벤트는 크기별로 풀을 두어 관리한다.
// 4바이트 이하의 이벤트는 detail::gPool4에서,
// 4바이트 초과 16바이트 이하의 이벤트는 detail::gPool16에서 할당받는다.
// holdEvent 매크로를 통해 자동으로 이벤트를 적절한 풀에서 할당 받아 이벤트 리스트에 추가할 수 있다.
namespace detail {

template <std::size_t N>
struct RawChunk {
	char mem[N];
};

extern Pool<RawChunk<4>> gPool4;
extern Pool<RawChunk<16>> gPool16;

template <class T>
char* fetchFromPool(...) {
	static_assert(false, "detail::fetchFromPool는 16바이트 초과 할당을 지원하지 않습니다.");
}

template <class T>
	requires (sizeof(T) <= 4)
char* fetchFromPool() {
	return gPool4.alloc()->mem;
}

template <class T>
	requires (sizeof(T) > 4 && sizeof(T) <= 16)
char* fetchFromPool() {
	return gPool16.alloc()->mem;
}

}	// namespace detail

// @brief eventList에 exprEventInit 표현식으로 이벤트를 생성한다.
//     ex) holdEvent( myEventList, MyEvent{ .myArg = arg } );
// @param eventList 생성된 이벤트를 저장할 이벤트 리스트, EventList 타입이어야 한다.
// @param exprEventInit 이벤트 생성 표현식
#define holdEvent(eventList, ...) \
    new ( \
        eventList.emplace_back( \
            detail::fetchFromPool<decltype(__VA_ARGS__)>() \
        ) \
    ) __VA_ARGS__

#define clearEvents(eventList)	\
	for (auto p : eventList) {	\
		if (detail::gPool4.contains(p)) {	\
			detail::gPool4.free( reinterpret_cast<detail::RawChunk<4>*>(p) );	\
		}	\
		else if (detail::gPool16.contains(p)) {	\
			detail::gPool16.free( reinterpret_cast<detail::RawChunk<16>*>(p) );	\
		}	\
	}	\
	eventList.clear()

// 이벤트는 처리 도중 새로운 이벤트를 발생시킬 수 있다.
// 때문에 순회 도중에 자료구조에 원소가 추가되더라도 기존의 반복자가 무효화되지 않는
// 자료구조가 필요하다.
using EventList = std::list<char*>;

// 모든 이벤트에 대한 타입 열거형
// 이 열거형에 대한 switch문을 통해 어떤 이벤트 버스에 전달할지,
// 어떻게 처리할지를 결정한다.
enum class EventType : u32t {
	Hit,
	Blood,
	Death,
	SIZE
};

// 모든 이벤트의 기반 구조체
// 이벤트들은 반드시 이 구조체를 상속하도록 한다.
// BasicEvent*가 가리키는 이벤트 객체의 type을 보고
// BasicEvent*를 구체적인 이벤트 구조체 포인터로 캐스팅하여 처리해야 한다.
struct BasicEvent {
	EventType type;
};

struct EvHit : BasicEvent { 
	EvHit() : BasicEvent{EventType::Hit} {}
	EvHit(i32t targetId, i32t hp)
		: BasicEvent{EventType::Hit}, targetId{targetId}, hp{hp} {}

	i32t targetId{-1};
	i32t hp{-1};
};
struct EvBlood : BasicEvent {
	EvBlood() : BasicEvent{EventType::Blood} {}
	EvBlood(i32t victimId) : BasicEvent{EventType::Blood}, victimId{victimId} {}

	i32t victimId{-1};
};
struct EvDeath : BasicEvent { 
	EvDeath() : BasicEvent{EventType::Death} {}
	EvDeath(i32t playerId) : BasicEvent{EventType::Death}, playerId{playerId} {}

	i32t playerId{-1};
};

class Timer;

// 이벤트의 수신 및 처리를 전담하는 클래스 인터페이스
// 어떠한 클래스에 이벤트 수신 기능을 달고 싶으면
// 이 클래스를 상속하여 이벤트 버스 클래스를 정의하고 그 객체를 멤버로 두자.
// 그리고 이벤트 버스에 대한 getter를 두어 외부에서 접근 가능하도록 하면 된다.
// (기왕이면 IEventBus*를 리턴하는 getter)
class IEventBus {
public:
	virtual ~IEventBus() = default;
	// @brief 이벤트를 수신해 처리한다.
	//     자체적으로 이벤트 버스의 소유자에 대한 갱신을 수행하거나,
	//     다른 이벤트 버스에 분배한다.
	// @param event 수신할 이벤트, type 멤버를 관찰하여 적절한 이벤트 구조체 포인터로 캐스팅해 사용하도록 한다.
	// @param deltaTime 프레임 경과 시간
	// @param evList 이벤트 처리 중 새로운 이벤트가 발생할 경우 발생된 이벤트들을 담을 이벤트 리스트
	// @param timer 이벤트 처리 중 지연된 작업을 예약할 경우 사용할 타이머
	// @param pVoidOwner 이벤트 버스의 소유자, 적절한 타입으로 캐스팅해 사용하도록 한다.
	// @note 소유자에 대한 포인터를 멤버로 저장하지 않고 전달받는 이유는,
	//     기껏 Rule of zero로 특별 멤버 함수들 작성할 필요 없이 짜놨는데
	//     이벤트 버스에 소유자 포인터를 두면서 모든 특별 멤버 함수를
	//     작성해야 하는 불상사를 막기 위함이다.
	//     용례가 보기 불편해도 어쩔 수 없다.
	virtual void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) = 0;
};

class NullEventBus : public IEventBus {
public:
	void receive(const BasicEvent* event, Seconds deltaTime, EventList& evList, Timer& timer, void* pVoidOwner) override {}
};

extern NullEventBus gNullEventBus;


#endif	// __event_HPP