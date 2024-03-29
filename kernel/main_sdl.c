#include <stdlib.h>

#include <SDL2/SDL.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// emulated interfaces
#define draw_buffer __draw_buffer // prevents defining draw buffers to pixel_t
#include "bitbox.h"
#undef draw_buffer

#define DIR NIX_DIR // prevents name clashes with datfs DIR
#include <dirent.h>
#undef DIR

#include "fatfs/ff.h"

// ticks in ms
#define TICK_INTERVAL 1000/60
#define USER_BUTTON_KEY SDLK_F12

#define KBR_MAX_NBR_PRESSED 6
#define RESYNC_TIME_MS 2000 // dont try to sync back if delay if more than

#define WM_TITLE_LED_ON  "Bitbox emulator (*)"
#define WM_TITLE_LED_OFF "Bitbox emulator"

#define THRESHOLD_HAT 30

/*
   TODO

 handle FULLSCREEN (alt-enter) as keyboard handles
 handling other events (plugged, ...)

 game recordings

*/


// emulated screen size, possibly displayed larger
int screen_width=0;
int screen_height=0;

#define LINE_BUFFER 1024
#ifndef LINE_MARGIN
#define LINE_MARGIN 64
#endif

// options
static int slow; // parameter : run slower ?
static int fullscreen; // shall run fullscreen
static int quiet=1; // quiet by default now
static int nodisplay=0; // no display / graphics
static int nosound=0; // mute all
static int scale=VGA_V_PIXELS<400 ? 2 : 1; // scale display by this in pixels

// Video
SDL_Window* emu_window;
SDL_Renderer* emu_renderer;
SDL_Texture* emu_texture;
uint8_t mybuffer1[LINE_BUFFER];
uint8_t mybuffer2[LINE_BUFFER];
uint8_t *draw_buffer = mybuffer1+LINE_MARGIN; // volatile ?

volatile uint16_t gamepad_buttons[2];
uint16_t kbd_gamepad_buttons;
uint16_t sdl_gamepad_buttons[2]; // real gamepad read from SDL
bool sdl_gamepad_from_axis[2]; // emulate buttons from gamepad ?

uint32_t vga_line=0;
volatile uint32_t vga_frame=0;
#ifdef VGA_SKIPLINE
volatile int vga_odd;
#endif

// IO
volatile int8_t mouse_x, mouse_y;
volatile uint8_t mouse_buttons;

int user_button=0;

// joystick handling.
static const int gamepad_max_buttons = 12;
static const int gamepad_max_pads = 2;

volatile int8_t gamepad_x[2], gamepad_y[2]; // analog pad values

volatile uint8_t keyboard_mod[2]; // LCtrl =1, LShift=2, LAlt=4, LWin - Rctrl, ...
volatile uint8_t keyboard_key[2][KBR_MAX_NBR_PRESSED]; // using raw USB key codes

#if VGA_MODE != NONE
uint32_t vga_palette32[256]; // 32 bits palette
extern uint8_t micro_palette[256*3];

void set_palette_colors(const uint8_t *rgb, int start, int len) {
    for (int i=start;i<start+len;i++) {        
        uint8_t r = *rgb++;
        uint8_t g = *rgb++;
        uint8_t b = *rgb++;
        vga_palette32[i] = 0xff<<24 | b<<16 | g<<8 | r; // RGBA
    }
}

void __attribute__((weak)) graph_vsync() {} // default empty

// emulate vsync
static void __attribute__ ((optimize("-O3"))) vsync_screen ()
{
    for (;vga_line<screen_height+VGA_V_SYNC;vga_line++) {
    #ifdef VGA_SKIPLINE
        vga_odd=0;
        graph_vsync(); // using line, updating draw_buffer ...
        vga_odd=1;
        graph_vsync(); //  a second time for SKIPLINE modes
    #else
        graph_vsync(); // once
    #endif
    }
}

static void __attribute__ ((optimize("-O3"))) update_texture (SDL_Texture *scr)
// uses global line + vga_odd
{

    draw_buffer = mybuffer1+LINE_MARGIN; // currently 16bit data

    uint32_t *pixels;
    int pitch; 
    SDL_LockTexture(emu_texture, 0, (void**)&pixels, &pitch);

    for (vga_line=0;vga_line<screen_height;vga_line++) {
        #ifdef VGA_SKIPLINE
            vga_odd=0;
            graph_line(); // using line, updating draw_buffer ...
            vga_odd=1;
            graph_line(); //  a second time for SKIPLINE modes
        #else
            graph_line();
        #endif
        // copy to screen at this position
        uint8_t *restrict src = draw_buffer;
        uint32_t *restrict dst = pixels + pitch*vga_line/sizeof(uint32_t); // pitch is in bytes
        for (int i=0;i<screen_width;i++) {
            *dst++ = vga_palette32[*src++];
        }

        // swap lines buffers to simulate double line buffering
        draw_buffer = ( draw_buffer == &mybuffer1[LINE_MARGIN] ) ? &mybuffer2[LINE_MARGIN] : &mybuffer1[LINE_MARGIN];
    }
    
    SDL_UnlockTexture(emu_texture);
}
#else
#warning VGA_MODE SET TO NONE
#endif


#ifndef NO_AUDIO
static uint16_t bitbox_sound_buffer[BITBOX_SNDBUF_LEN];
int bitbox_sound_idx = BITBOX_SNDBUF_LEN;

static void __attribute__ ((optimize("-O3"))) mixaudio(void * userdata, uint8_t * stream, int len)
// this callback is called each time we need to fill the buffer
{
    int i=0;
    uint16_t *dst = (uint16_t *)stream;
    len /= 2;
    while (i < len) {
        if (bitbox_sound_idx == BITBOX_SNDBUF_LEN) {
            bitbox_sound_idx = 0;
            game_snd_buffer(bitbox_sound_buffer, BITBOX_SNDBUF_LEN);
        }
        while (bitbox_sound_idx < BITBOX_SNDBUF_LEN && i < len)
            dst[i++] = bitbox_sound_buffer[bitbox_sound_idx++];
    }
#ifdef __HAIKU__
	// On Haiku, U8 audio format is broken so we convert to signed
	for (i = 0; i < len * 2; i++)
		stream[i] -= 128;
#endif
}

void audio_init(void)
{
    SDL_AudioSpec desired;

    desired.freq = BITBOX_SAMPLERATE;
#ifdef __HAIKU__
    desired.format = AUDIO_S8;
#else
    desired.format = AUDIO_U8;
#endif
    desired.channels = 2;

    /* The audio buffers holds at least one vga_frame worth samples */
    desired.samples = BITBOX_SNDBUF_LEN;

    /* Setup callback and "user data" parameter passed to it (we don't use it) */
    desired.callback = &mixaudio;
    desired.userdata = NULL;

    if (!quiet) {
       printf("Bitbox sndbuflen : %d\n",BITBOX_SNDBUF_LEN);
       printf("Audio parameters wanted (before): format=%d, %d channels, fs=%d, %d samples.\n",
            desired.format , desired.channels, desired.freq, desired.samples);
    }

    if (SDL_OpenAudio(&desired, NULL) != 0) {
        printf("Error in opening audio peripheral: %s\n", SDL_GetError());
        return ; // return anyways even with no sound
    }

    if (!quiet)
        printf("Audio parameters obtained (after): format=%d, %d channels, fs=%d, %d samples.\n",
        desired.format , desired.channels, desired.freq, desired.samples);

}

// default empty implementation
__attribute__((weak)) void game_snd_buffer(uint16_t *buffer, int len)  {}
#endif

void set_mode(int width, int height)
{
    screen_width = width;
    screen_height = height;
    emu_texture = SDL_CreateTexture(emu_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);

    if ( !emu_texture )
    {
        printf("%s\n",SDL_GetError());
        bitbox_die(-1,0);
    }

    if (!quiet) {
        //printf("%d bpp, flags:%x pitch %d\n", screen->format->BitsPerPixel, screen->flags, screen->pitch/2);
        printf("Screen is now %dx%d with a scale of %d\n",screen_width,screen_height,scale);
    }

}

static void joy_init()
// by David Lau
{
    int i;
    int joy_count;

    /* Initilize the Joystick, and disable all later joystick code if an error occured */

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK))
        return;

    joy_count = SDL_NumJoysticks();

    /* Open all joysticks, assignement is in opening order */
    for (i = 0; i < joy_count; i++)
    {
        SDL_Joystick *joy = SDL_JoystickOpen(i);

        if (!quiet)
            printf("found Joystick %d : %d axis, %d buttons, %d hats (emu=%d)\n",
                i,
                SDL_JoystickNumAxes(joy),
                SDL_JoystickNumButtons(joy),
                SDL_JoystickNumHats(joy),
                SDL_JoystickNumHats(joy)==0
                );

        if (i<2) {
            sdl_gamepad_from_axis[i]=SDL_JoystickNumHats(joy)==0;
        }
    }

    /* make sure that Joystick sdl_event polling is a go */
    SDL_JoystickEventState(SDL_ENABLE);
}


void instructions ()
{
    printf("Invoke game with those options : \n");
    printf("  --fullscreen to run fullscreen\n");
    printf("  --scale2x to scale display 2x (also --scale1x)\n");
    printf("  --slow to run game very slowly (for debug)\n");
    printf("  --verbose show helpscreen and various messages\n");
    printf("  --nodisplay: no graphics handled\n");
    printf("  --nosoubnd: no sound handled\n");
    printf("  -- options ... : sends extra arguments to emulated program\n");
    printf("\n");
    printf("Use Joystick, Mouse or keyboard.");
    printf("Bitbox user Button is emulated by the F12 key.\n");
    printf("       -------\n");
    printf("Some games emulate Gamepad with the following keyboard keys :\n");
    printf("    Space (Select),   Enter (Start),   Arrows (D-pad)\n");
    printf("    D (A button),     F (B button),    E (X button), R (Y button),\n");
    printf("    Left/Right CTRL (L/R shoulders),   ESC (quit)\n");
    printf("       -------\n");
}



// reverse mapping to USB BootP

#ifdef __HAIKU__
uint8_t key_trans[256] = {
	[0x01]=0,41, 58,59,60,61, 62,63,64,65, 66,67,68,69,  70,71,72,

	[0x11]=53,30,31,32,33,34,35,36,37,38,39,45,46,42,    73,74,75,  83,84,85,86,
	[0x26]=43, 20,26, 8,21,23,28,24,12,18,19,47,48,49,   76,77,78,  95,96,97,87,
	[0x3b]=57,   4,22, 7, 9,10,11,13,14,15,51,52,40,                92,93,94,
	[0x4b]=225,  29,27, 6,25, 5,17,16,54,55,56,229,         82,     89,90,91,88,
	[0x5c]=224,226,           44             ,230,228,   80,81,79,  98,   99,
	        227,                              231,118
};
#else
uint8_t key_trans[256] = { // scan_code -> USB BootP code
    [0x6F]=0x52, // up
    [0x74]=0x51, // down
    [0x71]=0x50, // left
    [0x72]=0x4F, // right
    [0x41]=0x2C, // space
    [0x09]=0x29, // ESC -- needs to define DISABLE_ESC_EXIT in makefile to avoid escaping emulator !
    [0x17]=0x2B, // TAB
    [0x16]=42, // backspace on mine... (lowagner)
    [0x77]=76, // delete
    [0x76]=73, // insert
    [0x7f]=0x48, // pause

    [0x0a]=30,31,32,33,34,35,36,37,38,39,45,46, // 1234567890-=
    [0x18]=20,26, 8,21,23,28,  24,  12,  18,  19,  // qwertyuiop
    [0x22]=47,48, // []
    [0x26]= 4,22, 7, 9,10,11,13,14,15,51, // asdfghjkl;
    [0x30]=52,53, // ' and `
    [0x33]=49,  //  backslash
    [0x34]=29,27, 6,25, 5,17,16,54,55,56, // zxcvbnm,./

    [0x6e]=74, // home
    [0x73]=77, // end
    [0x70]=75, // pgup
    [0x75]=78, // pgdn
    [0x5a]=98,99, // 0. on number pad
    [0x57]=89,90,91, // 1 through 3
    [0x53]=92,93,94, // 4 through 6
    [0x4f]=95,96,97, // 7 through 9 on number pad

    [0x32]=225, // left shift
    [0x3e]=229, // right shift
    [0x40]=226, // L alt
    [0x6c]=230, // R alt
    [0x25]=0xe0, // L CTRL
    [0x69]=0xe4, // R CTRL
    [0x85]=0xe0, // L cmd (mac)
    [0x86]=0xe4, // R cmd (mac)
    [0x24]=0x28, // Enter

};
#endif

// this is a copy of the same function in usbh_hid_keybd
void kbd_emulate_gamepad (void)
{
    // kbd code for each gamepad buttons
    static const uint8_t kbd_gamepad[] = {
        0x07, 0x09, 0x08, 0x15, 0xe0, 0xe4, 0x2c, 0x28, 0x52, 0x51, 0x50, 0x4f
    };

    kbd_gamepad_buttons = 0;
    for (int i=0;i<sizeof(kbd_gamepad);i++) {
        if (memchr((char *)keyboard_key[0],kbd_gamepad[i],KBR_MAX_NBR_PRESSED))
            kbd_gamepad_buttons |= (1<<i);
    }

    // special : mods
    if (keyboard_mod[0] & 1)
        kbd_gamepad_buttons |= gamepad_L;
    if (keyboard_mod[0] & 16)
        kbd_gamepad_buttons |= gamepad_R;

}

// emualte hat from axis
void emulate_joy_hat(void)
{
    for (int pad=0;pad<gamepad_max_pads;pad++)
        if (sdl_gamepad_from_axis[pad]) {
            // clear movement bits
            sdl_gamepad_buttons[pad] &= ~(gamepad_up | gamepad_down | gamepad_left | gamepad_right);

            // set according to axis
            if (gamepad_x[pad]<-THRESHOLD_HAT)
                sdl_gamepad_buttons[pad] |= gamepad_left;
            if (gamepad_x[pad]> THRESHOLD_HAT)
                sdl_gamepad_buttons[pad] |= gamepad_right;

            if (gamepad_y[pad]<-THRESHOLD_HAT)
                    sdl_gamepad_buttons[pad] |= gamepad_up;
            if (gamepad_y[pad]> THRESHOLD_HAT)
                    sdl_gamepad_buttons[pad] |= gamepad_down;

        }

}


static bool handle_events()
{
    SDL_Event sdl_event;
    uint8_t key;
    //mouse_x = mouse_y=0; // not moved this frame

    while (SDL_PollEvent(&sdl_event))
    {
        // check for messages
        switch (sdl_event.type)
        {
        // exit if the window is closed
        case SDL_QUIT:
            return true;
            break;

        // check for keypresses
        case SDL_KEYDOWN:
            if (sdl_event.key.repeat) break;

            #ifndef DISABLE_ESC_EXIT
            if (sdl_event.key.keysym.sym == SDLK_ESCAPE)
                return true; // quit now
            #endif


            /* note that this event WILL be propagated so on emulator
            you'll see both button and keyboard. It's ot really a problem since
            programs rarely use the button and the keyboard */
            if (sdl_event.key.keysym.sym == USER_BUTTON_KEY)
                user_button=1;

            // now create the keyboard event
            key = sdl_event.key.keysym.scancode;
            // mod key ?
            switch (key) {
                case 0xe0: // lctrl
                    keyboard_mod[0] |= 1;
                    break;
                case 225: // lshift
                    keyboard_mod[0] |= 2;
                    break;
                case 226: // lalt
                    keyboard_mod[0] |= 4;
                    break;

                case 0xe4: // rctrl
                    keyboard_mod[0] |= 16;
                    break;
                case 229: // rshift
                    keyboard_mod[0] |= 32;
                    break;
                case 230: // ralt
                    keyboard_mod[0] |= 64;
                    break;

                default : // set it
                    for (int i=0;i<KBR_MAX_NBR_PRESSED;i++) {
                        if (keyboard_key[0][i]==0) {
                            keyboard_key[0][i]=key;
                            break;
                        }
                    }
            }

            //message("keydown %x\n",sdl_event.key.keysym.scancode);
            break;

        case SDL_KEYUP:
            if (sdl_event.key.repeat) break;

            if (sdl_event.key.keysym.sym == USER_BUTTON_KEY)
                user_button=0;

            key = sdl_event.key.keysym.scancode;
            // mod key ?
            switch (key) {
                case 0xe0: // lctrl
                    keyboard_mod[0] &= ~1;
                    break;
                case 225: // lshift
                    keyboard_mod[0] &= ~2;
                    break;
                case 226: // lalt
                    keyboard_mod[0] &= ~4;
                    break;

                case 0xe4: // rctrl
                    keyboard_mod[0] &= ~16;
                    break;
                case 229: // rshift
                    keyboard_mod[0] &= ~32;
                    break;
                case 230: // ralt
                    keyboard_mod[0] &= ~64;
                    break;

                default :
                    // other one : find it & release it
                    for (int i=0;i<KBR_MAX_NBR_PRESSED;i++) {
                        if (key==keyboard_key[0][i]) {
                            keyboard_key[0][i]=0;
                            break;
                        }
                    }
            }

            // message("keyup %x\n",sdl_event.key.keysym.scancode );
            break;

        // joypads
        case SDL_JOYAXISMOTION: // analog position
            switch (sdl_event.jaxis.axis) {
                case 0: /* X axis */
                    gamepad_x[sdl_event.jbutton.which]=sdl_event.jaxis.value>>8;
                    break;
                case 1: /* Y axis*/
                    gamepad_y[sdl_event.jbutton.which]=sdl_event.jaxis.value>>8;
                    break;
            }
            break;

        case SDL_JOYBUTTONUP: // buttons
            if (sdl_event.jbutton.button>=gamepad_max_buttons || sdl_event.jbutton.which>=gamepad_max_pads)
                break;
            sdl_gamepad_buttons[sdl_event.jbutton.which] &= ~(1<<sdl_event.jbutton.button);
            break;

        case SDL_JOYBUTTONDOWN:
            if (sdl_event.jbutton.button>=gamepad_max_buttons || sdl_event.jbutton.which>=gamepad_max_pads)
                break;
            sdl_gamepad_buttons[sdl_event.jbutton.which] |= 1<<sdl_event.jbutton.button;
            break;

        case SDL_JOYHATMOTION: // HAT
            if (sdl_event.jbutton.which>=gamepad_max_pads)
                break;
            sdl_gamepad_buttons[sdl_event.jbutton.which] &= ~(gamepad_up|gamepad_down|gamepad_left|gamepad_right);
            if (sdl_event.jhat.value & SDL_HAT_UP)      sdl_gamepad_buttons[sdl_event.jbutton.which] |= gamepad_up;
            if (sdl_event.jhat.value & SDL_HAT_DOWN)    sdl_gamepad_buttons[sdl_event.jbutton.which] |= gamepad_down;
            if (sdl_event.jhat.value & SDL_HAT_LEFT)    sdl_gamepad_buttons[sdl_event.jbutton.which] |= gamepad_left;
            if (sdl_event.jhat.value & SDL_HAT_RIGHT)   sdl_gamepad_buttons[sdl_event.jbutton.which] |= gamepad_right;
            break;

        // mouse
        case SDL_MOUSEBUTTONDOWN:
            mouse_buttons |= 1<<(sdl_event.button.button-1);
            break;
        case SDL_MOUSEBUTTONUP:
            mouse_buttons &= ~1<<(sdl_event.button.button-1);
            break;
        case SDL_MOUSEMOTION :
            mouse_x += sdl_event.motion.xrel;
            mouse_y += sdl_event.motion.yrel;
            break;

        } // end switch
    } // end of message processing

    return false; // don't exit  now
}

// -------------------------------------------------
// limited fatfs-related functions.
// XXX add non readonly features

#ifdef USE_SDCARD

FRESULT f_mount (FATFS* fs, const TCHAR* path, BYTE opt)
{
    return FR_OK;
}

FRESULT f_open (FIL* fp, const TCHAR* path, BYTE mode)
{
    char *mode_host=0;

    // XXX quite buggy ...
    if (mode & FA_OPEN_ALWAYS) {
        if (!access(path, F_OK)) // 0 if OK
            mode_host = "r+";
        else
            mode_host = "w+";

    } else switch (mode) {
        // Not a very good approximation, should rewrite to handle properly
        case FA_READ | FA_OPEN_EXISTING : mode_host="r"; break;
        case FA_READ | FA_WRITE | FA_OPEN_EXISTING : mode_host="r+"; break;
        case FA_WRITE | FA_OPEN_EXISTING : mode_host="r+"; break; // faked

        case FA_WRITE | FA_CREATE_NEW : mode_host="wx"; break;
        case FA_READ | FA_WRITE | FA_CREATE_NEW : mode_host="wx+"; break;

        case FA_READ | FA_WRITE | FA_CREATE_ALWAYS : mode_host="w+"; break;
        case FA_WRITE | FA_CREATE_ALWAYS : mode_host="w"; break;

        default :
            return FR_INVALID_PARAMETER;
    }

    // fill size field
    struct stat st;
    if (stat(path, &st) == 0)
        fp->fsize= st.st_size;
    else
        fp->fsize=-1;

    fp->fs = (FATFS*) fopen ((const char*)path,mode_host); // now ignores mode.
    if (fp->fs) return FR_OK;

    switch(errno) {
        case ENOENT:
            return FR_NO_FILE;
        case EACCES:
            return FR_WRITE_PROTECTED;
        case EEXIST:
            return FR_EXIST;
        default:
            return FR_INT_ERR;
    }

}

FRESULT f_close (FIL* fp)
{
    int res = fclose( (FILE*) fp->fs);
    fp->fs=NULL;
    return res?FR_DISK_ERR:FR_OK; // FIXME handle reasons ?
}

FRESULT f_read (FIL* fp, void* buff, UINT btr, UINT* br)
{
    *br = fread ( buff, 1,btr, (FILE *)fp->fs);
    return FR_OK; // XXX handle ferror
}

FRESULT f_write (FIL* fp, const void* buff, UINT btr, UINT* br)
{
    *br = fwrite ( buff,1, btr, (FILE *)fp->fs);
    return FR_OK; // XXX handle ferror
}


FRESULT f_lseek (FIL* fp, DWORD ofs)
{
    int res = fseek ( (FILE *)fp->fs, ofs, SEEK_SET);
    return res ? FR_DISK_ERR : FR_OK; // always from start
}

/* Change current directory */
FRESULT f_chdir (const char* path)
{
    int res = chdir(path);
    return res ? FR_DISK_ERR : FR_OK;
}


FRESULT f_opendir ( DIR* dp, const TCHAR* path )
{
    if (path[0] == 0)
        path = ".";
    NIX_DIR *res = opendir(path);
    if (res) {
        dp->fs = (FATFS*) res; // hides it in the fs field as a fatfs variable
        dp->dir = (unsigned char *)path;
        return FR_OK;
    } else {
        printf("Error opening directory %s: %s\n", path, strerror(errno));
        return FR_DISK_ERR;
    }
}

FRESULT f_readdir ( DIR* dp, FILINFO* fno )
{
    errno=0;
    char buffer[512]; // assumes max path size for FAT32

    struct dirent *de = readdir((NIX_DIR *)dp->fs);

    if (de) {
        if (strlen(de->d_name)<=12) {
            // not long filename
            for (int i=0;i<13;i++)
                fno->fname[i]=de->d_name[i];
            fno->lfname[0]='\0';
        } else {
            // first make short name
            // copy first 6 chars
            for (int i=0;i<6;i++)
                fno->fname[i]=de->d_name[i];
            fno->fname[6]='~';
            fno->fname[7]='0'; // FIXME : multiple files
            fno->fname[8]='.';

            // copy extension
            for (int i=0;i<4;i++)
                fno->fname[9+i]=de->d_name[strlen(de->d_name)-3+i];
            fno->fname[14]='\0';

            // make long name
            if(_USE_LFN) {
                strncpy(fno->lfname,de->d_name,_MAX_LFN);
            } else {
                fno->lfname[0]='\0';
            }
        }


        fno->fattrib = 0;

        // check attributes of found file
        strncpy(buffer,(char *)dp->dir,sizeof(buffer)); // BYTE->char
        strcat(buffer,"/");
        strcat(buffer,fno->fname);

        struct stat stbuf;
        stat(buffer,&stbuf);

        if (S_ISDIR(stbuf.st_mode))
             fno->fattrib = AM_DIR;
        return FR_OK;

    } else {
        if (errno) {
            printf("Error reading directory %s: %s\n",dp->dir, strerror(errno)); // not neces an erro, can be end of dir.
            return FR_DISK_ERR;
        } else {
            fno->fname[0]='\0';
            return FR_OK;
        }
    }
}

FRESULT f_closedir (DIR* dp)
{
    if (!closedir((NIX_DIR *)dp->fs)) {
        return FR_OK ;
    } else {
        printf("Error closing directory %s : %s\n",dp->dir, strerror(errno));
        return FR_DISK_ERR;
    }
}

FRESULT f_rename (const char *file_from, const char *file_to)
{
    if (!rename(file_from,file_to) ) {
        return FR_OK;
    } else {
        printf("Error renaming %s to %s : : %s\n",file_from, file_to, strerror(errno));
        return FR_DISK_ERR;
    }
}
#endif // USE_SDCARD
// -- misc bitbox functions

// user button
int button_state() {
    return user_button;
}

// user LED
void set_led(int x) {
    if (!quiet)
        printf("Setting LED to %d\n",x);
    SDL_SetWindowTitle(emu_window, x?WM_TITLE_LED_ON:WM_TITLE_LED_OFF);
}

void message (const char *fmt, ...)
{
    va_list argptr;
    va_start(argptr, fmt);
    vprintf(fmt, argptr);
    va_end(argptr);
}

// --- main
int bitbox_argc;
char **bitbox_argv;

static void process_commandline( int argc, char** argv)
{
    for (int i=1;i<argc;i++) {

        if (!strcmp(argv[i],"--fullscreen"))
            fullscreen = 1;
        else if (!strcmp(argv[i],"--slow"))
            slow = 1;
        else if (!strcmp(argv[i],"--verbose"))
            quiet = 0;
        else if (!strcmp(argv[i],"--nodisplay"))
            nodisplay = 1;
        else if (!strcmp(argv[i],"--nosound"))
            nosound = 1;
        else if (!strcmp(argv[i],"--scale1x"))
            scale = 1;
        else if (!strcmp(argv[i],"--scale2x"))
            scale = 2;
        else if (!strcmp(argv[i],"--")) {
            // anything after goes to emulated program
            bitbox_argc = argc - i-1;
            bitbox_argv = &argv[i+1];
            break;
        } else {
            instructions();
            exit(0);
        }

        #if VGA_MODE==NONE
        nodisplay=1;
        #endif

        #ifndef NO_AUDIO
        nosound=1;
        #endif

    }

    // display current options
    if (!quiet) {
        printf("Options : %s %s scale : %d\n",fullscreen?"fullscreen":"windowed",slow?"slow":"normal speed",scale);
        instructions();
        printf(" - Starting\n");
    }
}

static void init_all(void)
{
    // initialize SDL video
    uint32_t flags=0;

    if (!nodisplay)
        flags |= SDL_INIT_VIDEO;
    
    if (!nosound)
        flags |= SDL_INIT_AUDIO;

    if ( SDL_Init( flags ) < 0 ) {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        exit(1);
    }
    atexit(SDL_Quit); // make sure SDL cleans up before exit

    message("Making window %d by %d\n", VGA_H_PIXELS*scale, VGA_V_PIXELS*scale);
    emu_window = SDL_CreateWindow(
         "This will surely be overwritten", 
         SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, VGA_H_PIXELS*scale, VGA_V_PIXELS*scale, 
         fullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE
    );
    if ( !emu_window ) {
        printf("%s\n",SDL_GetError());
        bitbox_die(-1,0);
    }

    emu_renderer = SDL_CreateRenderer(emu_window, -1, SDL_RENDERER_ACCELERATED);
    if ( !emu_renderer ) {
        printf("%s\n",SDL_GetError());
        bitbox_die(-1,0);
    }


    set_led(0); // off by default

    if (!nodisplay) {
        set_palette_colors(micro_palette,0,256); // default
        set_mode(VGA_H_PIXELS,VGA_V_PIXELS); // create a default new window
        SDL_ShowCursor(SDL_DISABLE);
    }

    if (!nosound) {
        audio_init();
        SDL_PauseAudio(0); // now start sound
    }

    joy_init();
}

SDL_sem *frame_sem;

void wait_vsync()
{
    // wait other thread to wake me up
    if (SDL_SemWait(frame_sem)) {
        printf("SDL_SemWait failed: %s\n", SDL_GetError());
    }
}

// wait for the next 60Hz frame.
// during this the other thread will typically update
static inline void frame_wait( void )
{
    static int next_time = 0;

    // 60 Hz loop delay
    int now = SDL_GetTicks();
    if (next_time > now)
        SDL_Delay(next_time - now);

    if (((int)now-(int)next_time) > RESYNC_TIME_MS)  { // if too much delay, resync
        printf("- lost sync : %d, resyncing\n",now-next_time);
        next_time = now;
    } else {
        next_time += slow ? TICK_INTERVAL*10:TICK_INTERVAL;
    }
}

int math_gcd(int a, int b)
{
    while (b != 0) {
        int c = a % b;
        a = b;
        b = c;
    }
    return a;
}

// we can draw to the screen no problem (it's the correct bitbox vga dimensions)
// but when copying to the window let's ensure the correct aspect ratio.
void get_renderer_rect(SDL_Rect *dest_rect)
{
    // ensure a nice aspect ratio using the least-common-multiple (lcm) of screen dimensions.
    // this could be precomputed in set_mode if desired, but it is not a hard computation.
    int aspect_lcm = screen_width / math_gcd(screen_width, screen_height) * screen_height;
    // for a 640x480 screen, aspect will be 4:3 (aspect_width:aspect_height)
    int aspect_width = aspect_lcm / screen_height;
    int aspect_height = aspect_lcm / screen_width;

    int window_width, window_height;
    SDL_GetWindowSize(emu_window, &window_width, &window_height);

    if (aspect_height * window_width >= aspect_width * window_height) {
        // height is the limiting factor:
        dest_rect->h = window_height;
        dest_rect->w = window_height * aspect_width / aspect_height;
        dest_rect->x = (window_width - dest_rect->w)/2;
        dest_rect->y = 0;
    } else {
        // width is the limiting factor:
        dest_rect->w = window_width;
        dest_rect->h = window_width * aspect_height / aspect_width;
        dest_rect->x = 0;
        dest_rect->y = (window_height - dest_rect->h)/2;
    }
}

/* this loop handles asynchronous emulation : screen refresh, user inputs.. */
int emu_loop (void *_)
{
    while (1) {
        if (!nodisplay)  {
            update_texture(emu_texture);
            SDL_RenderClear(emu_renderer);
            SDL_Rect dest_rect;
            get_renderer_rect(&dest_rect);
            SDL_RenderCopy(emu_renderer, emu_texture, NULL, &dest_rect);
            SDL_RenderPresent(emu_renderer);
        }

        // message processing loop
        bool done = handle_events();
        if (done) break;

        kbd_emulate_gamepad();
        emulate_joy_hat();
        gamepad_buttons[0] = kbd_gamepad_buttons|sdl_gamepad_buttons[0];
        gamepad_buttons[1] = sdl_gamepad_buttons[1];

        vga_frame++;

        /* we release the semaphore and wait for the other thread. */
        SDL_SemPost(frame_sem);
        frame_wait();

        if (!nodisplay) {
            vsync_screen(); // runs vsync now, let's hope other thread has finished ...
            //SDL_Flip(screen);
        }
    }

    SDL_DestroyTexture(emu_texture);
    SDL_DestroyRenderer(emu_renderer);
    SDL_DestroyWindow(emu_window);
    if (!quiet) printf("Exiting properly\n");

    SDL_Quit();
    exit(0); // should kill main thread
    return 0; // necessary for create thread
}


int main ( int argc, char** argv )
{
    process_commandline(argc,argv);
    init_all();
    
    frame_sem=SDL_CreateSemaphore(0);
    if (!frame_sem) {
        printf("SDL_SemCreate failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_CreateThread(emu_loop,"emulation",0);
    bitbox_main();


    SDL_DestroySemaphore(frame_sem);

    return 0;
}


