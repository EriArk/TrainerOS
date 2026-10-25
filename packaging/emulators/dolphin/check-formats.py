"""Synthetic WAD/TGC/split-ISO checks against the packaged native Dolphin tool.
Usage: python check-formats.py [qemu-aarch64 -L SYSROOT] dolphin-tool
"""
import pathlib,struct,subprocess,json,sys,tempfile
temporary=tempfile.TemporaryDirectory();root=pathlib.Path(temporary.name)
command=sys.argv[1:]
if not command:raise SystemExit(__doc__)
def wad(title):
 b=bytearray(0x540)
 struct.pack_into('>IHHIIIIII',b,0,0x20,0x4973,0,0,0,0x2a4,0x208,0,0)
 struct.pack_into('>I',b,0x40,0x10001)
 struct.pack_into('>Q',b,0x40+0x1dc,title)
 struct.pack_into('>I',b,0x300,0x10001)
 struct.pack_into('>Q',b,0x300+0x18c,title)
 struct.pack_into('>H',b,0x300+0x1de,1)
 return b
for name,title,expected in [('channel',0x0001000154455354,'wii'),('ios',0x000000010000003a,'other')]:
 p=root/(name+'.wad');p.write_bytes(wad(title))
 out=subprocess.run(command+['header','-j','-i',str(p)],capture_output=True,timeout=20)
 assert out.returncode==0,(name,out.stderr)
 value=json.loads(out.stdout);print(name,value.get('platform'),value.get('game_id'));assert value['platform']==expected

iso=bytearray(65536);iso[:6]=b'GTEST0';struct.pack_into('>I',iso,0x1c,0xc2339f3d)
tgc=bytearray(32768);struct.pack_into('>I',tgc,0,0xae0f38a2);struct.pack_into('>I',tgc,8,len(tgc))
(root/'native.tgc').write_bytes(tgc+iso)
(root/'native.part0.iso').write_bytes(iso[:32768]);(root/'native.part1.iso').write_bytes(iso[32768:])
for name in ('native.tgc','native.part0.iso'):
 out=subprocess.run(command+['header','-j','-i',str(root/name)],capture_output=True,timeout=20)
 assert out.returncode==0,(name,out.stderr)
 value=json.loads(out.stdout);assert value['platform']=='gc',(name,value)
 print(name,value['platform'])
print('4 native Dolphin format cases passed')
