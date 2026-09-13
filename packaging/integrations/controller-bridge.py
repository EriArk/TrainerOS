#!/usr/bin/python3
"""Controller exit and optional DS pointer for owned X11 Adventures.

Never kills the emulator. Close requests and pointer events apply only while
an X11 window belonging to this invocation is the active window.
"""
import ctypes as c, os, subprocess, sys, time

args=sys.argv[1:];pointer=False
if args and args[0]=='--pointer':pointer=True;args.pop(0)
if args and args[0]=='--':args.pop(0)
if not args or not os.path.isabs(args[0]):raise SystemExit(2)
env=dict(os.environ);env['QT_QPA_PLATFORM']='xcb';env['GDK_BACKEND']='x11'
env.pop('WAYLAND_DISPLAY',None)
child=None
try:
 s=c.CDLL('libSDL2-2.0.so.0');x=c.CDLL('libX11.so.6');xt=c.CDLL('libXtst.so.6')
 s.SDL_SetHint.argtypes=[c.c_char_p,c.c_char_p];s.SDL_SetHint(b'SDL_NO_SIGNAL_HANDLERS',b'1');s.SDL_SetHint(b'SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS',b'1')
 s.SDL_InitSubSystem.argtypes=[c.c_uint]
 if s.SDL_InitSubSystem(0x2000)!=0:raise RuntimeError('Controller subsystem unavailable')
 s.SDL_GameControllerOpen.argtypes=[c.c_int];s.SDL_GameControllerOpen.restype=c.c_void_p
 s.SDL_GameControllerGetAttached.argtypes=[c.c_void_p];s.SDL_GameControllerClose.argtypes=[c.c_void_p]
 s.SDL_GameControllerGetButton.argtypes=[c.c_void_p,c.c_int];s.SDL_GameControllerGetButton.restype=c.c_ubyte
 s.SDL_GameControllerGetAxis.argtypes=[c.c_void_p,c.c_int];s.SDL_GameControllerGetAxis.restype=c.c_int16
 s.SDL_FlushEvents.argtypes=[c.c_uint,c.c_uint]
 x.XOpenDisplay.argtypes=[c.c_char_p];x.XOpenDisplay.restype=c.c_void_p;display=x.XOpenDisplay(None)
 if not display:raise RuntimeError('No X11 display')
 # A window can disappear between reading its identity and sending a request.
 # Ignore that X11 race; it must not terminate the supervising process.
 error_type=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_void_p)
 error_handler=error_type(lambda display,event:0)
 x.XSetErrorHandler.argtypes=[error_type];x.XSetErrorHandler(error_handler)
 x.XDefaultRootWindow.argtypes=[c.c_void_p];x.XDefaultRootWindow.restype=c.c_ulong;root=x.XDefaultRootWindow(display)
 x.XInternAtom.argtypes=[c.c_void_p,c.c_char_p,c.c_int];x.XInternAtom.restype=c.c_ulong
 def atom(name):return x.XInternAtom(display,name.encode(),False)
 x.XGetWindowProperty.argtypes=[c.c_void_p,c.c_ulong,c.c_ulong,c.c_long,c.c_long,c.c_int,c.c_ulong,c.POINTER(c.c_ulong),c.POINTER(c.c_int),c.POINTER(c.c_ulong),c.POINTER(c.c_ulong),c.POINTER(c.c_void_p)]
 x.XFree.argtypes=[c.c_void_p];x.XFlush.argtypes=[c.c_void_p]
 def prop(window,name):
  actual=c.c_ulong();fmt=c.c_int();count=c.c_ulong();left=c.c_ulong();data=c.c_void_p()
  status=x.XGetWindowProperty(display,window,atom(name),0,1,False,0,c.byref(actual),c.byref(fmt),c.byref(count),c.byref(left),c.byref(data))
  try:return c.cast(data,c.POINTER(c.c_ulong))[0] if status==0 and count.value and fmt.value==32 else 0
  finally:
   if data:x.XFree(data)
 def owns(pid):
  for _ in range(20):
   if pid==child.pid:return True
   if pid<=1:return False
   try:
    with open(f'/proc/{pid}/stat') as f:pid=int(f.read().rsplit(')',1)[1].split()[1])
   except (OSError,ValueError,IndexError):return False
  return False
 class Data(c.Union):_fields_=[('b',c.c_char*20),('s',c.c_short*10),('l',c.c_long*5)]
 class Message(c.Structure):_fields_=[('type',c.c_int),('serial',c.c_ulong),('send_event',c.c_int),('display',c.c_void_p),('window',c.c_ulong),('message_type',c.c_ulong),('format',c.c_int),('data',Data)]
 class Event(c.Union):_fields_=[('client',Message),('pad',c.c_long*24)]
 x.XSendEvent.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_long,c.POINTER(Event)]
 def close(window):
  event=Event();event.client=Message(33,0,True,display,window,atom('_NET_CLOSE_WINDOW'),32,Data())
  event.client.data.l[0]=0;event.client.data.l[1]=2
  x.XSendEvent(display,root,False,(1<<20)|(1<<19),c.byref(event));x.XFlush(display)
 xt.XTestFakeRelativeMotionEvent.argtypes=[c.c_void_p,c.c_int,c.c_int,c.c_ulong]
 xt.XTestFakeButtonEvent.argtypes=[c.c_void_p,c.c_uint,c.c_int,c.c_ulong]
 # Initialize the required bridge before starting the game, so a missing
 # library/display cannot strand the user without controller exit controls.
 child=subprocess.Popen(args,env=env)
 controller=None;pressed=False;previousChord=False;lastDiscovery=0
 while child.poll() is None:
  s.SDL_GameControllerUpdate();s.SDL_FlushEvents(0x600,0x65f)
  if controller and not s.SDL_GameControllerGetAttached(controller):s.SDL_GameControllerClose(controller);controller=None
  if not controller and time.monotonic()-lastDiscovery>1:
   lastDiscovery=time.monotonic()
   for i in range(s.SDL_NumJoysticks()):
    if s.SDL_IsGameController(i):controller=s.SDL_GameControllerOpen(i);break
  active=prop(root,'_NET_ACTIVE_WINDOW');owned=active and owns(prop(active,'_NET_WM_PID'))
  chord=bool(controller and s.SDL_GameControllerGetButton(controller,4) and s.SDL_GameControllerGetButton(controller,6))
  if owned and chord and not previousChord:close(active)
  previousChord=chord
  click=bool(pointer and owned and controller and s.SDL_GameControllerGetAxis(controller,5)>16000)
  if click!=pressed:xt.XTestFakeButtonEvent(display,1,click,0);pressed=click
  if pointer and owned and controller:
   def motion(axis):
    value=s.SDL_GameControllerGetAxis(controller,axis)/32768
    return int((1 if value>0 else -1)*max(0,abs(value)-.2)/.8*14)
   dx,dy=motion(2),motion(3)
   if dx or dy:xt.XTestFakeRelativeMotionEvent(display,dx,dy,0)
  x.XFlush(display);time.sleep(.016)
 if pressed:xt.XTestFakeButtonEvent(display,1,False,0);x.XFlush(display)
 if controller:s.SDL_GameControllerClose(controller)
 s.SDL_QuitSubSystem(0x2000)
except Exception as error:
 print('Controller helper unavailable:',error,file=sys.stderr)
 # Never terminate a running emulator or interrupt a save if the bridge fails.
raise SystemExit(child.wait() if child is not None else 2)
