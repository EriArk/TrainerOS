"""No devices or real key injection; exercise shortcut and output boundaries."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'packaging/integrations'))
from keyboard_input import HomeChord, KeyboardNavigation, KeyboardOutput


class Shortcuts(unittest.TestCase):
    def test_failed_key_down_still_releases_the_chord(self):
        events=[]
        class Sink:
            codes={'Ctrl':1,'A':2}
            def button(self,key,down):
                events.append((key,down))
                if key=='A' and down:raise RuntimeError('output failed')
        out=KeyboardOutput.__new__(KeyboardOutput);out.owns_focus=lambda:True;out.sink=Sink()
        with self.assertRaises(RuntimeError):out.tap('A',['Ctrl'])
        self.assertEqual(events,[('Ctrl',True),('A',True),('A',False),('Ctrl',False)])
    def test_home_release_and_chord_hold(self):
        c=HomeChord()
        self.assertEqual(c.update(True,False),'')
        self.assertEqual(c.update(False,False,True),'home')
        self.assertEqual(c.update(True,True),'keyboard')
        for _ in range(8):self.assertEqual(c.update(True,True),'')
        self.assertEqual(c.update(False,True),'')
        self.assertEqual(c.update(True,True),'')
        self.assertEqual(c.update(False,False,True),'')
        self.assertEqual(c.update(True,True),'keyboard')

    def test_keyboard_opens_after_neutral_not_as_first_character(self):
        n=KeyboardNavigation()
        self.assertEqual(n.action(dict(confirm=True)),'')
        self.assertEqual(n.action(dict(neutral=True)),'')
        self.assertEqual(n.action(dict(confirm=True)),'confirm')
        self.assertEqual(n.action(dict(confirm=True)),'')
        self.assertEqual(n.action(dict(neutral=True)),'')
        self.assertEqual(n.action(dict(back=True)),'back')

    def test_packets_never_inject_to_foreign_focus(self):
        out=KeyboardOutput.__new__(KeyboardOutput);out.owns_focus=lambda:False
        out.tap=lambda *a: self.fail('foreign key')
        out.text=lambda *a: self.fail('foreign text')
        for packet in ({'text':'secret'},{'action':'paste'},{'key':65,'modifiers':0x04000000}):out.apply(packet)

    def test_commands_modifiers_and_text_are_distinct(self):
        out=KeyboardOutput.__new__(KeyboardOutput);out.owns_focus=lambda:True
        events=[];out.tap=lambda *a:events.append(('key',a));out.text=lambda value:events.append(('text',value))
        out.apply({'text':'Привет'})
        out.apply({'key':65,'modifiers':0x04000000,'text':'a'})
        out.apply({'key':65,'modifiers':0x02000000,'text':'A'})
        out.apply({'action':'paste'})
        self.assertEqual(events,[('text','Привет'),('key',('A',['Ctrl'])),('text','A'),('key',('V',['Ctrl']))])
        events.clear();out.apply({'key':65,'modifiers':0x10000000});out.apply({'action':'unknown'})
        self.assertEqual(events,[('key',('A',['Meta']))])


if __name__=='__main__':unittest.main()
