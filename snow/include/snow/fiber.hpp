#pragma once
#ifndef CONTINUATION_H_6NGEZ11
#define CONTINUATION_H_6NGEZ11

#include "snow/object.hpp"
#include "snow/objectptr.hpp"

namespace snow {
	struct Fiber;
	struct Class;
	typedef const ObjectPtr<Fiber>& FiberPtr;
	typedef const ObjectPtr<const Fiber>& FiberConstPtr;
	struct CallFrame;
	
	ObjectPtr<Fiber> create_fiber(const Value& functor); // functor is called with arguments calling_fiber, incoming_value
	Value fiber_resume(FiberPtr fiber, const Value& incoming_value);
	ObjectPtr<Fiber> get_current_fiber();
	ObjectPtr<Fiber> fiber_get_link(FiberConstPtr fiber);
	CallFrame* fiber_get_current_frame(FiberConstPtr fiber);
	ObjectPtr<Class> get_fiber_class();
}

#endif /* end of include guard: CONTINUATION_H_6NGEZ11 */
