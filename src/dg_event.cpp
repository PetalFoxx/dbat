#include "dbat/dg_event.h"
#include "dbat/dg_scripts.h"
#include "dbat/utils.h"

std::list<std::weak_ptr<trig_data>> triggers_waiting;

/* Process any events whose time has come. */
void event_process(uint64_t heart_pulse, double deltaTime) {
    // copy the queue to avoid any issues with the queue being modified while we're processing it
    auto queued = triggers_waiting;

    for (auto t : queued) {
        if(auto trig = t.lock()) {
            if(!trig->active) {
                continue;
            }
            trig->waiting -= deltaTime;
            if(trig->waiting <= 0) {
                trig->waiting = 0.0;
                trig->execute();
            }
        }
    }
    triggers_waiting.remove_if([](const std::weak_ptr<trig_data>& t) {
        if(t.expired()) return true;
        if(auto trig = t.lock()) {
            if(!trig->active) return true;
            if(trig->waiting <= 0) return true;
        }
        return false;
    });
}
