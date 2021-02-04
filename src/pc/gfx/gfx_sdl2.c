#include "../compat.h"

#if !defined(__BSD__) && !defined(TARGET_DOS) && (defined(ENABLE_OPENGL) || defined(ENABLE_OPENGL_LEGACY) || defined(ENABLE_SOFTRAST))

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#if FOR_WINDOWS
#include <GL/glew.h>
#include "SDL.h"
#define GL_GLEXT_PROTOTYPES 1
#include "SDL_opengl.h"
#else
#include <SDL/SDL.h>
/*#define GL_GLEXT_PROTOTYPES 1
#ifdef ENABLE_OPENGL_LEGACY
#include <SDL2/SDL_opengl.h>
#else
#include <SDL2/SDL_opengles2.h>
#endif*/
#endif

#include "../common.h"
#include "../configfile.h"
#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"

#define RES_HW_SCREEN_HORIZONTAL    240
#define RES_HW_SCREEN_VERTICAL      240

#if defined(VERSION_EU)
# define FRAMERATE 25
#else
# define FRAMERATE 30
#endif

#ifndef SDL_TRIPLEBUF
#define SDL_TRIPLEBUF SDL_DOUBLEBUF
#endif

#ifdef ENABLE_SOFTRAST
#define GFX_API_NAME "SDL1.2 - Software"
#include "gfx_soft.h"
//static SDL_Renderer *renderer;
SDL_Surface *sdl_screen = NULL;
static SDL_Surface *sdl_screen_fullRes = NULL;
static SDL_Surface *sdl_screen_halfRes = NULL;
SDL_PixelFormat sdl_screen_bgr;
static SDL_Surface *buffer = NULL;
static SDL_Surface *texture = NULL;
//static SDL_Texture *texture = NULL;
#else
#define GFX_API_NAME "SDL2 - OpenGL"
#endif

//static SDL_Window *wnd;
static int inverted_scancode_table[512];
static int vsync_enabled = 0;
static unsigned int window_width = DESIRED_SCREEN_WIDTH;
static unsigned int window_height = DESIRED_SCREEN_HEIGHT;
static bool fullscreen_state;
static void (*on_fullscreen_changed_callback)(bool is_now_fullscreen);
static bool (*on_key_down_callback)(int scancode);
static bool (*on_key_up_callback)(int scancode);
static void (*on_all_keys_up_callback)(void);
static bool half_res = false;

// time between consequtive game frames
const int frame_time = 1000 / FRAMERATE;
static int too_slow_in_a_row = 0;
static int normal_speed_in_a_row = 0;
static int elapsed_time_avg = 0;
static int elapsed_time_cnt = 0;
#define MAX_AVG_ELAPSED_TIME_COUNTS 10
#define MAX_TOO_SLOW_IN_A_ROW       1
#define MAX_NORMAL_SPEED_IN_A_ROW   2

// frameskip
static bool do_render = true;
static volatile uint32_t tick = 0;
static uint32_t last = 0;
static SDL_TimerID idTimer = 0;

// fps stats tracking
static int f_frames = 0;
static double f_time = 0.0;

#if defined(DIRECT_SDL) && defined(SDL_SURFACE)
	uint32_t *gfx_output;
#endif


const SDLKey windows_scancode_table[] =
{
    /*	0						1							2							3							4						5							6							7 */
    /*	8						9							A							B							C						D							E							F */
    SDLK_UNKNOWN,		SDLK_ESCAPE,		SDLK_1,				SDLK_2,				SDLK_3,			SDLK_4,				SDLK_5,				SDLK_6,			/* 0 */
    SDLK_7,				SDLK_8,				SDLK_9,				SDLK_0,				SDLK_MINUS,		SDLK_EQUALS,		SDLK_BACKSPACE,		SDLK_TAB,		/* 0 */

    SDLK_q,				SDLK_w,				SDLK_e,				SDLK_r,				SDLK_t,			SDLK_y,				SDLK_u,				SDLK_i,			/* 1 */
    SDLK_o,				SDLK_p,				SDLK_LEFTBRACKET,	SDLK_RIGHTBRACKET,	SDLK_RETURN,	SDLK_LCTRL,			SDLK_a,				SDLK_s,			/* 1 */

    SDLK_d,				SDLK_f,				SDLK_g,				SDLK_h,				SDLK_j,			SDLK_k,				SDLK_l,				SDLK_SEMICOLON,	/* 2 */
    SDLK_UNKNOWN,	SDLK_UNKNOWN,			SDLK_LSHIFT,		SDLK_BACKSLASH,		SDLK_z,			SDLK_x,				SDLK_c,				SDLK_v,			/* 2 */

    SDLK_b,				SDLK_n,				SDLK_m,				SDLK_COMMA,			SDLK_PERIOD,	SDLK_SLASH,			SDLK_RSHIFT,		SDLK_PRINT,/* 3 */
    SDLK_LALT,			SDLK_SPACE,			SDLK_CAPSLOCK,		SDLK_F1,			SDLK_F2,		SDLK_F3,			SDLK_F4,			SDLK_F5,		/* 3 */

    SDLK_F6,			SDLK_F7,			SDLK_F8,			SDLK_F9,			SDLK_F10,		SDLK_NUMLOCK,	SDLK_SCROLLOCK,	SDLK_HOME,		/* 4 */
    SDLK_UP,			SDLK_PAGEUP,		SDLK_KP_MINUS,		SDLK_LEFT,			SDLK_KP5,		SDLK_RIGHT,			SDLK_KP_PLUS,		SDLK_END,		/* 4 */

    SDLK_DOWN,			SDLK_PAGEDOWN,		SDLK_INSERT,		SDLK_DELETE,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_BACKSLASH,SDLK_F11,		/* 5 */
    SDLK_F12,			SDLK_PAUSE,			SDLK_UNKNOWN,		SDLK_UNKNOWN,			SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,	/* 5 */

    SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_F13,		SDLK_F14,			SDLK_F15,			SDLK_UNKNOWN,		/* 6 */
    SDLK_UNKNOWN,			SDLK_UNKNOWN,			SDLK_UNKNOWN,			SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	/* 6 */

    SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	/* 7 */
    SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN	/* 7 */
};

const SDLKey scancode_rmapping_extended[][2] = {
    {SDLK_KP_ENTER, SDLK_RETURN},
    {SDLK_RALT, SDLK_LALT},
    {SDLK_RCTRL, SDLK_LCTRL},
    {SDLK_KP_DIVIDE, SDLK_SLASH},
    //{SDLK_KP_PLUS, SDLK_CAPSLOCK}
};

const SDLKey scancode_rmapping_nonextended[][2] = {
    {SDLK_KP7, SDLK_HOME},
    {SDLK_KP8, SDLK_UP},
    {SDLK_KP9, SDLK_PAGEUP},
    {SDLK_KP4, SDLK_LEFT},
    {SDLK_KP6, SDLK_RIGHT},
    {SDLK_KP1, SDLK_END},
    {SDLK_KP2, SDLK_DOWN},
    {SDLK_KP3, SDLK_PAGEDOWN},
    {SDLK_KP0, SDLK_INSERT},
    {SDLK_KP_PERIOD, SDLK_DELETE},
    {SDLK_KP_MULTIPLY, SDLK_PRINT},
    {SDLK_UP, SDLK_UP},
    {SDLK_LEFT, SDLK_LEFT},
    {SDLK_RIGHT, SDLK_RIGHT},
    {SDLK_DOWN, SDLK_DOWN},
};

static void set_halfResScreen(bool on, bool call_callback) {
#ifdef ENABLE_SOFTRAST
  printf("%s %s\n", __func__, on?"ON":"OFF");

  half_res = on;

  if (on) {
      window_width = configScreenWidth/2;
      window_height = configScreenHeight/2;
      sdl_screen = sdl_screen_halfRes;
  } else {
      window_width = configScreenWidth;
      window_height = configScreenHeight;
      sdl_screen = sdl_screen_fullRes;
  }
  gfx_output = sdl_screen->pixels;
  printf("new window_width=%d, new window_height=%d\n", window_width, window_height);

  if (on_fullscreen_changed_callback != NULL && call_callback) {
      on_fullscreen_changed_callback(on);
  }

#endif
}

static void set_fullscreen(bool on, bool call_callback) {
  if (fullscreen_state == on) {
      return;
  }
  fullscreen_state = on;

#ifndef ENABLE_SOFTRAST
  if (on) {
      SDL_DisplayMode mode;
      SDL_GetDesktopDisplayMode(0, &mode);
      window_width = mode.w;
      window_height = mode.h;
  } else {
      window_width = configScreenWidth;
      window_height = configScreenHeight;
  }
  //printf("window_width=%d, window_height=%d\n", window_width, window_height);
  SDL_SetWindowSize(wnd, window_width, window_height);
  SDL_SetWindowFullscreen(wnd, on ? SDL_WINDOW_FULLSCREEN : 0);

  if (on_fullscreen_changed_callback != NULL && call_callback) {
      on_fullscreen_changed_callback(on);
  }
#else
  //set_halfResScreen(!on, call_callback);
#endif
}

int test_vsync(void) {
    // Even if SDL_GL_SetSwapInterval succeeds, it doesn't mean that VSync actually works.
    // A 60 Hz monitor should have a swap interval of 16.67 milliseconds.
    // Try to detect the length of a vsync by swapping buffers some times.
    // Since the graphics card may enqueue a fixed number of frames,
    // first send in four dummy frames to hopefully fill the queue.
    // This method will fail if the refresh rate is changed, which, in
    // combination with that we can't control the queue size (i.e. lag)
    // is a reason this generic SDL2 backend should only be used as last resort.
    #ifndef ENABLE_SOFTRAST
    Uint32 start;
    Uint32 end;

    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    start = SDL_GetTicks();
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    end = SDL_GetTicks();

    float average = 4.0 * 1000.0 / (end - start);

    vsync_enabled = 1;
    if (average > 27 && average < 33) {
        SDL_GL_SetSwapInterval(1);
    } else if (average > 57 && average < 63) {
        SDL_GL_SetSwapInterval(2);
    } else if (average > 86 && average < 94) {
        SDL_GL_SetSwapInterval(3);
    } else if (average > 115 && average < 125) {
        SDL_GL_SetSwapInterval(4);
    } else {
        vsync_enabled = 0;
    }
    vsync_enabled = 1;
    #endif
    vsync_enabled = 0;
}


static uint32_t timer_handler(uint32_t interval, void *param)
{
    //printf("%s, interval=%d, tick=%d\n", __func__, interval, tick);
     ++tick;
    return interval;
}

static void gfx_sdl_init(const char *game_name, bool start_in_fullscreen) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    idTimer = SDL_AddTimer(frame_time, timer_handler, NULL);
    last = tick;

    char title[512];
    sprintf(title, "%s (%s)", game_name, GFX_API_NAME);
	
    window_width = configScreenWidth;
    window_height = configScreenHeight;
#ifdef ENABLE_SOFTRAST

	SDL_ShowCursor(0);
  #ifdef CONVERT
  	 sdl_screen = SDL_SetVideoMode(window_width, window_height, 16, SDL_HWSURFACE | SDL_TRIPLEBUF);
  #else
    #ifdef DIRECT_SDL
    	sdl_screen = SDL_SetVideoMode(window_width, window_height, 32, SDL_HWSURFACE | SDL_TRIPLEBUF);
    #else
    	texture = SDL_SetVideoMode(240, 240, 32, SDL_HWSURFACE | SDL_TRIPLEBUF);
    	sdl_screen_fullRes = SDL_CreateRGBSurface(SDL_SWSURFACE, window_width, window_height, 32, 0,0,0,0);
      sdl_screen_halfRes = SDL_CreateRGBSurface(SDL_SWSURFACE, window_width/2, window_height/2, 32, 0,0,0,0);
      set_halfResScreen(!start_in_fullscreen, false);
    #endif
  #endif
	#ifdef SDL_SURFACE
	   gfx_output = sdl_screen->pixels;
	#endif
#else
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    wnd = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (start_in_fullscreen) {
        set_fullscreen(start_in_fullscreen, false);
    }

    SDL_GL_CreateContext(wnd);

    SDL_GL_SetSwapInterval(1);
    test_vsync();
    if (!vsync_enabled)
        puts("Warning: VSync is not enabled or not working. Falling back to timer for synchronization");
#endif

    f_time = SDL_GetTicks();

    for (size_t i = 0; i < sizeof(windows_scancode_table) / sizeof(SDLKey); i++) {
        inverted_scancode_table[windows_scancode_table[i]] = i;
    }

    for (size_t i = 0; i < sizeof(scancode_rmapping_extended) / sizeof(scancode_rmapping_extended[0]); i++) {
        inverted_scancode_table[scancode_rmapping_extended[i][0]] = inverted_scancode_table[scancode_rmapping_extended[i][1]] + 0x100;
    }

    for (size_t i = 0; i < sizeof(scancode_rmapping_nonextended) / sizeof(scancode_rmapping_nonextended[0]); i++) {
        inverted_scancode_table[scancode_rmapping_nonextended[i][0]] = inverted_scancode_table[scancode_rmapping_nonextended[i][1]];
        inverted_scancode_table[scancode_rmapping_nonextended[i][1]] += 0x100;
    }
}

static void gfx_sdl_set_fullscreen_changed_callback(void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
    on_fullscreen_changed_callback = on_fullscreen_changed;
}

static void gfx_sdl_set_fullscreen(bool enable) {
    set_fullscreen(enable, true);
}

static void gfx_sdl_set_keyboard_callbacks(bool (*on_key_down)(int scancode), bool (*on_key_up)(int scancode), void (*on_all_keys_up)(void)) {
    on_key_down_callback = on_key_down;
    on_key_up_callback = on_key_up;
    on_all_keys_up_callback = on_all_keys_up;
}

static void gfx_sdl_main_loop(void (*run_one_game_iter)(void)) {
    const uint32_t now = tick;

    const uint32_t frames = now - last;
    if (frames) {

        // catch up but skip the first FRAMESKIP frames
        int skip = (frames > configFrameskip) ? configFrameskip : (frames - 1);
        for (uint32_t f = 0; f < frames; ++f, --skip) {
            do_render = (skip <= 0);
            
            /*if(!do_render){
              printf("frameskip! %d missed frames\n", frames-1);
            }*/

            run_one_game_iter();
        }
    }
    else{
        do_render = true;
        run_one_game_iter();
    }
    
    last = now;

    /*while (1) {
        run_one_game_iter();
    }*/
}

static void gfx_sdl_get_dimensions(uint32_t *width, uint32_t *height) {
/*#ifdef ENABLE_SOFTRAST
    *width = configScreenWidth;
    *height = configScreenHeight;
#else
    *width = window_width;
    *height = window_height;
#endif*/
    *width = window_width;
    *height = window_height;
}

static int translate_scancode(int scancode) {
    if (scancode < 512) {
        return inverted_scancode_table[scancode];
    } else {
        return 0;
    }
}

static void gfx_sdl_onkeydown(int scancode) {
    int key = translate_scancode(scancode);

    if (on_key_down_callback != NULL) {
        on_key_down_callback(key);
    }
}

static void gfx_sdl_onkeyup(int scancode) {
    int key = translate_scancode(scancode);
    if (on_key_up_callback != NULL) {
        on_key_up_callback(key);
    }
}

static void gfx_sdl_handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
#ifndef TARGET_WEB
            // Scancodes are broken in Emscripten SDL2: https://bugzilla.libsdl.org/show_bug.cgi?id=3259
            case SDL_KEYDOWN:
        				if (event.key.keysym.sym == SDLK_q)
        				{
        					game_exit();
        				}

                // TESTS
                if (event.key.keysym.sym == SDLK_k)
                {
                  printf("%s Half Res\n", half_res?"Disabling":"Enabling");
                  set_fullscreen(!half_res, true);
                }

                gfx_sdl_onkeydown(event.key.keysym.sym);
                break;
            case SDL_KEYUP:
                gfx_sdl_onkeyup(event.key.keysym.sym);
                break;
#endif
            case SDL_QUIT:
                game_exit();
                break;
        }
    }
}

static bool gfx_sdl_start_frame(void) {
    //return true;
    return do_render;
}

static void sync_framerate_with_timer(void) {
    static Uint32 last_time = 0;
    //printf("%s\n", __func__);

    // get base timestamp on the first frame (might be different from 0)
    if (last_time == 0) last_time = SDL_GetTicks();
    const int elapsed = SDL_GetTicks() - last_time;
    if (elapsed < frame_time){
      SDL_Delay(frame_time - elapsed);
    }
    last_time = SDL_GetTicks();
    
    elapsed_time_avg += elapsed;
    elapsed_time_cnt++;

    if(elapsed_time_cnt > MAX_AVG_ELAPSED_TIME_COUNTS){
      elapsed_time_avg /= elapsed_time_cnt;

      int min_frame_time = half_res?(13*frame_time/32):frame_time;

      if (elapsed_time_avg < min_frame_time){
        too_slow_in_a_row = 0;
        normal_speed_in_a_row++;
        //printf("speed %d\n", normal_speed_in_a_row);
      }
      else{
        too_slow_in_a_row++;
        normal_speed_in_a_row = 0;
        //printf("slow %d\n", too_slow_in_a_row);
      }

      elapsed_time_cnt = 0;
      elapsed_time_avg = 0;
    }
      
}


static uint16_t rgb888Torgb565(uint32_t s)
{
	return (uint16_t) ((s >> 8 & 0xf800) + (s >> 5 & 0x7e0) + (s >> 3 & 0x1f));
}


/* Clear SDL screen (for multiple-buffering) */
static void clear_screen(SDL_Surface *surface) {
  memset(surface->pixels, 0, surface->w*surface->h*surface->format->BytesPerPixel);
}


/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
static void flip_NNOptimized_AllowOutOfScreen(SDL_Surface *src_surface, SDL_Rect *src_rect, SDL_Surface *dst_surface, int new_w, int new_h) {
  int w1 = src_rect->w;
  int h1 = src_rect->h;
  int w2 = new_w;
  int h2 = new_h;
  int x_ratio = (int) ((w1 << 16) / w2);
  int y_ratio = (int) ((h1 << 16) / h2);
  int x2, y2;

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL - new_h) / 2;
  int x_padding = 0;
  if (w2 > RES_HW_SCREEN_HORIZONTAL) {
    x_padding = (w2 - RES_HW_SCREEN_HORIZONTAL) / 2 + 1;
  }
  int x_padding_ratio = x_padding * w1 / w2;

  /// --- Offset to get first src_pixels row
  uint32_t *src_row = (uint32_t*)(src_surface->pixels) + src_surface->w * src_rect->y + src_rect->x;

  for (int i = 0; i < h2; i++) {
    if (i >= RES_HW_SCREEN_VERTICAL) {
      continue;
    }

    uint32_t *t = ((uint32_t *)dst_surface->pixels) + ((i + y_padding) * ((w2 > RES_HW_SCREEN_HORIZONTAL) ? RES_HW_SCREEN_HORIZONTAL : w2)) ;
    y2 = (i * y_ratio) >> 16;
    uint32_t *p = (uint32_t*)(src_row) + (y2*src_surface->w + x_padding_ratio) ;
    int rat = 0;
    for (int j = 0; j < w2; j++) {
      if (j >= RES_HW_SCREEN_HORIZONTAL) {
        continue;
      }
      x2 = rat >> 16;
      *t++ = p[x2];
      rat += x_ratio;
    }
  }
}



/// Nearest neighboor optimized with possible out of screen coordinates (for cropping)
void flip_Upscaling_Bilinear(SDL_Surface *src_surface, SDL_Rect *src_rect, SDL_Surface *dst_surface, int new_w, int new_h){
  int w1 = src_rect->w;
  int h1 = src_rect->h;
  int w2=new_w;
  int h2=new_h;
  int x_ratio = (int) ((w1 << 16) / w2);
  int y_ratio = (int) ((h1 << 16) / h2);
  uint32_t x_diff, y_diff;
  uint32_t red_comp, green_comp, blue_comp, alpha_comp;
  uint32_t p_val_tl, p_val_tr, p_val_bl, p_val_br;
  int x, y ;
  //printf("src_surface->h=%d, h2=%d\n", src_surface->h, h2);

  /// --- Compute padding for centering when out of bounds ---
  int y_padding = (RES_HW_SCREEN_VERTICAL-new_h)/2;
  int x_padding = 0;
  if(w2>RES_HW_SCREEN_HORIZONTAL){
    x_padding = (w2-RES_HW_SCREEN_HORIZONTAL)/2 + 1;
  }
  int x_padding_ratio = x_padding * w1 / w2;

  /// --- Offset to get first src_pixels row
  uint32_t *src_row = (uint32_t*)(src_surface->pixels) + src_surface->w * src_rect->y + src_rect->x;

  for (int i=0;i<h2;i++)
  {
    if(i>=RES_HW_SCREEN_VERTICAL){
      continue;
    }

    uint32_t *t = ((uint32_t *)dst_surface->pixels) + ((i + y_padding) * ((w2 > RES_HW_SCREEN_HORIZONTAL) ? RES_HW_SCREEN_HORIZONTAL : w2)) ;
    y = ((i*y_ratio)>>16);
    y_diff = (i*y_ratio) - (y<<16) ;
    uint32_t *p = (uint32_t*)(src_row) + (y*src_surface->w + x_padding_ratio) ;
    int rat = 0;
    for (int j=0;j<w2;j++)
    {
      if(j>=RES_HW_SCREEN_HORIZONTAL){
        continue;
      }
      x = (rat>>16);
      x_diff = rat - (x<<16) ;

      /// --- Getting adjacent pixels ---
      p_val_tl = p[x] ;
      p_val_tr = (x+1<w1)?p[x+1]:p[x];
      p_val_bl = (y+1<h1)?p[x+w1]:p[x];
      p_val_br = (y+1<h1 && x+1<w1)?p[x+w1+1]:p[x];

      // red element
      // Yr = Ar(1-w)(1-h) + Br(w)(1-h) + Cr(h)(1-w) + Dr(wh)
      red_comp = (( ((p_val_tl&0xFF000000)>>24) * ( (((1<<16)-x_diff) * ((1<<16)-y_diff)) >>8) )>>24) +
          (( ((p_val_tr&0xFF000000)>>24) * ((x_diff * ((1<<16)-y_diff)) >>8) )>>24) +
            (( ((p_val_bl&0xFF000000)>>24) * ((y_diff * ((1<<16)-x_diff)) >>8) )>>24) +
            (( ((p_val_br&0xFF000000)>>24) * ((y_diff * x_diff) >>8) )>>24);

      // green element
      // Yg = Ag(1-w)(1-h) + Bg(w)(1-h) + Cg(h)(1-w) + Dg(wh)
      green_comp = (( ((p_val_tl&0x00FF0000)>>16) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>8) )>>24) +
          (( ((p_val_tr&0x00FF0000)>>16) * ((x_diff * ((1<<16)-y_diff)) >>8) )>>24) +
            (( ((p_val_bl&0x00FF0000)>>16) * ((y_diff * ((1<<16)-x_diff)) >>8) )>>24) +
            (( ((p_val_br&0x00FF0000)>>16) * ((y_diff * x_diff) >>8) )>>24);

      // blue element
      // Yb = Ab(1-w)(1-h) + Bb(w)(1-h) + Cb(h)(1-w) + Db(wh)
      blue_comp = (( ((p_val_tl&0x0000FF00)>>8) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>8) )>>24) +
          (( ((p_val_tr&0x0000FF00)>>8) * ((x_diff * ((1<<16)-y_diff)) >>8) )>>24) +
            (( ((p_val_bl&0x0000FF00)>>8) * ((y_diff * ((1<<16)-x_diff)) >>8) )>>24) +
            (( ((p_val_br&0x0000FF00)>>8) * ((y_diff * x_diff) >>8) )>>24);

      // alpha element
      // Ya = Aa(1-w)(1-h) + Ba(w)(1-h) + Ca(h)(1-w) + Da(wh)
      alpha_comp = (( ((p_val_tl&0x000000FF)) * ((((1<<16)-x_diff) * ((1<<16)-y_diff))>>8) )>>24) +
          (( ((p_val_tr&0x000000FF)) * ((x_diff * ((1<<16)-y_diff)) >>8) )>>24) +
            (( ((p_val_bl&0x000000FF)) * ((y_diff * ((1<<16)-x_diff)) >>8) )>>24) +
            (( ((p_val_br&0x000000FF)) * ((y_diff * x_diff) >>8) )>>24);

     //   printf("red_comp=%d, green_comp=%d, blue_comp=%d, alpha_comp=%d, \n", red_comp, green_comp, blue_comp, alpha_comp);

      /// --- Write pixel value ---
      *t++ = ((red_comp<<24)&0xFF000000) + ((green_comp<<16)&0x00FF0000) + ((blue_comp<<8)&0x0000FF00) + ((alpha_comp)&0x000000FF);

      /// --- Update x ----
      rat += x_ratio;
    }
  }
}

static void gfx_sdl_swap_buffers_begin(void) {

    if (!vsync_enabled) {
        sync_framerate_with_timer();
    }
#ifdef ENABLE_SOFTRAST
#ifdef DIRECT_SDL

#ifndef SDL_SURFACE
	sdl_screen->pixels = gfx_output;
#endif
	SDL_Flip(sdl_screen);
#else
    /*#ifndef SDL_SURFACE
    	SDL_BlitSurface(sdl_screen, NULL, texture, NULL);
    #else
    	SDL_BlitSurface(sdl_screen, NULL, texture, NULL);
    #endif*/

        if(half_res){
          SDL_Rect src_rect_halfRes = {0, 0, configScreenWidth/2, configScreenHeight/2};

          /** Cropped */
          //flip_Upscaling_Bilinear(sdl_screen, &src_rect_halfRes, texture, configScreenWidth*RES_HW_SCREEN_VERTICAL/configScreenHeight, RES_HW_SCREEN_VERTICAL);
          //flip_NNOptimized_AllowOutOfScreen(sdl_screen, &src_rect_halfRes, texture, configScreenWidth*RES_HW_SCREEN_VERTICAL/configScreenHeight, RES_HW_SCREEN_VERTICAL);

          /** Stretched */
          flip_NNOptimized_AllowOutOfScreen(sdl_screen, &src_rect_halfRes, texture, RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
        } 
        else {
          SDL_Rect src_rect_fullRes = {0, 0, configScreenWidth, configScreenHeight};

          /** Cropped */
          //flip_NNOptimized_AllowOutOfScreen(sdl_screen, &src_rect_fullRes, texture, configScreenWidth*RES_HW_SCREEN_VERTICAL/configScreenHeight, RES_HW_SCREEN_VERTICAL);

          /** Stretched */
          flip_NNOptimized_AllowOutOfScreen(sdl_screen, &src_rect_fullRes, texture, RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
        }

        //flip_NNOptimized_AllowOutOfScreen(sdl_screen, ptr_src_rect, texture, RES_HW_SCREEN_HORIZONTAL, RES_HW_SCREEN_VERTICAL);
        //flip_NNOptimized_AllowOutOfScreen(sdl_screen, ptr_src_rect, texture, 320, RES_HW_SCREEN_VERTICAL);
        //SDL_BlitSurface(sdl_screen, NULL, texture, NULL);
    	
        SDL_Flip(texture);
#endif
#else
    SDL_GL_SwapWindow(wnd);
#endif
}

static void gfx_sdl_swap_buffers_end(void) {
    f_frames++;

    if( (normal_speed_in_a_row > MAX_NORMAL_SPEED_IN_A_ROW || fullscreen_state) && half_res){
      //printf("Forcing full res\n");
      set_halfResScreen(false, true);
    }
    else if(too_slow_in_a_row > MAX_TOO_SLOW_IN_A_ROW && !half_res && !fullscreen_state){
      //printf("Forcing half res\n");
      set_halfResScreen(true, true);
    }
}

static double gfx_sdl_get_time(void) {
    return 0.0;
}

static void gfx_sdl_shutdown(void) {
  if(sdl_screen_fullRes){SDL_FreeSurface(sdl_screen_fullRes);}
  if(sdl_screen_halfRes){SDL_FreeSurface(sdl_screen_halfRes);}

  SDL_RemoveTimer(idTimer);

  const double elapsed = (SDL_GetTicks() - f_time) / 1000.0;
  printf("\nstats\n");
  printf("frames    %010d\n", f_frames);
  printf("time      %010.4lf sec\n", elapsed);
  printf("frametime %010.8lf sec\n", elapsed / (double)f_frames);
  printf("framerate %010.5lf fps\n\n", (double)f_frames / elapsed);
  fflush(stdout);
}

struct GfxWindowManagerAPI gfx_sdl = {
    gfx_sdl_init,
    gfx_sdl_set_keyboard_callbacks,
    gfx_sdl_set_fullscreen_changed_callback,
    gfx_sdl_set_fullscreen,
    gfx_sdl_main_loop,
    gfx_sdl_get_dimensions,
    gfx_sdl_handle_events,
    gfx_sdl_start_frame,
    gfx_sdl_swap_buffers_begin,
    gfx_sdl_swap_buffers_end,
    gfx_sdl_get_time,
    gfx_sdl_shutdown,
};

#endif
