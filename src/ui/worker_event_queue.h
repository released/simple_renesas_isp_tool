#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <utility>
#include <vector>

namespace firmware_upgrade::ui {
// The session mutex owns synchronization. One wake covers all queued events;
// the UI drains bounded batches on a timer so paint/input can run between them.
template<class Event>
class WorkerEventQueue {
public:
    // One admitted operation has at most three protected events (report start,
    // report result, completion). Keep room for them and one replaceable item.
    explicit WorkerEventQueue(std::size_t maximum=256U) : maximum_((std::max)(std::size_t{4U},maximum)) {}
    bool Push(Event event) {
        using Kind=decltype(event.kind);
        if(event.kind==Kind::Progress) {
            for(auto it=events_.rbegin();it!=events_.rend();++it) {
                if(it->kind!=Kind::Log && it->kind!=Kind::Progress) break;
                if(it->kind==Kind::Progress) {
                    if(it->text==event.text && it->indeterminate==event.indeterminate) {
                        events_.erase(std::next(it).base());
                    }
                    break;
                }
            }
        }
        if (event.kind == Kind::State) {
            for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
                if (it->kind != Kind::Log && it->kind != Kind::State) break;
                if (it->kind == Kind::State) {
                    events_.erase(std::next(it).base());
                    break;
                }
            }
        }
        if(events_.size()>=maximum_) {
            auto discard = events_.end();
            for (const auto kind : {Kind::Log, Kind::Progress, Kind::State}) {
                discard = std::find_if(events_.begin(), events_.end(),
                    [kind](const Event& pending) {return pending.kind == kind;});
                if (discard != events_.end()) break;
            }
            ++dropped_;
            const bool last_state = discard != events_.end() && discard->kind == Kind::State &&
                std::none_of(std::next(discard), events_.end(),
                    [](const Event& pending) {return pending.kind == Kind::State;});
            if (discard == events_.end() ||
                (last_state && (event.kind == Kind::Log || event.kind == Kind::Progress))) {
                // Unsupported control-only floods fail closed: keep the owned
                // results and latest live state, bound memory, and expose loss.
                if (wake_pending_) return false;
                wake_pending_ = true;
                return true;
            }
            events_.erase(discard);
        }
        events_.push_back(std::move(event));
        if(wake_pending_) return false;
        wake_pending_=true;
        return true;
    }
    std::vector<Event> Take(std::size_t maximum) {
        std::vector<Event> result;
        const auto count=(std::min)(maximum,events_.size());
        result.reserve(count);
        for(std::size_t i=0;i<count;++i) {
            result.push_back(std::move(events_.front()));
            events_.pop_front();
        }
        if(events_.empty()) wake_pending_=false;
        return result;
    }
    bool Pending() const {return !events_.empty();}
    std::uint64_t TakeDropped() {
        const auto result=dropped_;
        dropped_=0U;
        return result;
    }
    void RetryWake() {wake_pending_=false;}
private:
    std::size_t maximum_;
    std::deque<Event> events_;
    std::uint64_t dropped_=0U;
    bool wake_pending_=false;
};
} // namespace firmware_upgrade::ui
