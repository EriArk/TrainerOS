#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
import ethernet as lan


def arp(sender, target, broadcast=True):
    return ((lan.BROADCAST if broadcast else lan.mac(target))+lan.mac(sender)+b'\x08\x06'
            +b'\x00\x01\x08\x00\x06\x04\x00\x01'+lan.mac(sender)
            +lan.IP_PREFIX+bytes([sender])+bytes(6)+lan.IP_PREFIX+bytes([target]))


def ipv4(sender, target):
    header = bytearray(20)
    header[0] = 0x45
    header[2:4] = (28).to_bytes(2, 'big')
    header[12:16] = lan.IP_PREFIX+bytes([sender])
    header[16:20] = lan.IP_PREFIX+bytes([target])
    return lan.mac(target)+lan.mac(sender)+b'\x08\x00'+bytes(header)+bytes(8)


class SegmentChecks(unittest.TestCase):
    seats = [1, 2, 3, 4]

    def test_four_member_arp_and_native_unicast_routing(self):
        self.assertEqual(lan.targets(arp(2, 1), self.seats, 1, 2), {1, 3, 4})
        self.assertEqual(lan.targets(ipv4(1, 4), self.seats, 1), {4})
        self.assertEqual(lan.targets(ipv4(2, 3), self.seats, 2), {1})
        self.assertEqual(lan.targets(ipv4(2, 3), self.seats, 1, 2), {3})
        self.assertEqual(lan.targets(ipv4(2, 3), self.seats, 3, 1), {3})

    def test_private_channels_cannot_impersonate_another_slot(self):
        for data in (arp(3, 1), ipv4(3, 1)):
            with self.assertRaises(ValueError):
                lan.targets(data, self.seats, 1, 2)
        data = bytearray(ipv4(2, 1))
        data[26:30] = lan.IP_PREFIX+b'\x03'
        with self.assertRaises(ValueError):
            lan.frame(bytes(data), self.seats, 2)

    def test_frames_cannot_reach_host_lan_or_unaccepted_seats(self):
        for offset, value in [(30, b'\xc0\xa8\x32\x01'), (0, bytes(6))]:
            data = bytearray(ipv4(2, 1))
            data[offset:offset+len(value)] = value
            with self.assertRaises(ValueError):
                lan.frame(bytes(data), self.seats, 2)
        with self.assertRaises(ValueError):
            lan.frame(ipv4(2, 3), [1, 2], 2)

    def test_truncated_oversized_and_wrong_protocol_are_rejected(self):
        original = ipv4(2, 1)
        for data in (original[:33], original+bytes(1600), original[:12]+b'\x86\xdd'+original[14:]):
            with self.assertRaises(ValueError):
                lan.frame(data, self.seats, 2)


if __name__ == '__main__':
    unittest.main()
