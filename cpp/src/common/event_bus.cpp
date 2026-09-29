#include "common/event_bus.h"
namespace fkw {
GlobalEventBus& GlobalEventBus::instance() {
    static GlobalEventBus bus;
    return bus;
}
}  // namespace fkw
