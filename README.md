## X4Daemon

X4Daemon comes with absolutely no warranty.
A daemon that makes it possible to use all special keys on your Sidewinder X4 keyboard.
It is licensed under the GPL (General Public License), version 2 or later.
Details should be in the src package or at http://www.gnu.org/licenses/licenses.

It is not possible to switch banks or to record macros.

Depending on your windowmanager you should be able to define keys somewhere, 
I use S1-S6 and Bank_Switch for switching between my virtual desktops in AwesomeWM, 
which is very nice. I didn't test this in Gnome or KDE, but it should work fine. 
You can check if X4Daemon works correctly with xev. 

I made this package for Archlinux, I tested it on debian, but I didn't test it on any other distros.

Special thanks go to tolga9009 for posting the keycodes on ubuntu forums and on libg15 forums.

### Usage
```
x4daemon [OPTION]...
   -d,   --debug        Activate debug mode. 
   -D,   --daemon     Activate daemon mode, alle messages go to syslog.
   -h,   --help           Show this help page.
   -r,   --reset          Reset device when starting (shouldn't be necessary).
   -v,   --version      Show version.
   -w,   --waiting      Wait for device at start and if it disconnects. 
```

### Standard assignment of keys
```
Key:               Keycode(input.h):     Keycode:       Keycode(XF86):
S1                  KEY_PROG1             148           XF86Launch1
S2                  KEY_PROG2             149           XF86Launch2
S3                  KEY_WWW               150           XF86WWW
S4                  KEY_MAIL              155           XF86Mail
S5                  KEY_COMPUTER          157           XF86MyComputer
S6                  KEY_PHONE             169           XF86Phone
Bank_Switch         KEY_MEDIA             226           XF86AudioMedia
Record              KEY_RECORD            167           XF86AudioRecord
Play_Pause          KEY_PLAYPAUSE         164           XF86AudioPlay
Previous            KEY_PREVIOUSSONG      165           XF86AudioPrev
Next                KEY_NEXTSONG          163           XF86AudioNext
Mute                KEY_MUTE              113           XF86AudioMute
VolumeUp            KEY_VOLUMEUP          115           XF86AudioRaiseVolume
VolumeDown          KEY_VOLUMEDOWN        114           XF86AudioLowerVolume
Calculator          KEY_CALC              140           XF86Calculator
```

You can change the keys in source code, see keys[] array.
(I just used some unused keycodes for the special keys.)
Mute,Prev,Next,PlayPause,VolDown, VolUP and Calc use the same keycodes as before.
See /usr/include/linux/input.h for more information.
If you find any bugs or if you want to tell me something: development AT geekparadise.de.

### Known Problems
* It works fine on virtualbox (stop X4Daemon on the host before trying!) until you want to end X4Daemon. 
X4Daemon then can't release the interface correctly until you disconnect the usb device in virtualbox's 
menu (X4Daemon will hang until you do this). This is not caused by X4Daemon (I think), 
it seem to be some problem with virtualbox's usb drivers.
