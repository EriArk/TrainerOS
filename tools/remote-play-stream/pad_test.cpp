// SPDX-License-Identifier: GPL-3.0-or-later
#include "pad_state.hpp"
#include <cstdio>
int main() {
    unsigned checks=0,failed=0;
    auto check=[&](bool ok){++checks;if(!ok)++failed;};
    PadLease lease;PadState press{};press.buttons=0x1000;press.lx=12000;
    uint64_t now=1000000000;
    check(!lease.update(press,1,now)); // No host grant.
    lease.permission(true,now);
    check(!lease.update(press,2,now)); // Held input cannot cross grant boundary.
    check(lease.update({},3,now));
    check(lease.update(press,4,now));
    check(lease.current(now).buttons==0x1000&&lease.current(now).lx==12000);
    check(!lease.update({},3,now)); // Out-of-order state cannot overwrite a newer packet.
    check(lease.current(now).buttons==0x1000);
    lease.permission(false,now);
    check(lease.current(now).neutral());
    check(!lease.update(press,5,now));
    lease.permission(true,now);
    check(!lease.update(press,6,now));
    check(lease.update({},7,now));
    PadState forbidden{};forbidden.buttons=0x0c00;
    check(lease.update(forbidden,8,now)&&lease.current(now).neutral());
    check(lease.update(press,9,now));
    now+=PadLease::TimeoutNs+1;
    check(lease.current(now).neutral()); // Stale local lease and network input.
    lease.permission(true,now);
    check(!lease.update(press,10,now));
    check(lease.update({},11,now));
    check(lease.update(press,12,now));
    lease.permission(true,now+PadLease::TimeoutNs);
    check(lease.current(now+PadLease::TimeoutNs+1).neutral()); // Network-only timeout.
    lease.revoke();check(lease.current(now).neutral());
    PadLease delayed;delayed.permission(true,now);
    check(delayed.update({},1,now));check(delayed.update(press,2,now));
    delayed.permission(true,now+PadLease::TimeoutNs+1);
    check(!delayed.update(press,3,now+PadLease::TimeoutNs+1));
    check(delayed.current(now+PadLease::TimeoutNs+1).neutral());
    std::printf("pad lease checks=%u failed=%u\n",checks,failed);
    return failed?1:0;
}
