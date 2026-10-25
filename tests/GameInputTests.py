"""Mapping, focus loss and independent watchdog output release regressions."""
import copy
import importlib.util
import json
from pathlib import Path
import socket
import threading
import unittest

spec = importlib.util.spec_from_file_location('game_input', Path(__file__).parents[1]/'packaging/integrations/game_input.py')
mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)


def profile():
    return dict(version=1, mode='keyboard_mouse', leftStick='wasd', rightStick='mouse',
                deadzone=20, sensitivity=600, modifier='leftShoulder',
                bindings={'south':'Space', 'east':'Space', 'select':'Ring 1'},
                shiftBindings={'south':'Ctrl+S'}, rings=[['1','2','3','4']])


def sample(buttons=(), sticks=(0,0,0,0), **extra):
    return dict(connected=True, neutral=not buttons and not any(sticks), buttons=list(buttons),
                sticks=list(sticks), **extra)


class Sink:
    def __init__(self): self.events=[]; self.down=set()
    def button(self,key,down):
        self.events.append((key,down))
        if down: self.down.add(key)
        else: self.down.discard(key)
    def motion(self,x,y): self.events.append(('motion',x,y))


class MappingTests(unittest.TestCase):
    def mapper(self):
        mapper=mod.Mapper(profile()); mapper.frame(sample(),True,0); return mapper

    def test_requires_neutral_on_start_and_after_focus_loss(self):
        m=mod.Mapper(profile())
        self.assertFalse(m.frame(sample(['south']),True,0)['held'])
        m.frame(sample(),True,.02)
        self.assertEqual(m.frame(sample(['south']),True,.04)['held'],['Space'])
        self.assertFalse(m.frame(sample(['south']),False,.06)['held'])
        self.assertFalse(m.frame(sample(['south']),True,.08)['held'])
        m.frame(sample(),True,.10)
        self.assertEqual(m.frame(sample(['south']),True,.12)['held'],['Space'])

    def test_shared_key_survives_one_physical_release(self):
        m=self.mapper(); sink=Sink(); out=mod.HeldOutput(sink)
        out.apply(m.frame(sample(['south','east']),True,.02))
        out.apply(m.frame(sample(['east']),True,.04))
        self.assertEqual(sink.down,{'Space'})
        self.assertEqual(sink.events.count(('Space',True)),1)
        out.apply(m.frame(sample(),True,.06));self.assertFalse(sink.down)

    def test_modifier_switch_releases_old_binding_and_orders_chord(self):
        m=self.mapper(); sink=Sink(); out=mod.HeldOutput(sink)
        out.apply(m.frame(sample(['south']),True,.02))
        out.apply(m.frame(sample(['south','leftShoulder']),True,.04))
        self.assertEqual(sink.down,{'Ctrl','S'})
        self.assertLess(sink.events.index(('Ctrl',True)),sink.events.index(('S',True)))
        out.release(); self.assertFalse(sink.down)
        self.assertLess(sink.events.index(('S',False)),sink.events.index(('Ctrl',False)))

    def test_ring_suppresses_normal_actions_and_fires_once_on_release(self):
        m=self.mapper()
        frame=m.frame(sample(['select','south'],(1,0,1,0)),True,.02)
        self.assertEqual(frame['radial']['selected'],1)
        self.assertFalse(frame['held']);self.assertEqual(frame['motion'],[0,0])
        self.assertEqual(m.frame(sample([], (1,0,0,0)),True,.04)['held'],['2'])
        self.assertEqual(m.frame(sample(),True,.06)['held'],[])

    def test_ring_center_and_home_cancel(self):
        m=self.mapper();m.frame(sample(['select'],(0,-1,0,0)),True,.02)
        self.assertFalse(m.frame(sample(),True,.04)['held'])
        m.frame(sample(['select'],(0,-1,0,0)),True,.06)
        self.assertFalse(m.frame(sample([], (0,-1,0,0),home=True),True,.08)['held'])
        self.assertFalse(m.frame(sample([], (0,-1,0,0)),True,.10)['held'])

    def test_touch_ring_choice_fires_once_and_requires_button_release(self):
        m=self.mapper();m.frame(sample(['select']),True,.02)
        m.select_slot(2)
        self.assertEqual(m.frame(sample(['select']),True,.04)['held'],['3'])
        self.assertFalse(m.frame(sample(['select']),True,.06)['radial'])
        m.frame(sample(),True,.08)
        self.assertTrue(m.frame(sample(['select']),True,.10)['radial'])

    def test_focus_loss_cancels_queued_touch_choice(self):
        m=self.mapper();m.frame(sample(['select']),True,.02);m.select_slot(2)
        self.assertFalse(m.frame(sample(['select']),False,.04)['held'])
        m.frame(sample(),True,.06)
        self.assertFalse(m.frame(sample(),True,.08)['held'])

    def test_disconnect_releases_mouse_and_keyboard(self):
        p=profile();p['bindings']['east']='MouseLeft';m=mod.Mapper(p);m.frame(sample(),True,0)
        sink=Sink();out=mod.HeldOutput(sink);out.apply(m.frame(sample(['east','south']),True,.02))
        out.apply(m.frame({'connected':False},True,.04));self.assertFalse(sink.down)

    def test_bad_profiles_and_reserved_home_rejected(self):
        for key,value in [('version',2),('deadzone',float('nan')),('sensitivity',0),('mode','exec')]:
            p=profile();p[key]=value
            with self.assertRaises(ValueError): mod.validate(p)
        p=profile();p['bindings']['home']='Escape'
        with self.assertRaises(ValueError):mod.validate(p)

    def test_pointer_has_deadzone_and_bounded_dt(self):
        m=self.mapper()
        self.assertEqual(m.frame(sample(sticks=(0,0,.15,-.15)),True,.02)['motion'],[0,0])
        self.assertEqual(m.frame(sample(sticks=(0,0,1,-1)),True,100)['motion'],[30,-30])

    def guard(self, close=True, focus=True):
        parent,child=socket.socketpair();sink=Sink();out=mod.HeldOutput(sink);restored=[];errors=[]
        def worker():
            try:mod.mapped_guard(child,out,lambda:focus,lambda:restored.append(True),timeout=.07)
            except Exception as e:errors.append(e)
            finally:child.close()
        thread=threading.Thread(target=worker);thread.start();self.assertEqual(parent.recv(1),b'R')
        packet=(json.dumps({'held':['Ctrl','MouseLeft'],'motion':[0,0]})+'\n').encode()
        parent.sendall(packet[:9]);parent.sendall(packet[9:])
        if close:parent.shutdown(socket.SHUT_WR)
        thread.join(timeout=1);parent.close()
        self.assertFalse(thread.is_alive());self.assertFalse(errors)
        self.assertFalse(sink.down);self.assertEqual(restored,[True])
        return sink

    def test_watchdog_parent_eof_releases_outputs(self):
        sink=self.guard();self.assertIn(('Ctrl',True),sink.events);self.assertIn(('Ctrl',False),sink.events)

    def test_watchdog_stall_releases_outputs(self): self.guard(close=False)
    def test_watchdog_refuses_foreign_focus(self):
        sink=self.guard(focus=False);self.assertNotIn(('Ctrl',True),sink.events)


if __name__=='__main__':unittest.main()
