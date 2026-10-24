// SPDX-License-Identifier: GPL-3.0-or-later
#include "Core/HLE/AdhocRelayRetry.h"
#include <cassert>
int main() {
    using namespace std::chrono;
    AdhocRelayRetry retry;
    auto now=AdhocRelayRetry::Clock::time_point{}+seconds(100);
    assert(retry.Ready(now) && !retry.Failing());
    // Requests every frame must not spin a failing TCP connect every frame.
    for (int n=0;n<15;++n) {
        assert(retry.Ready(now)); retry.Failed(now);
        assert(retry.Failing() && !retry.Ready(now+milliseconds(999)));
        now+=seconds(1);
    }
    // Exceed the old permanent-disable threshold; keep bounded retry opportunities.
    for (int n=0;n<20;++n) {
        assert(retry.Ready(now));retry.Failed(now);
        assert(!retry.Ready(now+seconds(4)));
        now+=seconds(5);
    }
    assert(retry.Ready(now));retry.Reset();
    assert(!retry.Failing() && retry.Ready(now));
    retry.Failed(now);assert(retry.Ready(now+seconds(1)));
    retry.Reset();assert(retry.Ready(now));
}
