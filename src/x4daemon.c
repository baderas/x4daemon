/*     x4daemon - A daemon that makes it possible to use all special keys on your Sidewinder X4 keyboard.
 *     Copyright (C) 2012 Andreas Bader
 *     If you find any bugs or if you want to tell me something: development AT geekparadise.de
 *      
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License
 *     as published by the Free Software Foundation; either version 2
 *     of the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <config.h>

#define _version "0.4.4"

// Keys for {S1,S2,S3,
//               S4,S5,S6,Bank_Switch
//               Record,Play_Pause,Prev,
//               Next,Mute,Vol_Up,
//               Vol_Down, Calculator}
// Change to your own values if you like to, possible values
// are found in /usr/include/linux/input.h.
// You can also use integer values like 113 (stands for KEY_MUTE)
int keys[15] = {KEY_PROG1,KEY_PROG2,KEY_WWW,
                        KEY_MAIL,KEY_COMPUTER,KEY_PHONE,KEY_MEDIA,
                        KEY_RECORD,KEY_PLAYPAUSE,KEY_PREVIOUSSONG,
                        KEY_NEXTSONG, KEY_MUTE,KEY_VOLUMEUP,
                        KEY_VOLUMEDOWN,KEY_CALC};

// Numbers have to be converted from hex to integer
// Exmplae: For calculator button you get when pressing&releasing:
// 01|92|01|00|00|00|00|00
// Take all numbers and convert them to integer:
// First and thrird number stay 1
// Second number (92) -> 146
// Now you got the correct entry:
// unsigned int calc[8] = {1,146,1,0,0,0,0,0};
// For S1-6 the second received value is 80000 (release)
// For the other buttons the second received value is 10000000 (release)
unsigned int s1[8] = {8,1,0,0,0,0,0,0};
unsigned int s2[8] = {8,2,0,0,0,0,0,0};
unsigned int s3[8] = {8,4,0,0,0,0,0,0};
unsigned int s4[8] = {8,8,0,0,0,0,0,0};
unsigned int s5[8] = {8,16,0,0,0,0,0,0};
unsigned int s6[8] = {8,32,0,0,0,0,0,0};
unsigned int s_release[8] = {8,0,0,0,0,0,0,0}; // not used
unsigned int bank_switch[8] = {1,0,0,0,0,0,20,0};
unsigned int record[8] = {1,0,0,0,0,0,17,0};
unsigned int play_pause[8] = {1,205,0,0,0,0,0,0};
unsigned int prev[8] = {1,182,0,0,0,0,0,0};
unsigned int next[8] = {1,181,0,0,0,0,0,0};
unsigned int mute[8] = {1,226,0,0,0,0,0,0};
unsigned int vol_up[8] = {1,233,0,0,0,0,0,0};
unsigned int vol_down[8] = {1,234,0,0,0,0,0,0};
unsigned int calc[8] = {1,146,1,0,0,0,0,0};
unsigned int other_release[8] = {1,0,0,0,0,0,0,0}; // not used

// uinput variables
static int  fd = -1;
struct uinput_user_dev uidev;
struct input_event ev; 

// libusb variables
struct libusb_context *luc;
struct libusb_device_handle *ludh;
struct libusb_device *lud = NULL;
libusb_device **list;
struct libusb_transfer *lt;
uint8_t adress;
unsigned char *buffer;
int length = 16;
unsigned int timeout = 4294967295u;
unsigned char endpoint_adr = 0x83;

// other variables
bool detached = false; // if kernel driver was detached
bool debug = false; // if debug was chosen
bool reset = false; // if device reset was chosen
bool daemonize = false; // if daemon mode was chosen
bool waiting = false; // if waiting mode was chosen -> that means that we will not abort when device 
// is disconnected. Instead we poll for it every $waiting_time seconds,
// even if it is missing at start
int waiting_time = 10;
int ret; // variable for return values
int j; // some variable for loops
pid_t pid,sid;
int max_tries=3; // how often probe for automatic loaded uinput module to show up

// Prints message to syslog or stdout 
// as Warning if warning is true && debug is true
// as Error if warning is false && error is true
// as Debug if debug is true
// as normal message if debug,warning,error all are false
// don't print message in daemon mode when daemon is false
void output(char *message, bool warning, bool error, bool debug_, bool daemon)
{
        if (!daemonize)
        {
                // in normal mode print warnings/errors/infos
                // in debug mode print also debug infos
                if (warning)
                {
                        printf("Warning: %s\n", message);
                }
                else if (error)
                {
                        printf("Error: %s\n", message);
                }
                else if (debug_)
                {
                        if (debug)
                                printf("Debug: %s\n", message);
                }
                else
                {
                        printf("%s\n", message);
                }
        }
        else // syslog is already open in this case
        {
                // In daemon mode only log errors, no warnings/debug.
                // if debug is activated, also warning/debug are logged.
                if (daemon)
                {
                        if (warning)
                        {
                                if (debug)
                                        syslog(LOG_WARNING, "%s", message);
                        }
                        else if (error)
                        {
                                syslog(LOG_ERR, "%s", message);
                        }
                        else if (debug_)
                        {
                                if (debug)
                                        syslog(LOG_DEBUG, "%s", message);
                        }
                        else
                        {
                                syslog(LOG_INFO, "%s", message);
                        }
                }
        }
}

// Called while quitting because of errors, sigint(strg+c),sigterm/sigkill/sighup.
void end()
{
        if (fd != -1 && ioctl(fd, UI_DEV_DESTROY) < 0)
                output("Can't close uinput device.", true, false, false, true);
        close(fd);  
        if (lt)
                libusb_cancel_transfer(lt);
        if (lt)
                libusb_free_transfer(lt);
        if (ludh)
                libusb_clear_halt(ludh, endpoint_adr);
        if (ludh)
                ret=libusb_release_interface(ludh, 1);   
        if (ret)
                output("Can't release interface.", true, false, false, true);           
        if (detached)
        {
                ret=libusb_attach_kernel_driver(ludh, 1);
                if (ret)
                        output("Can't reattach kernel driver.", true, false, false, true);
        }
        if (lud)
                libusb_unref_device(lud);
        if (ludh)
                free(ludh);
        if (luc)
                libusb_exit(luc);
        if (buffer)
                free(buffer);
        output("Goodbye.", false, false, false, false);
        (void) signal(SIGINT, SIG_DFL);
        exit(0);
}

// catches Strg-C and gives messages+calls end()
void catcher(int sig) {
        output("Strg-C received.", false, false, false, true);
        end();
}

bool check_args(int argc, char **argv){
        int j, i;
        bool check;
        char *possible_args[12] = {"-h","--help",
                                                 "-v","--version",
                                                 "-d","--debug",
                                                 "-D","--daemon",
                                                 "-r","--reset",
                                                 "-w","--waiting"};
        for (j = 1; j < argc; j++)
        {
                check = false;
                for (i = 0; i < (sizeof(possible_args) / sizeof(possible_args[0])); i++)
                {
                       if(strcmp(argv[j], possible_args[i]) == 0)
                       {
                                check = true;
                       }
                }
                if (!check)
                {
                        return false;
                }
        }
        return true;
        
}

// looks if module is not loaded and tries to load it
void init_module() {
        if (_MP && strcmp(_MP, "no") == 0)
        {
                output("Modprobe was not found.Install modprobe and recompile x4daemon.", false, true, false, true);
                end();
        }
        if (_LM && strcmp(_LM, "no") == 0)
        {
                output("Lsmod was not found.Install lsmod and recompile x4daemon.", false, true, false, true);
                end();
        }
        if (_LM && strcmp(_GREP, "no") == 0)
        {
                output("Grep was not found.Install grep and recompile x4daemon.", false, true, false, true);
                end();
        }
        char *sys_buffer = malloc(sizeof(_LM) + sizeof(_GREP) + sizeof(" | ") + sizeof(" uinput 1>/dev/null 2>/dev/null"));
        sprintf(sys_buffer, "%s%s%s%s", _LM, " | ", _GREP, " uinput 1>/dev/null 2>/dev/null");
        int ret = system(sys_buffer);
        free(sys_buffer);
        if (ret == 0)
        {
                output("Uinput module already loaded.", false, false, true, true);
        }
        else
        {
                output("Uinput module not loaded, trying to load it...", false, false, true, true);
                char *sys_buffer = malloc(sizeof(_MP) + sizeof(" uinput 1>/dev/null 2>/dev/null"));
                sprintf(sys_buffer, "%s%s", _MP, " uinput 1>/dev/null 2>/dev/null");
                ret = system(sys_buffer);
                free(sys_buffer);
                if (ret == 0)
                {
                        ///// some distris like debian need some wait after loading uinput module:
                        ret = 0;
                        output("Waiting for the uinput module to show up...",false,false,true,true);
                        while (ret != 0 && max_tries != 0)
                        {
                                char *sys_buffer = malloc(sizeof(_LM) + sizeof(_GREP) + sizeof(" | ") + sizeof(" uinput 1>/dev/null 2>/dev/null"));
                                sprintf(sys_buffer, "%s%s%s%s", _LM, " | ", _GREP, " uinput 1>/dev/null 2>/dev/null");
                                int ret = system(sys_buffer);
                                free(sys_buffer);
                                max_tries--;
                                if (ret!=0)
                                {
                                        sleep(1);
                                        output("Uinput module didn't show up, wait some more.",false,false,true,true);
                                }
                        }
                        if (ret != 0)
                        {
                                output("Uinput module could not be loaded. #1", false, true, false, true);
                                end();        
                        }
                        //////////////////////////////////////////////////////////////////////////////////////////
                        output("Uinput module successfully loaded.", false, false, true, true);
                        
                }
                else
                {
                        output("Uinput module could not be loaded. #2", false, true, false, true);
                        end();
                }
        }
        
}

void interpret_args(char *arg){
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
                unsigned char *helperr = malloc(2048 * sizeof(unsigned char));
                sprintf(helperr, "x4daemon %s.\n" \
                        "X4Daemon comes with absolutely no warranty.\n"\
                        "A daemon that makes it possible to use"\
                        " all special keys on your Sidewinder X4 keyboard.\n"\
                        "It is licensed under the GPL (General Public License), version 2 or later.\n" \
                        "Details should be in the src package or at http://www.gnu.org/licenses/licenses.\n\n" \
                        "Usage: x4daemon [OPTION]...\n"
                        "   -d, --debug\t\tActivate debug mode. \n" \
                        "   -D, --daemon\t\tActivate daemon mode, alle messages go to syslog.\n" \
                        "   -h, --help\t\tShow this help page.\n" \
                        "   -r, --reset\t\tReset device when starting (shouldn't be necessary).\n" \
                        "   -v, --version\tShow version.\n" \
                        "   -w, --waiting\tWait for device at start and if it disconnects. \n\n" \
                        "Standard assignment of keys is:\n" \
                        "Key:\t\tKeycode(input.h):\tKeycode:\tKeycode(XF86):\n"\
                        "S1\t\tKEY_PROG1\t\t148\t\tXF86Launch1\n" \
                        "S2\t\tKEY_PROG2\t\t149\t\tXF86Launch2\n" \
                        "S3\t\tKEY_WWW\t\t\t150\t\tXF86WWW\n" \
                        "S4\t\tKEY_MAIL\t\t155\t\tXF86Mail\n" \
                        "S5\t\tKEY_COMPUTER\t\t157\t\tXF86MyComputer\n" \
                        "S6\t\tKEY_PHONE\t\t169\t\tXF86Phone\n" \
                        "Bank_Switch\tKEY_MEDIA\t\t226\t\tXF86AudioMedia\n" \
                        "Record\t\tKEY_RECORD\t\t167\t\tXF86AudioRecord\n" \
                        "Play_Pause\tKEY_PLAYPAUSE\t\t164\t\tXF86AudioPlay\n" \
                        "Previous\tKEY_PREVIOUSSONG\t165\t\tXF86AudioPrev\n" \
                        "Next\t\tKEY_NEXTSONG\t\t163\t\tXF86AudioNext\n" \
                        "Mute\t\tKEY_MUTE\t\t113\t\tXF86AudioMute\n" \
                        "VolumeUp\tKEY_VOLUMEUP\t\t115\t\tXF86AudioRaiseVolume\n" \
                        "VolumeDown\tKEY_VOLUMEDOWN\t\t114\t\tXF86AudioLowerVolume\n" \
                        "Calculator\tKEY_CALC\t\t140\t\tXF86Calculator\n\n" \
                        "You can change the keys in source code, see keys[] array.\n" \
                        "(I just used some unused keycodes for the special keys.)\n" \
                        "Mute,Prev,Next,PlayPause,VolDown, VolUP and Calc " \
                        "use the same keycodes as before.\n" \
                        "See /usr/include/linux/input.h for more information.\n\n" \
                        "It is not possible to switch banks or to record macros.\n\n" \
                        "If you find any bugs or if you want to tell me something: development AT geekparadise.de.",
                        _version);
                output(helperr, false, false, false, false);
                free(helperr);
                end();
        }
        else if (!strcmp(arg, "-v") || !strcmp(arg, "--version"))
       {
                unsigned char *helper = malloc(512 * sizeof(unsigned char));
                sprintf(helper, "Version: %s", _version);
                output(helper, false, false, false, false);
                free(helper);
                end();
        }
        else if (!strcmp(arg, "-d") || !strcmp(arg, "--debug"))
        {
                output("Debug messages activated.", false, false, true, true);
                debug = true;
        }
        else if (!strcmp(arg, "-D") || !strcmp(arg, "--daemon"))
        {
                openlog ("X4Daemon", LOG_PID | LOG_NDELAY, LOG_DAEMON);
                daemonize = true;
        }
        else if (!strcmp(arg, "-r") || !strcmp(arg, "--reset"))
        {
                output("Reset device activated.", false, false, true, true);
                reset = true;
        }
        else if (!strcmp(arg, "-w") || !strcmp(arg, "--waiting"))
        {
                output("Wait for disconnected device activated.", false, false, true, true);
                waiting = true;
        }
}

// sends a keycode to the uinput device
void send_keycode(int key) {
        struct input_event ev;
        unsigned char *helper = malloc(128 * sizeof(unsigned char));
        sprintf(helper, "Pressing %i...", key);
        output(helper, false, false, true, true);
        free(helper);
        memset(&ev, 0, sizeof(struct input_event));
        ev.type = EV_KEY;
        ev.code = key;
        ev.value = 1;
        int ret = write(fd, &ev, sizeof(struct input_event));
        if(ret < 0)
        {
                output("Can't write to uinput device #2.", false, true, false, true);
                end();
        }     
        memset(&ev, 0, sizeof(struct input_event));
        ev.type = EV_KEY;
        ev.code = key;
        ev.value = 0;
        if(write(fd, &ev, sizeof(struct input_event)) < 0)
        {       
                output("Can't write to uinput device #3.", false, true, false, true);
                end();
        }
        memset(&ev, 0, sizeof(struct input_event));
        ev.type = EV_SYN;
        ev.code = 0;
        ev.value = 0;
        if(write(fd, &ev, sizeof(struct input_event)) < 0)
        {
                output("Can't write to uinput device #4.",false,true,false,true);
                end();
        }
}

// initalizes the uinput device
void init_uinput() {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if(fd < 0)
        {
                fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
                if(fd < 0)
                {
                        output("Can't open uinput device (/dev/uinput || /dev/input/uinput).", false, true, false, true);
                        end();
                }
        }
        if(ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
        {
                output("While doing some ioctl stuff #1.", false, true, false, true);
                end();
        }
        int i;
        for (i = 0; i < (sizeof(keys) / sizeof(int)); i++)
        {
                if(ioctl(fd, UI_SET_KEYBIT, keys[i]) < 0)
                {
                        unsigned char *helper = malloc(128 * sizeof(unsigned char));
                        sprintf(helper,"While doing ioctl on key: %i.", keys[i]);
                        output(helper, false, true, false, true);
                        free(helper);
                        end();
                }
        }
        memset(&uidev, 0, sizeof(uidev));
        snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "x4daemon");
        uidev.id.bustype = BUS_USB;
        uidev.id.vendor  = 0x1;
        uidev.id.product = 0x1;
        uidev.id.version = 1;
        if(write(fd, &uidev, sizeof(uidev)) < 0)
        {
                output("Can't write to uinput device #1.", false, true, false, true);
                end();
        }
        if(ioctl(fd, UI_DEV_CREATE) < 0)
        {
                output("While doing some ioctl stuff #2.", false, true, false, true);
                end();
        }
        sleep(2);
}

// compares a given X4 keycode with one given (from s1[] or s2[] or ... etc.)
bool compare(unsigned char *comp1, unsigned int *comp2, int length)
{
        if (length == 0)
                return false;
        bool gleich = true;
        int j;
        for(j = 0; j < length; j++)
        {
                if ((int)comp1[j] != comp2[j])
                        gleich = false;
        }
        return gleich;
}

// decides which button was pressed
// and calls send_keycode with the correct keycode from keys[]
// release codes are ignored
void decide(unsigned char *buffer, int transferred)
{
        if (compare(buffer, s1, transferred))
       {
                output("Button pressed:S1", false, false, true, true);
                send_keycode(keys[0]);
        }
        else if (compare(buffer, s2, transferred))
        {
                output("Button pressed:S2", false, false, true, true);
                send_keycode(keys[1]);
        }
        else if (compare(buffer, s3, transferred))
        {
                output("Button pressed:S3", false, false, true, true);
                send_keycode(keys[2]);
        }
        else if (compare(buffer, s4, transferred))
        {
                output("Button pressed:S4", false, false, true, true);
                send_keycode(keys[3]);
        }
        else if (compare(buffer, s5, transferred))
        {
                output("Button pressed:S5", false, false, true, true);
                send_keycode(keys[4]);
        }
        else if (compare(buffer, s6, transferred))
        {
                output("Button pressed:S6", false, false, true, true);
                send_keycode(keys[5]);
        }
        else if (compare(buffer, bank_switch, transferred))
        {
                output("Button pressed:Bank_Switch", false, false, true, true);
                send_keycode(keys[6]);
        }
        else if (compare(buffer, record, transferred))
        {
                output("Button pressed:Record", false, false, true, true);
                send_keycode(keys[7]);
        }
        else if (compare(buffer, play_pause, transferred))
        {
                output("Button pressed:Play/Pause", false, false, true, true);
                send_keycode(keys[8]);
        }
        else if (compare(buffer, prev, transferred))
        {
                output("Button pressed:Previous", false, false, true, true);
                send_keycode(keys[9]);
        }
        else if (compare(buffer, next, transferred))
        {
                output("Button pressed:Next", false, false, true, true);
                send_keycode(keys[10]);
        }
        else if (compare(buffer, mute, transferred))
        {
                output("Button pressed:Mute", false, false, true, true);
                send_keycode(keys[11]);
        }
        else if (compare(buffer, vol_up, transferred))
        {
                output("Button pressed:Volume_Up", false, false, true, true);
                send_keycode(keys[12]);
        }
        else if (compare(buffer, vol_down, transferred))
        {
                output("Button pressed:Volume_Down", false, false, true, true);
                send_keycode(keys[13]);
        }
        else if (compare(buffer, calc, transferred))
        {
                output("Button pressed:Calculator", false, false, true, true);
                send_keycode(keys[14]);
        }
        else if (compare(buffer, s_release, transferred))
        {
                output("Button released: Sx", false, false, true, true);
        }
        else if (compare(buffer, other_release, transferred))
        {
                output("Button released: Other", false, false, true, true);
        }
        else
        {
                output("No known Button pressed.", true, false, false, true);
        }
}

// initalizes libusb
// ends when error on finding or opening device only if ending is true
bool init_libusb(bool ending)
{
        // init
        libusb_init(&luc);
        // search device
        ssize_t cnt = libusb_get_device_list(luc, &list);
        ssize_t i = 0;
        int err = 0;
        if (cnt < 0)
        {
                output("Can't get usb device list.", false, true, false, true);
                end();
        }
        output("Start searching our usb device...", false, false, true, true);
        for (i = 0; i < cnt; i++)
        {
                libusb_device *device = list[i];
                struct libusb_device_descriptor device_desc;
                ret = libusb_get_device_descriptor(device, &device_desc);
                if (ret < 0)
                {
                        output("Can't get device descriptor for some device(s).", true, false, false, true);
                        continue;
                }

                if (device_desc.idVendor == 0x045e && device_desc.idProduct == 0x0768)
                {
                        lud = libusb_ref_device(device);
                        unsigned char *helper = malloc(512 * sizeof(unsigned char));
                        sprintf(helper, "Found it! (Vendor:%x, Product:%x)", device_desc.idVendor, device_desc.idProduct);
                        output(helper, false, false, true, true);
                        free(helper);
                        break;
                }
        }
        // Free device list & free unused devices
        libusb_free_device_list(list, 1);
        if (!lud)
        {
                if (ending)
                {
                        output("No Sidewinder X4 device found.", false, true, false, true);
                        end();
                }
                else
                {
                        return false;
                }
        }
        ret = libusb_open(lud, &ludh);
        if (ret)
        {
                if (ending)
                {
                        output("Can't open the device.", false, true, false, true);
                        end();
                }
                else
                {
                        output("Can't reopen the device, sleeping some more....", true, false, false, true);
                        return false;
                }
        }

        if (reset)
        {
                ret=libusb_reset_device(ludh);
                if (ret)
                {
                        output("Couldn't reset the device.", true, false, false, true);
                }
        }

        // Detach kernel driver if used, save that it was detached
        if (libusb_kernel_driver_active(ludh, 1))
        {
                ret = libusb_detach_kernel_driver(ludh, 1);
                if (ret)
                {
                        output("Can't detach kernel driver.", false, true, false, true);
                        end;
                }
                detached = true;
                output("Sucessfully detached kernel driver.", false, false, true, true);
        }

        // Claim Interface
        ret = libusb_claim_interface(ludh, 1);
        if (ret)
        {
                output("Can't claim the interface.", false, true, false, true);
                end();
        }  
        else
        {
                output("Sucessfully claimed interface..", false, false, true, true);
                return true;
        }
}

// Prototype
void waiting_init();

// interrupt call for uinput interrupt 
// called when a button is pressed
void interrupt_call(struct libusb_transfer *lt)
{
        decide(lt->buffer, lt->actual_length);

        // for Debugging and conversion of the numbers
        unsigned char *helper = malloc(512 * sizeof(unsigned char));
        unsigned char *helper2 = malloc(16 * sizeof(unsigned char));

        strcpy(helper, "Received the following: ");
        for(j = 0; j < lt->actual_length; j++)
        {
                if (strlen(helper) < (512 - 24)) //512-"Received the following: "
                {
                        sprintf(helper2, "%02x ", (int)lt->buffer[j]);
                        strcat(helper, helper2);
                }
        }
        output(helper, false, false, true, true);
        free(helper);
        free(helper2);
        int ret;
        ret = libusb_submit_transfer(lt);
        if(ret)
        {
                output("Can't resubmit libsusb transfer. Device disconnected? ", false, true, false, true);
                if (!waiting)
                {
                        end();
                }
                else
                {
                        waiting_init();
                }
        }
}

// Waits and tries every $waiting_time (global) seoncds to
// restart complete initialisation via libusb
void waiting_init()
{
        bool found = false;
        if (ludh)
                libusb_clear_halt(ludh,endpoint_adr);
        if (ludh)
                libusb_release_interface(ludh,1); 
        if (lud)
                libusb_unref_device(lud);
        while (!found)
        {      if (luc)
                        libusb_exit(luc);
                output("Trying to reinitalize the device...", false, false, true, true);
                if (!init_libusb(false))
                {
                        sleep(2);
                }
                else
                {
                        // setup asynchronous interrupt
                        adress = libusb_get_device_address(lud);
                        lt = libusb_alloc_transfer(0);
                        libusb_fill_interrupt_transfer(lt, ludh, endpoint_adr, buffer, length, &interrupt_call, NULL, timeout);
                        ret = libusb_submit_transfer(lt);
                        if(ret)
                        {
                                output("Can't submit libusb interrupt transfer.#2", false, true, false, true);
                                end();
                        }
                        else
                        {
                                output("Successfully reinitialized the device and resubmited a interrupt transfer.", false, false, true, true);
                                found = true;
                        }
                }
        }
}

int main (int argc, char **argv)
{
        // Handle args 
        signal(SIGINT, catcher);
        signal(SIGHUP, catcher);
        signal(SIGTERM, catcher);
        signal(SIGKILL, catcher);

        // Check & Interpret Args
                
        if (!check_args(argc, argv))
        {
                output("Unknown argument.", false, true, false, true);
                end();
        }

        for (j = 1; j < argc; j++)
        {
                interpret_args(argv[j]);
        }

        output("Starting...!", false, false, false, false);
        // init uinput
        init_module();
        init_uinput();
        output("Uinput initalized...", false, false, true, true);

        // Init libusb

        // beffer before init, because waiting_init() does under some settings the interrupt setup
        buffer = malloc(length * sizeof(unsigned char));

        if(waiting)
        {
                output("Waiting for device...", false, false, true, true);
                waiting_init();
        }
        else
        {
                init_libusb(true);
                output("Libusb initalized...!", false, false, true, true);

                // setup asynchronous interrupt
                adress = libusb_get_device_address(lud);

                lt=libusb_alloc_transfer(0);

                libusb_fill_interrupt_transfer(lt, ludh, endpoint_adr, buffer, length, &interrupt_call, NULL, timeout);

                ret=libusb_submit_transfer(lt);

                if(ret)
                {
                        output("Can't submit libusb interrupt transfer.#1", false, true, false, true);
                        end();
                }
        }

        // Go to Background if -D or --Daemon by user
        if (daemonize)
        {
                pid = fork();

                if (pid < 0)
                {
                       output("Can't fork into background #1.", false, true, false, true);
                       end(); 
                }        
                else if (pid > 0)
                {
                        exit(0);
                }
                umask(0);
                sid = setsid();
                if (sid < 0)
                {
                        output("Can't fork into background #2.", false, true, false, true);
                        end(); 
                }
                if (chdir("/") < 0)
                {
                    output("Can't fork into background #2.", false, true, false, true);
                    end(); 
                }
                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
        }
        // Call libusb event handle routine
        while (1)
        {
                libusb_handle_events(luc); //blocking, no sleep() needed
        }
}
