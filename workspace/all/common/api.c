#include "defines.h"
#include "api.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <msettings.h>
#include <pthread.h>
#include <samplerate.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>

#include "utils.h"

#include <pthread.h>

///////////////////////////////

void LOG_note(int level, const char* fmt, ...) {
	char buf[1024] = {0};
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	switch(level) {
#ifdef DEBUG
	case LOG_DEBUG:
		printf("[DEBUG] %s", buf);
		break;
#endif
	case LOG_INFO:
		printf("[INFO] %s", buf);
		break;
	case LOG_WARN:
		fprintf(stderr, "[WARN] %s", buf);
		break;
	case LOG_ERROR:
		fprintf(stderr, "[ERROR] %s", buf);
		break;
	default:
		break;
	}
	fflush(stdout);
}

///////////////////////////////

uint32_t RGB_WHITE;
uint32_t RGB_BLACK;
uint32_t RGB_LIGHT_GRAY;
uint32_t RGB_GRAY;
uint32_t RGB_DARK_GRAY;
float currentbufferms = 20.0;
LightSettings lights[MAX_LIGHTS];

static struct GFX_Context {
	SDL_Surface* screen;
	SDL_Surface* assets;

	int mode;
	int vsync;
} gfx;

static SDL_Rect asset_rects[ASSET_COUNT];
static uint32_t asset_rgbs[ASSET_COLORS];
GFX_Fonts font;

///////////////////////////////

uint32_t THEME_COLOR1;
uint32_t THEME_COLOR2;
uint32_t THEME_COLOR3;
uint32_t THEME_COLOR1_255;
uint32_t THEME_COLOR2_255;
uint32_t THEME_COLOR3_255;
SDL_Color ALT_BUTTON_TEXT_COLOR;

// Function to convert hex color code to RGB and set the values
static inline uint32_t HexToUint(const char *hexColor) {
    int r, g, b;
    sscanf(hexColor, "%02x%02x%02x", &r, &g, &b);
	return SDL_MapRGB(gfx.screen->format, r, g, b);
}

static inline uint32_t HexToUint32_unmapped(const char *hexColor) {
    // Convert the hex string to an unsigned long
    uint32_t value = (uint32_t)strtoul(hexColor, NULL, 16);
    return value;
}

static inline SDL_Color UintToColour(uint32_t colour)
{
	SDL_Color tempcol;
	tempcol.a = 255;
	tempcol.r = (colour >> 16) & 0xFF;
	tempcol.g = (colour >> 8) & 0xFF;
	tempcol.b = colour & 0xFF;
	return tempcol;
}

static inline uint32_t UintMult(uint32_t color, uint32_t modulate_rgb)
{
	SDL_Color dest = UintToColour(color);
	SDL_Color modulate = UintToColour(modulate_rgb);

	dest.r = (int)dest.r * modulate.r / 255;
	dest.g = (int)dest.g * modulate.g / 255;
	dest.b = (int)dest.b * modulate.b / 255;

	return (dest.r << 16) | (dest.g << 8) | dest.b;
}

char *FONT_PATH;

MinUISettings settings;

void loadSettings() {

	FILE *file = PLAT_OpenSettings("minuisettings.txt");
	if (file == NULL)
    {
		LOG_info("Unable to open settings file\n");
		char hexColor[7];
		snprintf(hexColor, sizeof(hexColor), "%06x", 0xffffff);
		THEME_COLOR1_255 = HexToUint32_unmapped(hexColor);
		snprintf(hexColor, sizeof(hexColor), "%06x", 0x9b2257);
		THEME_COLOR2_255 = HexToUint32_unmapped(hexColor);
		snprintf(hexColor, sizeof(hexColor), "%06x", 0x1e2329);
		THEME_COLOR3_255 = HexToUint32_unmapped(hexColor);
		snprintf(hexColor, sizeof(hexColor), "%06x", 0xffffff);
		THEME_COLOR1 = HexToUint(hexColor);
		snprintf(hexColor, sizeof(hexColor), "%06x", 0x9b2257);
		THEME_COLOR2 = HexToUint(hexColor);
		snprintf(hexColor, sizeof(hexColor), "%06x", 0x1e2329);
		THEME_COLOR3 = HexToUint(hexColor);
		FONT_PATH = RES_PATH "/chillroundm.ttf";
	} 
	else {
		char line[256];
		int current_light = -1;
		while (fgets(line, sizeof(line), file))
		{
				int temp_value;
				uint32_t temp_color;
				if (sscanf(line, "font=%i", &temp_value) == 1)
				{
					LOG_info("ditte? %i \n",temp_value);
					if(temp_value==1) {
						FONT_PATH = RES_PATH "/chillroundm.ttf";
					} else {
						FONT_PATH = RES_PATH "/BPreplayBold-unhinted.otf";
					}
					continue;
				}
				if (sscanf(line, "color1=%x", &temp_color) == 1)
				{
					char hexColor[7];
					snprintf(hexColor, sizeof(hexColor), "%06x", temp_color);
					
					// Set RGB values
					THEME_COLOR1 = HexToUint(hexColor);
					THEME_COLOR1_255 = HexToUint32_unmapped(hexColor);
					continue;
				}
				if (sscanf(line, "color2=%x", &temp_color) == 1)
				{
					char hexColor[7];
					snprintf(hexColor, sizeof(hexColor), "%06x", temp_color);
					
					// Set RGB values
					THEME_COLOR2 = HexToUint(hexColor);
					THEME_COLOR2_255 = HexToUint32_unmapped(hexColor);
					continue;
				}
				if (sscanf(line, "color3=%x", &temp_color) == 1)
				{
					char hexColor[7];
					snprintf(hexColor, sizeof(hexColor), "%06x", temp_color);
					
					// Set RGB values
					THEME_COLOR3 = HexToUint(hexColor);
					THEME_COLOR3_255 = HexToUint32_unmapped(hexColor);
					continue;
				}
		}
		fclose(file);
	}

	ALT_BUTTON_TEXT_COLOR = UintToColour(THEME_COLOR3_255);

}




///////////////////////////////
static int qualityLevels[] = {
	3,
	4,
	2,
	1
};
static struct PWR_Context {
	int initialized;
	
	int can_sleep;
	int can_poweroff;
	int can_autosleep;
	int requested_sleep;
	int requested_wake;
	int resume_tick;
	
	pthread_t battery_pt;
	int is_charging;
	int charge;
	int should_warn;

	SDL_Surface* overlay;
} pwr = {0};


static struct SND_Context {
	int initialized;
	double frame_rate;
	
	int sample_rate_in;
	int sample_rate_out;
	
	SND_Frame* buffer;		// buf
	size_t frame_count; 	// buf_len
	
	int frame_in;     // buf_w
	int frame_out;    // buf_r
	int frame_filled; // max_buf_w
	
} snd = {0};

///////////////////////////////

static int _;

static double current_fps = SCREEN_FPS;
static int fps_counter = 0;
double currentfps = 0.0;
double currentreqfps = 0.0;

int currentbuffersize = 0;
int currentsampleratein = 0;
int currentsamplerateout = 0;
int should_rotate = 0;

SDL_Surface* GFX_init(int mode) {
	// TODO: this doesn't really belong here...
	// tried adding to PWR_init() but that was no good (not sure why)

	PLAT_initLid();
	LEDS_initLeds();
	LEDS_updateLeds();
	
	gfx.screen = PLAT_initVideo();
	gfx.vsync = VSYNC_STRICT;
	gfx.mode = mode;

	loadSettings();
	
	RGB_WHITE		= SDL_MapRGB(gfx.screen->format, TRIAD_WHITE);
	RGB_BLACK		= SDL_MapRGB(gfx.screen->format, TRIAD_BLACK);
	RGB_LIGHT_GRAY	= SDL_MapRGB(gfx.screen->format, TRIAD_LIGHT_GRAY);
	RGB_GRAY		= SDL_MapRGB(gfx.screen->format, TRIAD_GRAY);
	RGB_DARK_GRAY	= SDL_MapRGB(gfx.screen->format, TRIAD_DARK_GRAY);

	asset_rgbs[ASSET_WHITE_PILL]	= RGB_WHITE;
	asset_rgbs[ASSET_BLACK_PILL]	= RGB_BLACK;
	asset_rgbs[ASSET_DARK_GRAY_PILL]= RGB_DARK_GRAY;
	asset_rgbs[ASSET_OPTION]		= RGB_DARK_GRAY;
	asset_rgbs[ASSET_BUTTON]		= RGB_WHITE;
	asset_rgbs[ASSET_PAGE_BG]		= RGB_WHITE;
	asset_rgbs[ASSET_STATE_BG]		= RGB_WHITE;
	asset_rgbs[ASSET_PAGE]			= RGB_BLACK;
	asset_rgbs[ASSET_BAR]			= RGB_WHITE;
	asset_rgbs[ASSET_BAR_BG]		= RGB_BLACK;
	asset_rgbs[ASSET_BAR_BG_MENU]	= RGB_DARK_GRAY;
	asset_rgbs[ASSET_UNDERLINE]		= RGB_GRAY;
	asset_rgbs[ASSET_DOT]			= RGB_LIGHT_GRAY;
	asset_rgbs[ASSET_HOLE]			= RGB_BLACK;
	
	asset_rects[ASSET_WHITE_PILL]		= (SDL_Rect){SCALE4( 1, 1,30,30)};
	asset_rects[ASSET_BLACK_PILL]		= (SDL_Rect){SCALE4(33, 1,30,30)};
	asset_rects[ASSET_DARK_GRAY_PILL]	= (SDL_Rect){SCALE4(65, 1,30,30)};
	asset_rects[ASSET_OPTION]			= (SDL_Rect){SCALE4(97, 1,20,20)};
	asset_rects[ASSET_BUTTON]			= (SDL_Rect){SCALE4( 1,33,20,20)};
	asset_rects[ASSET_PAGE_BG]			= (SDL_Rect){SCALE4(64,33,15,15)};
	asset_rects[ASSET_STATE_BG]			= (SDL_Rect){SCALE4(23,54, 8, 8)};
	asset_rects[ASSET_PAGE]				= (SDL_Rect){SCALE4(39,54, 6, 6)};
	asset_rects[ASSET_BAR]				= (SDL_Rect){SCALE4(33,58, 4, 4)};
	asset_rects[ASSET_BAR_BG]			= (SDL_Rect){SCALE4(15,55, 4, 4)};
	asset_rects[ASSET_BAR_BG_MENU]		= (SDL_Rect){SCALE4(85,56, 4, 4)};
	asset_rects[ASSET_UNDERLINE]		= (SDL_Rect){SCALE4(85,51, 3, 3)};
	asset_rects[ASSET_DOT]				= (SDL_Rect){SCALE4(33,54, 2, 2)};
	asset_rects[ASSET_BRIGHTNESS]		= (SDL_Rect){SCALE4(23,33,19,19)};
	asset_rects[ASSET_VOLUME_MUTE]		= (SDL_Rect){SCALE4(44,33,10,16)};
	asset_rects[ASSET_VOLUME]			= (SDL_Rect){SCALE4(44,33,18,16)};
	asset_rects[ASSET_BATTERY]			= (SDL_Rect){SCALE4(47,51,17,10)};
	asset_rects[ASSET_BATTERY_LOW]		= (SDL_Rect){SCALE4(66,51,17,10)};
	asset_rects[ASSET_BATTERY_FILL]		= (SDL_Rect){SCALE4(81,33,12, 6)};
	asset_rects[ASSET_BATTERY_FILL_LOW]	= (SDL_Rect){SCALE4( 1,55,12, 6)};
	asset_rects[ASSET_BATTERY_BOLT]		= (SDL_Rect){SCALE4(81,41,12, 6)};
	asset_rects[ASSET_SCROLL_UP]		= (SDL_Rect){SCALE4(97,23,24, 6)};
	asset_rects[ASSET_SCROLL_DOWN]		= (SDL_Rect){SCALE4(97,31,24, 6)};
	asset_rects[ASSET_WIFI]				= (SDL_Rect){SCALE4(95,39,14,10)};
	asset_rects[ASSET_HOLE]				= (SDL_Rect){SCALE4( 1,63,20,20)};
	asset_rects[ASSET_GAMEPAD]			= (SDL_Rect){SCALE4(92,51,18,10)};
	
	char asset_path[MAX_PATH];
	sprintf(asset_path, RES_PATH "/assets@%ix.png", FIXED_SCALE);
	if (!exists(asset_path)) LOG_info("missing assets, you're about to segfault dummy!\n");
	gfx.assets = IMG_Load(asset_path);
	
	TTF_Init();
	font.large 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_LARGE));
	font.medium = TTF_OpenFont(FONT_PATH, SCALE1(FONT_MEDIUM));
	font.small 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_SMALL));
	font.tiny 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_TINY));
	
	TTF_SetFontStyle(font.large, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.medium, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.small, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.tiny, TTF_STYLE_BOLD);
	
	return gfx.screen;
}
void GFX_quit(void) {
	TTF_CloseFont(font.large);
	TTF_CloseFont(font.medium);
	TTF_CloseFont(font.small);
	TTF_CloseFont(font.tiny);
	
	SDL_FreeSurface(gfx.assets);
	
	GFX_freeAAScaler();
	
	GFX_clearAll();

	PLAT_quitVideo();
}

void GFX_setMode(int mode) {
	gfx.mode = mode;
}
int GFX_getVsync(void) {
	return gfx.vsync;
}
void GFX_setVsync(int vsync) {
	PLAT_setVsync(vsync);
	gfx.vsync = vsync;
}

int GFX_hdmiChanged(void) {
	static int had_hdmi = -1;
	int has_hdmi = GetHDMI();
	if (had_hdmi==-1) had_hdmi = has_hdmi;
	if (had_hdmi==has_hdmi) return 0;
	had_hdmi = has_hdmi;
	return 1;
}

#define FRAME_BUDGET 17 // 60fps
static uint32_t frame_start = 0;

static uint64_t per_frame_start = 0;
#define FPS_BUFFER_SIZE 50
void GFX_startFrame(void) {
	frame_start = SDL_GetTicks();
}


void chmodfile(const char *file, int writable)
{
    struct stat statbuf;
    if (stat(file, &statbuf) == 0)
    {
        mode_t newMode;
        if (writable)
        {
            // Add write permissions for all users
            newMode = statbuf.st_mode | S_IWUSR | S_IWGRP | S_IWOTH;
        }
        else
        {
            // Remove write permissions for all users
            newMode = statbuf.st_mode & ~(S_IWUSR | S_IWGRP | S_IWOTH);
        }

        // Apply the new permissions
        if (chmod(file, newMode) != 0)
        {
            printf("chmod error %d %s", writable, file);
        }
    }
    else
    {
        printf("stat error %d %s", writable, file);
    }
}

uint32_t GFX_extract_dominant_color(const void *data, unsigned width, unsigned height, size_t pitch) {
	if (!data) {
        fprintf(stderr, "Error: data is NULL.\n");
        return 0;
    }

    uint16_t *pixels = (uint16_t *)data;
    int pixel_count = width * height;

    uint64_t total_r = 0;
    uint64_t total_g = 0;
    uint64_t total_b = 0;
    uint8_t r, g, b;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t pixel = pixels[y * (pitch / 2) + x];  // Access pixel data

            // Manually extract RGB values from RGB565 format
            r = (pixel & 0xF800) >> 8;  // 5 bits red
            g = (pixel & 0x07E0) >> 3;  // 6 bits green
            b = (pixel & 0x001F) << 3;  // 5 bits blue

            // Normalize RGB values to 8 bits
            r |= r >> 5;
            g |= g >> 6;
            b |= b >> 5;

            // Accumulate the color components
            total_r += r;
            total_g += g;
            total_b += b;
        }
    }

    // Calculate the average color components
    uint8_t avg_r = total_r / pixel_count;
    uint8_t avg_g = total_g / pixel_count;
    uint8_t avg_b = total_b / pixel_count;

    // Combine average color components into a single uint32_t color
    uint32_t average_color = (avg_r << 16) | (avg_g << 8) | avg_b;

    return average_color;

}


void GFX_setAmbientColor(const void *data, unsigned width, unsigned height, size_t pitch,int mode) {
	if(mode==0) return;

	uint32_t dominant_color = GFX_extract_dominant_color(data, width, height,pitch);
   
	if(mode==1 || mode==2 || mode==5) {
		lights[2].color1 = dominant_color;
		lights[2].effect = 4;
		lights[2].brightness = 100;
	}
	if(mode==1 || mode==3) {
		lights[0].color1 = dominant_color;
		lights[0].effect = 4;
		lights[0].brightness = 100;
		lights[1].color1 = dominant_color;
		lights[1].effect = 4;
		lights[1].brightness = 100;
	}
	if(mode==1 || mode==4 || mode==5) {
		lights[3].color1 = dominant_color;
		lights[3].effect = 4;
		lights[3].brightness = 100;
	}
}

void GFX_flip(SDL_Surface* screen) {
	int should_vsync = (gfx.vsync!=VSYNC_OFF && (gfx.vsync==VSYNC_STRICT || frame_start==0 || SDL_GetTicks()-frame_start<FRAME_BUDGET));
	PLAT_flip(screen, should_vsync);
	
	currentfps = current_fps;
	fps_counter++;

	uint64_t performance_frequency = SDL_GetPerformanceFrequency();
	uint64_t frame_duration = SDL_GetPerformanceCounter() - per_frame_start;
	double elapsed_time_s = (double)frame_duration / performance_frequency;
	double tempfps = 1.0 / elapsed_time_s;
	if (!should_vsync) {
		uint64_t frame_time = performance_frequency / snd.frame_rate;  // Time per frame in performance counter units
		if (frame_duration < frame_time) {
			uint64_t delay_time = frame_time - frame_duration;  // Calculate the remaining time to wait
			SDL_Delay((1000 * delay_time) / performance_frequency);  // Convert to milliseconds and delay
		}
		per_frame_start = SDL_GetPerformanceCounter();
		return;
	}
	if(tempfps < SCREEN_FPS * 0.8 || tempfps > SCREEN_FPS * 1.2) tempfps = SCREEN_FPS;
	
	// filling with  60.1 cause i'd rather underrun than overflow in start phase
	static double fps_buffer[FPS_BUFFER_SIZE] = {60.1};
	static int buffer_index = 0;

	fps_buffer[buffer_index] = tempfps;
	buffer_index = (buffer_index + 1) % FPS_BUFFER_SIZE;
	// give it a little bit to stabilize and then use, meanwhile the buffer will
	// cover it
	if (fps_counter > 100) {
		double average_fps = 0.0;
		int fpsbuffersize = MIN(fps_counter, FPS_BUFFER_SIZE);
		for (int i = 0; i < fpsbuffersize; i++) {
			average_fps += fps_buffer[i];
		}
		average_fps /= fpsbuffersize;
		current_fps = average_fps;
	}
	
	per_frame_start = SDL_GetPerformanceCounter();
}
// eventually this function should be removed as its only here because of all the audio buffer based delay stuff
void GFX_sync(void) {
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (gfx.vsync!=VSYNC_OFF) {
		// this limiting condition helps SuperFX chip games
		if (gfx.vsync==VSYNC_STRICT || frame_start==0 || frame_duration<FRAME_BUDGET) { // only wait if we're under frame budget
			PLAT_vsync(FRAME_BUDGET-frame_duration);
		}
	}
	else {
		if (frame_duration<FRAME_BUDGET) SDL_Delay(FRAME_BUDGET-frame_duration);
	}
}
// if a fake vsycn delay is really needed 
void GFX_delay(void) {
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (frame_duration<((1/SCREEN_FPS) * 1000)) SDL_Delay(((1/SCREEN_FPS) * 1000)-frame_duration);
}

FALLBACK_IMPLEMENTATION int PLAT_supportsOverscan(void) { return 0; }
FALLBACK_IMPLEMENTATION void PLAT_setEffectColor(int next_color) { }

int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding) {
	int text_width;
	strcpy(out_name, in_name);
	TTF_SizeUTF8(font, out_name, &text_width, NULL);
	text_width += padding;
	
	while (text_width>max_width) {
		int len = strlen(out_name);
		strcpy(&out_name[len-4], "...\0");
		TTF_SizeUTF8(font, out_name, &text_width, NULL);
		text_width += padding;
	}
	
	return text_width;
}
int GFX_getTextHeight(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding) {
	int text_height;
	strcpy(out_name, in_name);
	TTF_SizeUTF8(font, out_name, NULL, &text_height);
	text_height += padding;
	
	return text_height;
}
int GFX_getTextWidth(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding) {
	int text_width;
	strcpy(out_name, in_name);
	TTF_SizeUTF8(font, out_name, &text_width, NULL);
	text_width += padding;
	
	return text_width;
}

// scrolling text stuff
static int text_offset = 0;

void GFX_resetScrollText() {
	text_offset = 0;
}
void GFX_scrollTextSurface(TTF_Font* font, const char* in_name, SDL_Surface** out_surface, int max_width, int padding, SDL_Color color, float transparency) {
    
    static int frame_counter = 0;
    int text_width, text_height;

    TTF_SizeUTF8(font, in_name, &text_width, &text_height);

    int full_text_width = text_width + padding - 15; 

    // Ensure transparency is within 0 to 1 range
    if (transparency < 0.0f) transparency = 0.0f;
    if (transparency > 1.0f) transparency = 1.0f;

    // Convert transparency float (0 to 1) into an alpha value (0 to 255)
    Uint8 alpha = (Uint8)(transparency * 255);
    color.a = alpha;  // Apply transparency to text color

    char scroll_text[1024]; 
    snprintf(scroll_text, sizeof(scroll_text), "%s  %s", in_name, in_name); 

    SDL_Surface* full_text_surface = TTF_RenderUTF8_Blended(font, scroll_text, color);
    if (!full_text_surface) {
        printf("Text rendering failed: %s\n", TTF_GetError());
        return;
    }

    if (text_width < max_width) {
        text_offset = 0;  
    }

    SDL_Rect src_rect = { text_offset, 0, max_width, full_text_surface->h };
    SDL_Surface* scrolling_surface = SDL_CreateRGBSurface(0, max_width, full_text_surface->h, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    
    // Enable alpha blending on the new surface
    SDL_SetSurfaceBlendMode(scrolling_surface, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(full_text_surface, &src_rect, scrolling_surface, NULL);
    
    // Apply transparency to the output surface
    SDL_SetSurfaceAlphaMod(scrolling_surface, alpha);

    SDL_FreeSurface(full_text_surface);

    if (*out_surface) {
        SDL_FreeSurface(*out_surface);
    }
    *out_surface = scrolling_surface;

    if (text_width + padding * 2 > max_width) {
        frame_counter++;
        if (frame_counter >= 0) {  
            text_offset += 2;  
            if (text_offset >= full_text_width) {
                text_offset = 0; 
            }
            frame_counter = 0;
        }
    }
}


int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines) {
	if (!str) return 0;
	
	int line_width;
	int max_line_width = 0;
	char* line = str;
	char buffer[MAX_PATH];
	
	TTF_SizeUTF8(font, line, &line_width, NULL);
	if (line_width<=max_width) {
		line_width = GFX_truncateText(font,line,buffer,max_width,0);
		strcpy(line,buffer);
		return line_width;
	}
	
	char* prev = NULL;
	char* tmp = line;
	int lines = 1;
	int i = 0;
	while (!max_lines || lines<max_lines) {
		tmp = strchr(tmp, ' ');
		if (!tmp) {
			if (prev) {
				TTF_SizeUTF8(font, line, &line_width, NULL);
				if (line_width>=max_width) {
					if (line_width>max_line_width) max_line_width = line_width;
					prev[0] = '\n';
					line = prev + 1;
				}
			}
			break;
		}
		tmp[0] = '\0';
		
		TTF_SizeUTF8(font, line, &line_width, NULL);

		if (line_width>=max_width) { // wrap
			if (line_width>max_line_width) max_line_width = line_width;
			tmp[0] = ' ';
			tmp += 1;
			prev[0] = '\n';
			prev += 1;
			line = prev;
			lines += 1;
		}
		else { // continue
			tmp[0] = ' ';
			prev = tmp;
			tmp += 1;
		}
		i += 1;
	}
	
	line_width = GFX_truncateText(font,line,buffer,max_width,0);
	strcpy(line,buffer);
	
	if (line_width>max_line_width) max_line_width = line_width;
	return max_line_width;
}

///////////////////////////////

// scale_blend (and supporting logic) from picoarch

struct blend_args {
	int w_ratio_in;
	int w_ratio_out;
	uint16_t w_bp[2];
	int h_ratio_in;
	int h_ratio_out;
	uint16_t h_bp[2];
	uint16_t *blend_line;
} blend_args;

#if __ARM_ARCH >= 5
static inline uint32_t average16(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x0821;
	asm ("eor %0, %2, %3\r\n"
	     "and %0, %0, %1\r\n"
	     "add %0, %3, %0\r\n"
	     "add %0, %0, %2\r\n"
	     "lsr %0, %0, #1\r\n"
	     : "=&r" (ret) : "r" (lowbits), "r" (c1), "r" (c2) : );
	return ret;
}
static inline uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x08210821;

	asm ("eor %0, %3, %1\r\n"
	     "and %0, %0, %2\r\n"
	     "adds %0, %1, %0\r\n"
	     "and %1, %1, #0\r\n"
	     "movcs %1, #0x80000000\r\n"
	     "adds %0, %0, %3\r\n"
	     "rrx %0, %0\r\n"
	     "orr %0, %0, %1\r\n"
	     : "=&r" (ret), "+r" (c2) : "r" (lowbits), "r" (c1) : "cc" );

	return ret;
}

#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#else

static inline uint32_t average16(uint32_t c1, uint32_t c2) {
	return (c1 + c2 + ((c1 ^ c2) & 0x0821))>>1;
}
static inline uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t sum = c1 + c2;
	uint32_t ret = sum + ((c1 ^ c2) & 0x08210821);
	uint32_t of = ((sum < c1) | (ret < sum)) << 31;

	return (ret >> 1) | of;
}

#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#endif

#define AVERAGE16(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE16_NOCHK((c1), (c2)))
#define AVERAGE16_1_3(c1, c2) ((c1) == (c2) ? (c1) : (AVERAGE16_NOCHK(AVERAGE16_NOCHK((c1), (c2)), (c2))))

#define AVERAGE32(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE32_NOCHK((c1), (c2)))
#define AVERAGE32_1_3(c1, c2) ((c1) == (c2) ? (c1) : (AVERAGE32_NOCHK(AVERAGE32_NOCHK((c1), (c2)), (c2))))

static inline int gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

static void scaleAA(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_p) {
	int dy = 0;
	int lines = h;

	int rat_w = blend_args.w_ratio_in;
	int rat_dst_w = blend_args.w_ratio_out;
	uint16_t *bw = blend_args.w_bp;

	int rat_h = blend_args.h_ratio_in;
	int rat_dst_h = blend_args.h_ratio_out;
	uint16_t *bh = blend_args.h_bp;

	while (lines--) {
		while (dy < rat_dst_h) {
			uint16_t *dst16 = (uint16_t *)dst;
			uint16_t *pblend = (uint16_t *)blend_args.blend_line;
			int col = w;
			int dx = 0;

			uint16_t *pnext = (uint16_t *)(src + pitch);
			if (!lines)
				pnext -= (pitch / sizeof(uint16_t));

			if (dy > rat_dst_h - bh[0]) {
				pblend = pnext;
			} else if (dy <= bh[0]) {
				/* Drops const, won't get touched later though */
				pblend = (uint16_t *)src;
			} else {
				const uint32_t *src32 = (const uint32_t *)src;
				const uint32_t *pnext32 = (const uint32_t *)pnext;
				uint32_t *pblend32 = (uint32_t *)pblend;
				int count = w / 2;

				if (dy <= bh[1]) {
					const uint32_t *tmp = pnext32;
					pnext32 = src32;
					src32 = tmp;
				}

				if (dy > rat_dst_h - bh[1] || dy <= bh[1]) {
					while(count--) {
						*pblend32++ = AVERAGE32_1_3(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				} else {
					while(count--) {
						*pblend32++ = AVERAGE32(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				}
			}

			while (col--) {
				uint16_t a, b, out;

				a = *pblend;
				b = *(pblend+1);

				while (dx < rat_dst_w) {
					if (a == b) {
						out = a;
					} else if (dx > rat_dst_w - bw[0]) { // top quintile, bbbb
						out = b;
					} else if (dx <= bw[0]) { // last quintile, aaaa
						out = a;
					} else {
						if (dx > rat_dst_w - bw[1]) { // 2nd quintile, abbb
							a = AVERAGE16_NOCHK(a,b);
						} else if (dx <= bw[1]) { // 4th quintile, aaab
							b = AVERAGE16_NOCHK(a,b);
						}

						out = AVERAGE16_NOCHK(a, b); // also 3rd quintile, aabb
					}
					*dst16++ = out;
					dx += rat_w;
				}

				dx -= rat_dst_w;
				pblend++;
			}

			dy += rat_h;
			dst += dst_p;
		}

		dy -= rat_dst_h;
		src += pitch;
	}
}

scaler_t GFX_getAAScaler(GFX_Renderer* renderer) {
	int gcd_w, div_w, gcd_h, div_h;
	blend_args.blend_line = calloc(renderer->src_w, sizeof(uint16_t));

	gcd_w = gcd(renderer->src_w, renderer->dst_w);
	blend_args.w_ratio_in = renderer->src_w / gcd_w;
	blend_args.w_ratio_out = renderer->dst_w / gcd_w;
	
	double blend_denominator = (renderer->src_w>renderer->dst_w) ? 5 : 2.5; // TODO: these values are really only good for the nano...
	// blend_denominator = 5.0; // better for trimui
	// LOG_info("blend_denominator: %f (%i && %i)\n", blend_denominator, HAS_SKINNY_SCREEN, renderer->dst_w>renderer->src_w);
	
	div_w = round(blend_args.w_ratio_out / blend_denominator);
	blend_args.w_bp[0] = div_w;
	blend_args.w_bp[1] = blend_args.w_ratio_out >> 1;

	gcd_h = gcd(renderer->src_h, renderer->dst_h);
	blend_args.h_ratio_in = renderer->src_h / gcd_h;
	blend_args.h_ratio_out = renderer->dst_h / gcd_h;

	div_h = round(blend_args.h_ratio_out / blend_denominator);
	blend_args.h_bp[0] = div_h;
	blend_args.h_bp[1] = blend_args.h_ratio_out >> 1;
	
	return scaleAA;
}
void GFX_freeAAScaler(void) {
	if (blend_args.blend_line != NULL) {
		free(blend_args.blend_line);
		blend_args.blend_line = NULL;
	}
}

///////////////////////////////

void GFX_ApplyRounderCorners(SDL_Surface* surface, int radius) {
	if (!surface) return;

    Uint32* pixels = (Uint32*)surface->pixels;
    int width = surface->w;
    int height = surface->h;
    SDL_PixelFormat* fmt = surface->format;
    
    Uint32 transparent_black = SDL_MapRGBA(fmt, 0, 0, 0, 0);  // Fully transparent black

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int dx = (x < radius) ? radius - x : (x >= width - radius) ? x - (width - radius - 1) : 0;
            int dy = (y < radius) ? radius - y : (y >= height - radius) ? y - (height - radius - 1) : 0;
            if (dx * dx + dy * dy > radius * radius) {
                pixels[y * width + x] = transparent_black;  // Set to fully transparent black
            }
        }
    }
}



void GFX_blitAssetColor(int asset, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect, uint32_t asset_color) {

	SDL_Rect* rect = &asset_rects[asset];
	SDL_Rect adj_rect = {
		.x = rect->x,
		.y = rect->y,
		.w = rect->w,
		.h = rect->h,
	};
	if (src_rect) {
		adj_rect.x += src_rect->x;
		adj_rect.y += src_rect->y;
		adj_rect.w  = src_rect->w;
		adj_rect.h  = src_rect->h;
	}

	// This could be a RAII
	if(asset_color != RGB_WHITE)
	{
		// TODO: Is there a neat way to get the opposite effect of SDL_MapRGB?
		// This is kinda ugly and not very generic.
		if(asset_color == THEME_COLOR1)
			asset_color = THEME_COLOR1_255;
		else if(asset_color == THEME_COLOR2)
			asset_color = THEME_COLOR2_255;
		else if(asset_color == THEME_COLOR3)
			asset_color = THEME_COLOR3_255;

		SDL_Color restore;
		SDL_GetSurfaceColorMod(gfx.assets, &restore.r, &restore.g, &restore.b);
		SDL_SetSurfaceColorMod(gfx.assets, 
			(asset_color >> 16) & 0xFF, 
			(asset_color >> 8) & 0xFF, 
			asset_color & 0xFF);
		SDL_BlitSurface(gfx.assets, &adj_rect, dst, dst_rect);
		SDL_SetSurfaceColorMod(gfx.assets, restore.r, restore.g, restore.b);
	}
	else {
		SDL_BlitSurface(gfx.assets, &adj_rect, dst, dst_rect);
	}
}
void GFX_blitAsset(int asset, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect) {
	GFX_blitAssetColor(asset, src_rect, dst, dst_rect, RGB_WHITE);
}
void GFX_blitPillColor(int asset, SDL_Surface* dst, SDL_Rect* dst_rect, uint32_t asset_color, uint32_t fill_color) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;

	if (h==0) h = asset_rects[asset].h;
	
	int r = h / 2;
	if (w < h) w = h;
	w -= h;
	
	GFX_blitAssetColor(asset, &(SDL_Rect){0,0,r,h}, dst, &(SDL_Rect){x,y}, asset_color);
	x += r;
	if (w>0) {
		SDL_FillRect(dst, &(SDL_Rect){x,y,w,h}, UintMult(fill_color, asset_color));
		//SDL_FillRect(dst, &(SDL_Rect){x,y,w,h}, asset_color);
		x += w;
	}
	GFX_blitAssetColor(asset, &(SDL_Rect){r,0,r,h}, dst, &(SDL_Rect){x,y}, asset_color);
}
void GFX_blitPill(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	GFX_blitPillColor(asset, dst, dst_rect, RGB_WHITE, asset_rgbs[asset]);
}
void GFX_blitPillLight(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	GFX_blitPillColor(asset, dst, dst_rect, THEME_COLOR2, RGB_WHITE);
}
void GFX_blitPillDark(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	GFX_blitPillColor(asset, dst, dst_rect, THEME_COLOR1, RGB_WHITE);
}
void GFX_blitRect(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;
	int c = asset_rgbs[asset];
	
	SDL_Rect* rect = &asset_rects[asset];
	int d = rect->w;
	int r = d / 2;

	GFX_blitAssetColor(asset, &(SDL_Rect){0,0,r,r}, dst, &(SDL_Rect){x,y}, THEME_COLOR1);
	SDL_FillRect(dst, &(SDL_Rect){x+r,y,w-d,r}, c);
	GFX_blitAssetColor(asset, &(SDL_Rect){r,0,r,r}, dst, &(SDL_Rect){x+w-r,y}, THEME_COLOR1);
	SDL_FillRect(dst, &(SDL_Rect){x,y+r,w,h-d}, c);
	GFX_blitAssetColor(asset, &(SDL_Rect){0,r,r,r}, dst, &(SDL_Rect){x,y+h-r}, THEME_COLOR1);
	SDL_FillRect(dst, &(SDL_Rect){x+r,y+h-r,w-d,r}, c);
	GFX_blitAssetColor(asset, &(SDL_Rect){r,r,r,r}, dst, &(SDL_Rect){x+w-r,y+h-r}, THEME_COLOR1);
}
void GFX_blitBattery(SDL_Surface* dst, SDL_Rect* dst_rect) {
	// LOG_info("dst: %p\n", dst);
	int x = 0;
	int y = 0;
	if (dst_rect) {
		x = dst_rect->x;
		y = dst_rect->y;
	}
	SDL_Rect rect = asset_rects[ASSET_BATTERY];
	x += (SCALE1(PILL_SIZE) - (rect.w + FIXED_SCALE)) / 2;
	y += (SCALE1(PILL_SIZE) - rect.h) / 2;
	
	if (pwr.is_charging) {
		GFX_blitAssetColor(ASSET_BATTERY, NULL, dst, &(SDL_Rect){x,y}, THEME_COLOR1);
		GFX_blitAssetColor(ASSET_BATTERY_BOLT, NULL, dst, &(SDL_Rect){x+SCALE1(3),y+SCALE1(2)}, THEME_COLOR1);
	}
	else {
		int percent = pwr.charge;
		GFX_blitAssetColor(percent<=10?ASSET_BATTERY_LOW:ASSET_BATTERY, NULL, dst, &(SDL_Rect){x,y}, THEME_COLOR1);
		
		rect = asset_rects[ASSET_BATTERY_FILL];
		SDL_Rect clip = rect;
		clip.w *= percent;
		clip.w /= 100;
		if (clip.w<=0) return;
		clip.x = rect.w - clip.w;
		clip.y = 0;
		
		GFX_blitAssetColor(percent<=20?ASSET_BATTERY_FILL_LOW:ASSET_BATTERY_FILL, &clip, dst, &(SDL_Rect){x+SCALE1(3)+clip.x,y+SCALE1(2)}, THEME_COLOR1);
	}
}
int GFX_getButtonWidth(char* hint, char* button) {
	int button_width = 0;
	int width;
	
	int special_case = !strcmp(button,BRIGHTNESS_BUTTON_LABEL); // TODO: oof
	
	if (strlen(button)==1) {
		button_width += SCALE1(BUTTON_SIZE);
	}
	else {
		button_width += SCALE1(BUTTON_SIZE) / 2;
		TTF_SizeUTF8(special_case ? font.large : font.tiny, button, &width, NULL);
		button_width += width;
	}
	button_width += SCALE1(BUTTON_MARGIN);
	
	TTF_SizeUTF8(font.small, hint, &width, NULL);
	button_width += width + SCALE1(BUTTON_MARGIN);
	return button_width;
}
void GFX_blitButton(char* hint, char*button, SDL_Surface* dst, SDL_Rect* dst_rect) {
	SDL_Surface* text;
	int ox = 0;
	
	int special_case = !strcmp(button,BRIGHTNESS_BUTTON_LABEL); // TODO: oof
	
	// button
	if (strlen(button)==1) {
		GFX_blitAssetColor(ASSET_BUTTON, NULL, dst, dst_rect, THEME_COLOR1);

		// label
		text = TTF_RenderUTF8_Blended(font.medium, button, ALT_BUTTON_TEXT_COLOR);
		SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){dst_rect->x+(SCALE1(BUTTON_SIZE)-text->w)/2,dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2});
		ox += SCALE1(BUTTON_SIZE);
		SDL_FreeSurface(text);
	}
	else {
		text = TTF_RenderUTF8_Blended(special_case ? font.large : font.tiny, button, ALT_BUTTON_TEXT_COLOR);
		GFX_blitPillDark(ASSET_BUTTON, dst, &(SDL_Rect){dst_rect->x,dst_rect->y,SCALE1(BUTTON_SIZE)/2+text->w,SCALE1(BUTTON_SIZE)});
		ox += SCALE1(BUTTON_SIZE)/4;
		
		int oy = special_case ? SCALE1(-2) : 0;
		SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2,text->w,text->h});
		ox += text->w;
		ox += SCALE1(BUTTON_SIZE)/4;
		SDL_FreeSurface(text);
	}
	
	ox += SCALE1(BUTTON_MARGIN);

	// hint text
	text = TTF_RenderUTF8_Blended(font.small, hint, COLOR_WHITE);
	SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){ox+dst_rect->x,dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2,text->w,text->h});
	SDL_FreeSurface(text);
}
void GFX_blitMessage(TTF_Font* font, char* msg, SDL_Surface* dst, SDL_Rect* dst_rect) {
	if (!dst_rect) dst_rect = &(SDL_Rect){0,0,dst->w,dst->h};
	
	// LOG_info("GFX_blitMessage: %p (%ix%i)", dst, dst_rect->w,dst_rect->h);
	
	SDL_Surface* text;
#define TEXT_BOX_MAX_ROWS 16
#define LINE_HEIGHT 24
	char* rows[TEXT_BOX_MAX_ROWS];
	int row_count = 0;

	char* tmp;
	rows[row_count++] = msg;
	while ((tmp=strchr(rows[row_count-1], '\n'))!=NULL) {
		if (row_count+1>=TEXT_BOX_MAX_ROWS) return; // TODO: bail
		rows[row_count++] = tmp+1;
	}
	
	int rendered_height = SCALE1(LINE_HEIGHT) * row_count;
	int y = dst_rect->y;
	y += (dst_rect->h - rendered_height) / 2;
	
	char line[256];
	for (int i=0; i<row_count; i++) {
		int len;
		if (i+1<row_count) {
			len = rows[i+1]-rows[i]-1;
			if (len) strncpy(line, rows[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(rows[i]);
			strcpy(line, rows[i]);
		}
		
		
		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, COLOR_WHITE);
			int x = dst_rect->x;
			x += (dst_rect->w - text->w) / 2;
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x,y});
			SDL_FreeSurface(text);
		}
		y += SCALE1(LINE_HEIGHT);
	}
}

int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting) {
	int ox;
	int oy;
	int ow = 0;
	
	int setting_value;
	int setting_min;
	int setting_max;
	
	if (show_setting && !GetHDMI()) {
		ow = SCALE1(PILL_SIZE + SETTINGS_WIDTH + 10 + 4);
		ox = dst->w - SCALE1(PADDING) - ow;
		oy = SCALE1(PADDING);
		GFX_blitPillColor(ASSET_WHITE_PILL, dst, &(SDL_Rect){
			ox,
			oy,
			ow,
			SCALE1(PILL_SIZE)
		},THEME_COLOR2, RGB_WHITE);
		
		if (show_setting==1) {
			setting_value = GetBrightness();
			setting_min = BRIGHTNESS_MIN;
			setting_max = BRIGHTNESS_MAX;
		}
		else if (show_setting==3) {
			setting_value = GetColortemp();
			setting_min = COLORTEMP_MIN;
			setting_max = COLORTEMP_MAX;
		}
		else {
			setting_value = GetVolume();
			setting_min = VOLUME_MIN;
			setting_max = VOLUME_MAX;
		}
		
		int asset = show_setting==3?ASSET_BUTTON:show_setting==1?ASSET_BRIGHTNESS:(setting_value>0?ASSET_VOLUME:ASSET_VOLUME_MUTE);
		int ax = ox + (show_setting==1 || show_setting == 3 ? SCALE1(6) : SCALE1(8));
		int ay = oy + (show_setting==1 || show_setting == 3 ? SCALE1(5) : SCALE1(7));
		GFX_blitAssetColor(asset, NULL, dst, &(SDL_Rect){ax,ay}, THEME_COLOR1);
		
		ox += SCALE1(PILL_SIZE);
		oy += SCALE1((PILL_SIZE - SETTINGS_SIZE) / 2);
		GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_BAR_BG : ASSET_BAR_BG_MENU, dst, &(SDL_Rect){
			ox,
			oy,
			SCALE1(SETTINGS_WIDTH),
			SCALE1(SETTINGS_SIZE)
		});
		
		float percent = ((float)(setting_value-setting_min) / (setting_max-setting_min));
		if (show_setting==1 || show_setting==3 || setting_value>0) {
			GFX_blitPillDark(ASSET_BAR, dst, &(SDL_Rect){
				ox,
				oy,
				SCALE1(SETTINGS_WIDTH) * percent,
				SCALE1(SETTINGS_SIZE)
			});
		}
	}
	else {
		// TODO: handle wifi
		int show_wifi = PLAT_isOnline(); // NOOOOO! not every frame!

		int ww = SCALE1(PILL_SIZE-3);
		ow = SCALE1(PILL_SIZE);
		if (show_wifi) ow += ww;

		ox = dst->w - SCALE1(PADDING) - ow;
		oy = SCALE1(PADDING);
		GFX_blitPillColor(ASSET_WHITE_PILL, dst, &(SDL_Rect){
			ox,
			oy,
			ow,
			SCALE1(PILL_SIZE)
		}, THEME_COLOR2, RGB_WHITE);
		if (show_wifi) {
			SDL_Rect rect = asset_rects[ASSET_WIFI];
			int x = ox;
			int y = oy;
			x += (SCALE1(PILL_SIZE) - rect.w) / 2;
			y += (SCALE1(PILL_SIZE) - rect.h) / 2;
			
			GFX_blitAssetColor(ASSET_WIFI, NULL, dst, &(SDL_Rect){x,y}, THEME_COLOR1);
			ox += ww;
		}
		GFX_blitBattery(dst, &(SDL_Rect){ox,oy});
	}
	
	return ow;
}
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting) {

		if (show_setting==1) GFX_blitButtonGroup((char*[]){ BRIGHTNESS_BUTTON_LABEL,"BRIGHTNESS",  NULL }, 0, dst, 0);
		else if (show_setting==3) GFX_blitButtonGroup((char*[]){ BRIGHTNESS_BUTTON_LABEL,"COLOR TEMP",  NULL }, 0, dst, 0);
		else GFX_blitButtonGroup((char*[]){ "MNU","BRGHT","SEL","CLTMP",  NULL }, 0, dst, 0);
	
}

int GFX_blitButtonGroup(char** pairs, int primary, SDL_Surface* dst, int align_right) {
	int ox;
	int oy;
	int ow;
	char* hint;
	char* button;

	struct Hint {
		char* hint;
		char* button;
		int ow;
	} hints[2]; 
	int w = 0; // individual button dimension
	int h = 0; // hints index
	ow = 0; // full pill width
	ox = align_right ? dst->w - SCALE1(PADDING) : SCALE1(PADDING);
	oy = dst->h - SCALE1(PADDING + PILL_SIZE);
	
	for (int i=0; i<2; i++) {
		if (!pairs[i*2]) break;
		if (HAS_SKINNY_SCREEN && i!=primary) continue; // space saving
		
		button = pairs[i * 2];
		hint = pairs[i * 2 + 1];
		w = GFX_getButtonWidth(hint, button);
		hints[h].hint = hint;
		hints[h].button = button;
		hints[h].ow = w;
		h += 1;
		ow += SCALE1(BUTTON_MARGIN) + w;
	}
	
	ow += SCALE1(BUTTON_MARGIN);
	if (align_right) ox -= ow;
	GFX_blitPillColor(ASSET_WHITE_PILL, dst, &(SDL_Rect){
		ox,
		oy,
		ow,
		SCALE1(PILL_SIZE)
	}, THEME_COLOR2, RGB_WHITE);
	
	ox += SCALE1(BUTTON_MARGIN);
	oy += SCALE1(BUTTON_MARGIN);
	for (int i=0; i<h; i++) {
		GFX_blitButton(hints[i].hint, hints[i].button, dst, &(SDL_Rect){ox,oy});
		ox += hints[i].ow + SCALE1(BUTTON_MARGIN);
	}
	return ow;
}

#define MAX_TEXT_LINES 16
void GFX_sizeText(TTF_Font* font, char* str, int leading, int* w, int* h) {
	char* lines[MAX_TEXT_LINES];
	int count = 0;

	char* tmp;
	lines[count++] = str;
	while ((tmp=strchr(lines[count-1], '\n'))!=NULL) {
		if (count+1>MAX_TEXT_LINES) break; // TODO: bail?
		lines[count++] = tmp+1;
	}
	*h = count * leading;
	
	int mw = 0;
	char line[256];
	for (int i=0; i<count; i++) {
		int len;
		if (i+1<count) {
			len = lines[i+1]-lines[i]-1;
			if (len) strncpy(line, lines[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}
		
		if (len) {
			int lw;
			TTF_SizeUTF8(font, line, &lw, NULL);
			if (lw>mw) mw = lw;
		}
	}
	*w = mw;
}
void GFX_blitText(TTF_Font* font, char* str, int leading, SDL_Color color, SDL_Surface* dst, SDL_Rect* dst_rect) {
	if (dst_rect==NULL) dst_rect = &(SDL_Rect){0,0,dst->w,dst->h};
	
	char* lines[MAX_TEXT_LINES];
	int count = 0;

	char* tmp;
	lines[count++] = str;
	while ((tmp=strchr(lines[count-1], '\n'))!=NULL) {
		if (count+1>MAX_TEXT_LINES) break; // TODO: bail?
		lines[count++] = tmp+1;
	}
	int x = dst_rect->x;
	int y = dst_rect->y;
	
	SDL_Surface* text;
	char line[256];
	for (int i=0; i<count; i++) {
		int len;
		if (i+1<count) {
			len = lines[i+1]-lines[i]-1;
			if (len) strncpy(line, lines[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}
		
		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, color);
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x+((dst_rect->w-text->w)/2),y+(i*leading)});
			SDL_FreeSurface(text);
		}
	}
}

///////////////////////////////

// based on picoarch's audio 
// implementation, rewritten 
// to (try to) understand it 
// better

#define MAX_SAMPLE_RATE 48000
#define BATCH_SIZE 100
#ifndef SAMPLES
	#define SAMPLES 512 // default
#endif

#define ms SDL_GetTicks



pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;

static void SND_audioCallback(void *userdata, uint8_t *stream, int len) {
	if (snd.frame_count == 0)
		return;

	int16_t *out = (int16_t *)stream;
	len /= (sizeof(int16_t) * 2);

	// Lock the mutex before accessing shared resources
	

	while (snd.frame_out != snd.frame_in && len > 0) {
		
		*out++ = snd.buffer[snd.frame_out].left;
		*out++ = snd.buffer[snd.frame_out].right;
		pthread_mutex_lock(&audio_mutex);
		snd.frame_out += 1;
		len -= 1;
		if (snd.frame_out >= snd.frame_count)
			snd.frame_out = 0;
		pthread_mutex_unlock(&audio_mutex);
	}
	

	if (len > 0) {
		memset(out, 0, len * (sizeof(int16_t) * 2));
	}
}
static void SND_resizeBuffer(void) { // plat_sound_resize_buffer

	if (snd.frame_count == 0)
		return;

	SDL_LockAudio();

	int buffer_bytes = snd.frame_count * sizeof(SND_Frame);
	snd.buffer = realloc(snd.buffer, buffer_bytes);

	memset(snd.buffer, 0, buffer_bytes);

	snd.frame_in = 0;
	snd.frame_out = 0;

	SDL_UnlockAudio();
}
static int soundQuality = 2;
static int resetSrcState = 0;
void SND_setQuality(int quality) {
	soundQuality = qualityLevels[quality];
	resetSrcState = 1;
}
ResampledFrames resample_audio(const SND_Frame *input_frames,
	int input_frame_count, int input_sample_rate,
	int output_sample_rate, double ratio) {
	int error;
	static double previous_ratio = 1.0;
	static SRC_STATE *src_state = NULL;

	if (!src_state || resetSrcState) {
		resetSrcState = 0;
		src_state = src_new(soundQuality, 2, &error);
		if (src_state == NULL) {
			fprintf(stderr, "Error initializing SRC state: %s\n",
				src_strerror(error));
			exit(1);
		}
	}

	if (previous_ratio != ratio) {
		if (src_set_ratio(src_state, ratio) != 0) {
			fprintf(stderr, "Error setting resampling ratio: %s\n",
				src_strerror(src_error(src_state)));
			exit(1);
		}

		previous_ratio = ratio;
	}

	int max_output_frames = (int)(input_frame_count * ratio + 1);

	float *input_buffer = malloc(input_frame_count * 2 * sizeof(float));
	float *output_buffer = malloc(max_output_frames * 2 * sizeof(float));
	if (!input_buffer || !output_buffer) {
		fprintf(stderr, "Error allocating buffers\n");
		free(input_buffer);
		free(output_buffer);
		src_delete(src_state);
		exit(1);
	}

	for (int i = 0; i < input_frame_count; i++) {
		input_buffer[2 * i] = input_frames[i].left / 32768.0f;
		input_buffer[2 * i + 1] = input_frames[i].right / 32768.0f;
	}

	SRC_DATA src_data = {.data_in = input_buffer,
		.data_out = output_buffer,
		.input_frames = input_frame_count,
		.output_frames = max_output_frames,
		.src_ratio = ratio,
		.end_of_input = 0};

	// Perform resampling
	if (src_process(src_state, &src_data) != 0) {
		fprintf(stderr, "Error resampling: %s\n",
			src_strerror(src_error(src_state)));
		free(input_buffer);
		free(output_buffer);
		exit(1);
	}

	int output_frame_count = src_data.output_frames_gen;

	SND_Frame *output_frames = malloc(output_frame_count * sizeof(SND_Frame));
	if (!output_frames) {
		fprintf(stderr, "Error allocating output frames\n");
		free(input_buffer);
		free(output_buffer);
		exit(1);
	}

	for (int i = 0; i < output_frame_count; i++) {
		float left = output_buffer[2 * i];
		float right = output_buffer[2 * i + 1];

		left = fmaxf(-1.0f, fminf(1.0f, left));
		right = fmaxf(-1.0f, fminf(1.0f, right));

		output_frames[i].left = (int16_t)(left * 32767.0f);
		output_frames[i].right = (int16_t)(right * 32767.0f);
	}

	// Free temporary buffers
	free(input_buffer);
	free(output_buffer);

	// Return the resampled frames
	ResampledFrames resampled;
	resampled.frames = output_frames;
	resampled.frame_count = output_frame_count;

	return resampled;
}


#define ROLLING_AVERAGE_WINDOW_SIZE 5
static float adjustment_history[ROLLING_AVERAGE_WINDOW_SIZE] = {0.0f};
static int adjustment_index = 0;

float calculateBufferAdjustment(float remaining_space, float targetbuffer_over, float targetbuffer_under, int batchsize) {

    float midpoint = (targetbuffer_over + targetbuffer_under) / 2.0f;

    float normalizedDistance;
    if (remaining_space < midpoint) {
        normalizedDistance = (midpoint - remaining_space) / (midpoint - targetbuffer_over);
    } else {
        normalizedDistance = (remaining_space - midpoint) / (targetbuffer_under - midpoint);
    }
	// I make crazy small adjustments, mooore tiny is mooore stable :D But don't come neir the limits cuz imma hit ya with that 0.005 ratio adjustment, pow pow!
    // I wonder if staying in the middle of 0 to 4000 with 512 samples per batch playing at tiny different speeds each iteration is like the smallest I can get
	// lets say hovering around 2000 means 2000 samples queue, about 4 frames, so at 17ms(60fps) thats  68ms delay right?
	// Should have payed attention when my math teacher was talking dammit
	// Also I chose 3 for pow, but idk if that really the best nr, anyone good in maths looking at my code?
	float adjustment = 0.0000001f + (0.005f - 0.0000001f) * pow(normalizedDistance, 3);

    if (remaining_space < midpoint) {
        adjustment = -adjustment;
    }

    adjustment_history[adjustment_index] = adjustment;
    adjustment_index = (adjustment_index + 1) % ROLLING_AVERAGE_WINDOW_SIZE;

    // Calculate the rolling average
    float rolling_average = 0.0f;
    for (int i = 0; i < ROLLING_AVERAGE_WINDOW_SIZE; ++i) {
        rolling_average += adjustment_history[i];
    }
    rolling_average /= ROLLING_AVERAGE_WINDOW_SIZE;

    return rolling_average;
}




static SND_Frame tmpbuffer[BATCH_SIZE];
static SND_Frame *unwritten_frames = NULL;
static int unwritten_frame_count = 0;

float currentratio = 0.0;
int currentbufferfree = 0;
int currentframecount = 0;
static double ratio = 1.0;
size_t SND_batchSamples(const SND_Frame *frames, size_t frame_count) {
	
	int framecount = (int)frame_count;

	int consumed = 0;
	int total_consumed_frames = 0;

	float remaining_space=snd.frame_count;
	if (snd.frame_in >= snd.frame_out) {
		remaining_space = snd.frame_count - (snd.frame_in - snd.frame_out);
	}
	else {
		remaining_space = snd.frame_out - snd.frame_in;
	}
	currentbufferfree = remaining_space;

	float tempdelay = ((snd.frame_count - remaining_space) / snd.sample_rate_out) * 1000;

	currentbufferms = tempdelay;

	float tempratio = (float)snd.sample_rate_out / (float)snd.sample_rate_in;
	// i use 0.4* as minimum free space because i want my algorithm to fight more for free buffer then full, cause you know free buffer is lower latency :D
	// My algorithm is fighting here with audio hardware. 
	// It's like a person is trying to balance on a rope (my algorithm) and another person (the audio hardware and screen) is wiggling the rope and the balancing person got to keep countering and try to stay stable
	float bufferadjustment = calculateBufferAdjustment(remaining_space, snd.frame_count*0.4, snd.frame_count,frame_count);
	ratio = (tempratio * (snd.frame_rate / current_fps)) + bufferadjustment;

	currentratio = ratio;

	if(ratio > 1.5) 
		ratio = 1.5;
	if(ratio < 0.5)
		ratio = 0.5;

	while (framecount > 0) {
		
		int amount = MIN(BATCH_SIZE, framecount);

		for (int i = 0; i < amount; i++) {
			tmpbuffer[i] = frames[consumed + i];
		}
		consumed += amount;
		framecount -= amount;

		ResampledFrames resampled = resample_audio(
			tmpbuffer, amount, snd.sample_rate_in, snd.sample_rate_out, ratio);

		// Write resampled frames to the buffer
		int written_frames = 0;
		
		for (int i = 0; i < resampled.frame_count; i++) {
			if ((snd.frame_in + 1) % snd.frame_count == snd.frame_out) {
				// Buffer is full, break. This should never happen tho, but just to be save
				break;
			}
			pthread_mutex_lock(&audio_mutex);
			snd.buffer[snd.frame_in] = resampled.frames[i];
			snd.frame_in = (snd.frame_in + 1) % snd.frame_count;
			pthread_mutex_unlock(&audio_mutex);
			written_frames++;
			
		}
		
		total_consumed_frames += written_frames;
		free(resampled.frames);
	}

	return total_consumed_frames;
}

void SND_init(double sample_rate, double frame_rate) { // plat_sound_init
	LOG_info("SND_init\n");
	currentreqfps = frame_rate;
	SDL_InitSubSystem(SDL_INIT_AUDIO);

		
#if defined(USE_SDL2)
	LOG_info("Available audio drivers:\n");
	for (int i=0; i<SDL_GetNumAudioDrivers(); i++) {
		LOG_info("- %s\n", SDL_GetAudioDriver(i));
	}
	LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver());
#endif	
	
	memset(&snd, 0, sizeof(struct SND_Context));
	snd.frame_rate = frame_rate;

	SDL_AudioSpec spec_in;
	SDL_AudioSpec spec_out;

	spec_in.freq = PLAT_pickSampleRate(sample_rate, MAX_SAMPLE_RATE);
	spec_in.format = AUDIO_S16;
	spec_in.channels = 2;
	spec_in.samples = SAMPLES;
	spec_in.callback = SND_audioCallback;
	
	if (SDL_OpenAudio(&spec_in, &spec_out)<0) LOG_info("SDL_OpenAudio error: %s\n", SDL_GetError());
	
	snd.frame_count = ((float)spec_out.freq/SCREEN_FPS)*6; // buffer size based on sample rate out (with 6 frames headroom), ideally you want to use actual FPS but don't know it at this point yet 
	currentbuffersize = snd.frame_count;
	snd.sample_rate_in  = sample_rate;
	snd.sample_rate_out = spec_out.freq;
	currentsampleratein = snd.sample_rate_in;
	currentsamplerateout = snd.sample_rate_out;
	
	SND_resizeBuffer();
	
	SDL_PauseAudio(0);

	LOG_info("sample rate: %i (req) %i (rec) [samples %i]\n", snd.sample_rate_in, snd.sample_rate_out, SAMPLES);
	snd.initialized = 1;
}
void SND_quit(void) { // plat_sound_finish
	if (!snd.initialized) return;
	
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	
	if (snd.buffer) {
		free(snd.buffer);
		snd.buffer = NULL;
	}
}

///////////////////////////////

LID_Context lid = {
	.has_lid = 0,
	.is_open = 1,
};

FALLBACK_IMPLEMENTATION void PLAT_initLid(void) {  }
FALLBACK_IMPLEMENTATION int PLAT_lidChanged(int* state) { return 0; }

///////////////////////////////

PAD_Context pad;

#define AXIS_DEADZONE 0x4000
void PAD_setAnalog(int neg_id,int pos_id,int value,int repeat_at) {
	// LOG_info("neg %i pos %i value %i\n", neg_id, pos_id, value);
	int neg = 1 << neg_id;
	int pos = 1 << pos_id;	
	if (value>AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed&pos)) { // not pressing
			pad.is_pressed 		|= pos; // set
			pad.just_pressed	|= pos; // set
			pad.just_repeated	|= pos; // set
			pad.repeat_at[pos_id]= repeat_at;
		
			if (pad.is_pressed&neg) { // was pressing opposite
				pad.is_pressed 		&= ~neg; // unset
				pad.just_repeated 	&= ~neg; // unset
				pad.just_released	|=  neg; // set
			}
		}
	}
	else if (value<-AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed&neg)) { // not pressing
			pad.is_pressed		|= neg; // set
			pad.just_pressed	|= neg; // set
			pad.just_repeated	|= neg; // set
			pad.repeat_at[neg_id]= repeat_at;
		
			if (pad.is_pressed&pos) { // was pressing opposite
				pad.is_pressed 		&= ~pos; // unset
				pad.just_repeated 	&= ~pos; // unset
				pad.just_released	|=  pos; // set
			}
		}
	}
	else { // not pressing
		if (pad.is_pressed&neg) { // was pressing
			pad.is_pressed 		&= ~neg; // unset
			pad.just_repeated	&=  neg; // unset
			pad.just_released	|=  neg; // set
		}
		if (pad.is_pressed&pos) { // was pressing
			pad.is_pressed 		&= ~pos; // unset
			pad.just_repeated	&=  pos; // unset
			pad.just_released	|=  pos; // set
		}
	}
}

void PAD_reset(void) {
	// LOG_info("PAD_reset");
	pad.just_pressed = BTN_NONE;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;
}
FALLBACK_IMPLEMENTATION void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}
	
	// the actual poll
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		int btn = BTN_NONE;
		int pressed = 0; // 0=up,1=down
		int id = -1;
		if (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
			uint8_t code = event.key.keysym.scancode;
			pressed = event.type==SDL_KEYDOWN;
			// LOG_info("key event: %i (%i)\n", code,pressed);
				 if (code==CODE_UP) 		{ btn = BTN_DPAD_UP; 		id = BTN_ID_DPAD_UP; }
 			else if (code==CODE_DOWN)		{ btn = BTN_DPAD_DOWN; 		id = BTN_ID_DPAD_DOWN; }
			else if (code==CODE_LEFT)		{ btn = BTN_DPAD_LEFT; 		id = BTN_ID_DPAD_LEFT; }
			else if (code==CODE_RIGHT)		{ btn = BTN_DPAD_RIGHT; 	id = BTN_ID_DPAD_RIGHT; }
			else if (code==CODE_A)			{ btn = BTN_A; 				id = BTN_ID_A; }
			else if (code==CODE_B)			{ btn = BTN_B; 				id = BTN_ID_B; }
			else if (code==CODE_X)			{ btn = BTN_X; 				id = BTN_ID_X; }
			else if (code==CODE_Y)			{ btn = BTN_Y; 				id = BTN_ID_Y; }
			else if (code==CODE_START)		{ btn = BTN_START; 			id = BTN_ID_START; }
			else if (code==CODE_SELECT)		{ btn = BTN_SELECT; 		id = BTN_ID_SELECT; }
			else if (code==CODE_MENU)		{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (code==CODE_MENU_ALT)	{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (code==CODE_L1)			{ btn = BTN_L1; 			id = BTN_ID_L1; }
			else if (code==CODE_L2)			{ btn = BTN_L2; 			id = BTN_ID_L2; }
			else if (code==CODE_L3)			{ btn = BTN_L3; 			id = BTN_ID_L3; }
			else if (code==CODE_R1)			{ btn = BTN_R1; 			id = BTN_ID_R1; }
			else if (code==CODE_R2)			{ btn = BTN_R2; 			id = BTN_ID_R2; }
			else if (code==CODE_R3)			{ btn = BTN_R3; 			id = BTN_ID_R3; }
			else if (code==CODE_PLUS)		{ btn = BTN_PLUS; 			id = BTN_ID_PLUS; }
			else if (code==CODE_MINUS)		{ btn = BTN_MINUS; 			id = BTN_ID_MINUS; }
			else if (code==CODE_POWER)		{ btn = BTN_POWER; 			id = BTN_ID_POWER; }
			else if (code==CODE_POWEROFF)	{ btn = BTN_POWEROFF;		id = BTN_ID_POWEROFF; } // nano-only
		}
		else if (event.type==SDL_JOYBUTTONDOWN || event.type==SDL_JOYBUTTONUP) {
			uint8_t joy = event.jbutton.button;
			pressed = event.type==SDL_JOYBUTTONDOWN;
			// LOG_info("joy event: %i (%i)\n", joy,pressed);
				 if (joy==JOY_UP) 		{ btn = BTN_DPAD_UP; 		id = BTN_ID_DPAD_UP; }
 			else if (joy==JOY_DOWN)		{ btn = BTN_DPAD_DOWN; 		id = BTN_ID_DPAD_DOWN; }
			else if (joy==JOY_LEFT)		{ btn = BTN_DPAD_LEFT; 		id = BTN_ID_DPAD_LEFT; }
			else if (joy==JOY_RIGHT)	{ btn = BTN_DPAD_RIGHT; 	id = BTN_ID_DPAD_RIGHT; }
			else if (joy==JOY_A)		{ btn = BTN_A; 				id = BTN_ID_A; }
			else if (joy==JOY_B)		{ btn = BTN_B; 				id = BTN_ID_B; }
			else if (joy==JOY_X)		{ btn = BTN_X; 				id = BTN_ID_X; }
			else if (joy==JOY_Y)		{ btn = BTN_Y; 				id = BTN_ID_Y; }
			else if (joy==JOY_START)	{ btn = BTN_START; 			id = BTN_ID_START; }
			else if (joy==JOY_SELECT)	{ btn = BTN_SELECT; 		id = BTN_ID_SELECT; }
			else if (joy==JOY_MENU)		{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (joy==JOY_MENU_ALT) { btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (joy==JOY_MENU_ALT2){ btn = BTN_MENU; 			id = BTN_ID_MENU; }
			else if (joy==JOY_L1)		{ btn = BTN_L1; 			id = BTN_ID_L1; }
			else if (joy==JOY_L2)		{ btn = BTN_L2; 			id = BTN_ID_L2; }
			else if (joy==JOY_L3)		{ btn = BTN_L3; 			id = BTN_ID_L3; }
			else if (joy==JOY_R1)		{ btn = BTN_R1; 			id = BTN_ID_R1; }
			else if (joy==JOY_R2)		{ btn = BTN_R2; 			id = BTN_ID_R2; }
			else if (joy==JOY_R3)		{ btn = BTN_R3; 			id = BTN_ID_R3; }
			else if (joy==JOY_PLUS)		{ btn = BTN_PLUS; 			id = BTN_ID_PLUS; }
			else if (joy==JOY_MINUS)	{ btn = BTN_MINUS; 			id = BTN_ID_MINUS; }
			else if (joy==JOY_POWER)	{ btn = BTN_POWER; 			id = BTN_ID_POWER; }
		}
		else if (event.type==SDL_JOYHATMOTION) {
			int hats[4] = {-1,-1,-1,-1}; // -1=no change,0=up,1=down,2=left,3=right btn_ids
			int hat = event.jhat.value;
			// LOG_info("hat event: %i\n", hat);
			// TODO: safe to assume hats will always be the primary dpad?
			// TODO: this is literally a bitmask, make it one (oh, except there's 3 states...)
			switch (hat) {
				case SDL_HAT_UP:			hats[0]=1;	  hats[1]=0;	hats[2]=0;	  hats[3]=0;	break;
				case SDL_HAT_DOWN:			hats[0]=0;	  hats[1]=1;	hats[2]=0;	  hats[3]=0;	break;
				case SDL_HAT_LEFT:			hats[0]=0;	  hats[1]=0;	hats[2]=1;	  hats[3]=0;	break;
				case SDL_HAT_RIGHT:			hats[0]=0;	  hats[1]=0;	hats[2]=0;	  hats[3]=1;	break;
				case SDL_HAT_LEFTUP:		hats[0]=1;	  hats[1]=0;	hats[2]=1;	  hats[3]=0;	break;
				case SDL_HAT_LEFTDOWN:		hats[0]=0;	  hats[1]=1;	hats[2]=1;	  hats[3]=0;	break;
				case SDL_HAT_RIGHTUP:		hats[0]=1;	  hats[1]=0;	hats[2]=0;	  hats[3]=1;	break;
				case SDL_HAT_RIGHTDOWN:		hats[0]=0;	  hats[1]=1;	hats[2]=0;	  hats[3]=1;	break;
				case SDL_HAT_CENTERED:		hats[0]=0;	  hats[1]=0;	hats[2]=0;	  hats[3]=0;	break;
				default: break;
			}
			
			for (id=0; id<4; id++) {
				int state = hats[id];
				btn = 1 << id;
				if (state==0) {
					pad.is_pressed		&= ~btn; // unset
					pad.just_repeated	&= ~btn; // unset
					pad.just_released	|= btn; // set
				}
				else if (state==1 && (pad.is_pressed & btn)==BTN_NONE) {
					pad.just_pressed	|= btn; // set
					pad.just_repeated	|= btn; // set
					pad.is_pressed		|= btn; // set
					pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
				}
			}
			btn = BTN_NONE; // already handled, force continue
		}
		else if (event.type==SDL_JOYAXISMOTION) {
			int axis = event.jaxis.axis;
			int val = event.jaxis.value;
			// LOG_info("axis: %i (%i)\n", axis,val);
			
			// triggers on tg5040
			if (axis==AXIS_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
				pressed = val>0;
			}
			else if (axis==AXIS_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
				pressed = val>0;
			}
			
			else if (axis==AXIS_LX) { pad.laxis.x = val; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, val, tick+PAD_REPEAT_DELAY); }
			else if (axis==AXIS_LY) { pad.laxis.y = val; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  val, tick+PAD_REPEAT_DELAY); }
			else if (axis==AXIS_RX) pad.raxis.x = val;
			else if (axis==AXIS_RY) pad.raxis.y = val;
			
			// axis will fire off what looks like a release
			// before the first press but you can't release
			// a button that wasn't pressed
			if (!pressed && btn!=BTN_NONE && !(pad.is_pressed & btn)) {
				// LOG_info("cancel: %i\n", axis);
				btn = BTN_NONE;
			}
		}
		else if (event.type==SDL_QUIT) PWR_powerOff();
		
		if (btn==BTN_NONE) continue;
		
		if (!pressed) {
			pad.is_pressed		&= ~btn; // unset
			pad.just_repeated	&= ~btn; // unset
			pad.just_released	|= btn; // set
		}
		else if ((pad.is_pressed & btn)==BTN_NONE) {
			pad.just_pressed	|= btn; // set
			pad.just_repeated	|= btn; // set
			pad.is_pressed		|= btn; // set
			pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
		}
	}
	
	if (lid.has_lid && PLAT_lidChanged(NULL)) pad.just_released |= BTN_SLEEP;
}
FALLBACK_IMPLEMENTATION int PLAT_shouldWake(void) {
	int lid_open = 1; // assume open by default
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open) return 1;
	
	
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type==SDL_KEYUP) {
			uint8_t code = event.key.keysym.scancode;
			if ((BTN_WAKE==BTN_POWER && code==CODE_POWER) || (BTN_WAKE==BTN_MENU && (code==CODE_MENU || code==CODE_MENU_ALT))) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open) return 0;  // do it here so we eat the input
				return 1;
			}
		}
		else if (event.type==SDL_JOYBUTTONUP) {
			uint8_t joy = event.jbutton.button;
			if ((BTN_WAKE==BTN_POWER && joy==JOY_POWER) || (BTN_WAKE==BTN_MENU && (joy==JOY_MENU || joy==JOY_MENU_ALT))) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open) return 0;  // do it here so we eat the input
				return 1;
			}
		} 
	}
	return 0;
}
FALLBACK_IMPLEMENTATION int PLAT_supportsDeepSleep(void) { return 0; }
FALLBACK_IMPLEMENTATION int PLAT_deepSleep(void) {
	const char *state_path = "/sys/power/state";

	int state_fd = open(state_path, O_WRONLY);
	if (state_fd < 0) {
		LOG_error("failed to open %s: %d\n", state_path, errno);
		return -1;
	}

	LOG_info("suspending to RAM\n");
	int ret = write(state_fd, "mem", 3);
	if (ret < 0) {
		// Can fail shortly after resuming with EBUSY
		LOG_error("failed to set power state: %d\n", errno);
		close(state_fd);
		return -1;
	}

	LOG_info("returned from suspend\n");
	close(state_fd);
	return 0;
}

int PAD_anyJustPressed(void)	{ return pad.just_pressed!=BTN_NONE; }
int PAD_anyPressed(void)		{ return pad.is_pressed!=BTN_NONE; }
int PAD_anyJustReleased(void)	{ return pad.just_released!=BTN_NONE; }

int PAD_justPressed(int btn)	{ return pad.just_pressed & btn; }
int PAD_isPressed(int btn)		{ return pad.is_pressed & btn; }
int PAD_justReleased(int btn)	{ return pad.just_released & btn; }
int PAD_justRepeated(int btn)	{ return pad.just_repeated & btn; }

int PAD_tappedMenu(uint32_t now) {
	#define MENU_DELAY 250 // also in PWR_update()
	static uint32_t menu_start = 0;
	static int ignore_menu = 0; 
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
		menu_start = now;
	}
	else if (PAD_isPressed(BTN_MENU) && BTN_MOD_BRIGHTNESS==BTN_MENU && (PAD_justPressed(BTN_MOD_PLUS) || PAD_justPressed(BTN_MOD_MINUS))) {
		ignore_menu = 1;
	}
	return (!ignore_menu && PAD_justReleased(BTN_MENU) && now-menu_start<MENU_DELAY);
}

///////////////////////////////

static struct VIB_Context {
	int initialized;
	pthread_t pt;
	int queued_strength;
	int strength;
} vib = {0};
static void* VIB_thread(void *arg) {
#define DEFER_FRAMES 3
	static int defer = 0;
	while(1) {
		SDL_Delay(17);
		if (vib.queued_strength!=vib.strength) {
			if (defer<DEFER_FRAMES && vib.queued_strength==0) { // minimize vacillation between 0 and some number (which this motor doesn't like)
				defer += 1;
				continue;
			}
			vib.strength = vib.queued_strength;
			defer = 0;

			PLAT_setRumble(vib.strength);
		}
	}
	return 0;
}
void VIB_init(void) {
	vib.queued_strength = vib.strength = 0;
	pthread_create(&vib.pt, NULL, &VIB_thread, NULL);
	vib.initialized = 1;
}
void VIB_quit(void) {
	if (!vib.initialized) return;
	
	VIB_setStrength(0);
	pthread_cancel(vib.pt);
	pthread_join(vib.pt, NULL);
}
void VIB_setStrength(int strength) {
	if (vib.queued_strength==strength) return;
	vib.queued_strength = strength;
}
int VIB_getStrength(void) {
	return vib.strength;
}

///////////////////////////////

static void PWR_initOverlay(void) {
	// setup surface
	pwr.overlay = PLAT_initOverlay();

	// draw battery
	SDLX_SetAlpha(gfx.assets, 0,0);
	GFX_blitAssetColor(ASSET_WHITE_PILL, NULL, pwr.overlay, NULL, THEME_COLOR1);
	SDLX_SetAlpha(gfx.assets, SDL_SRCALPHA,0);
	GFX_blitBattery(pwr.overlay, NULL);
}

static void PWR_updateBatteryStatus(void) {
	PLAT_getBatteryStatusFine(&pwr.is_charging, &pwr.charge);
	PLAT_enableOverlay(pwr.should_warn && pwr.charge<=PWR_LOW_CHARGE);
	
	// low power warn on all leds 
	if(pwr.charge < PWR_LOW_CHARGE+10) {
		LEDS_setIndicator(3,0xFF3300,-1);
	}
	if(pwr.charge < PWR_LOW_CHARGE) {
		LEDS_setIndicator(3,0xFF0000,-1);
	}
}

static void* PWR_monitorBattery(void *arg) {
	while(1) {
		// TODO: the frequency of checking could depend on whether 
		// we're in game (less frequent) or menu (more frequent)
		sleep(5);
		PWR_updateBatteryStatus();
	}
	return NULL;
}

void PWR_init(void) {
	pwr.can_sleep = 1;
	pwr.can_poweroff = 1;
	pwr.can_autosleep = 1;
	
	pwr.requested_sleep = 0;
	pwr.requested_wake = 0;
	pwr.resume_tick = 0;
	
	pwr.should_warn = 0;
	pwr.charge = PWR_LOW_CHARGE;
	
	PWR_initOverlay();

	PWR_updateBatteryStatus();
	pthread_create(&pwr.battery_pt, NULL, &PWR_monitorBattery, NULL);
	pwr.initialized = 1;
}
void PWR_quit(void) {
	if (!pwr.initialized) return;
	
	PLAT_quitOverlay();
	
	// cancel battery thread
	pthread_cancel(pwr.battery_pt);
	pthread_join(pwr.battery_pt, NULL);
}
void PWR_warn(int enable) {
	pwr.should_warn = enable;
	PLAT_enableOverlay(pwr.should_warn && pwr.charge<=PWR_LOW_CHARGE);
}

int PWR_ignoreSettingInput(int btn, int show_setting) {
	return show_setting && (btn==BTN_MOD_PLUS || btn==BTN_MOD_MINUS);
}

void PWR_update(int* _dirty, int* _show_setting, PWR_callback_t before_sleep, PWR_callback_t after_sleep) {
	int dirty = _dirty ? *_dirty : 0;
	int show_setting = _show_setting ? *_show_setting : 0;
	
	static uint32_t last_input_at = 0; // timestamp of last input (autosleep)
	static uint32_t checked_charge_at = 0; // timestamp of last time checking charge
	static uint32_t setting_shown_at = 0; // timestamp when settings started being shown
	static uint32_t power_pressed_at = 0; // timestamp when power button was just pressed
	static uint32_t mod_unpressed_at = 0; // timestamp of last time settings modifier key was NOT down
	static uint32_t was_muted = -1;
	if (was_muted==-1) was_muted = GetMute();
	
	static int was_charging = -1;
	if (was_charging==-1) {
		was_charging = pwr.is_charging;
		if(pwr.is_charging) {
			LED_setIndicator(2,0xFF0000,-1,2);
		}
	}

	uint32_t now = SDL_GetTicks();
	if (was_charging || PAD_anyPressed() || last_input_at==0) last_input_at = now;
	
	#define CHARGE_DELAY 1000
	if (dirty || now-checked_charge_at>=CHARGE_DELAY) {
		int is_charging = pwr.is_charging;
		if (was_charging!=is_charging) {
			if(is_charging) {
				LED_setIndicator(2,0xFF0000,-1,2);
			} else {
				PLAT_initLeds(lights);
				LEDS_updateLeds();
			}
			was_charging = is_charging;
			dirty = 1;
		} 
		checked_charge_at = now;
	}
	
	if (PAD_justReleased(BTN_POWEROFF) || (power_pressed_at && now-power_pressed_at>=1000)) {
		if (before_sleep) before_sleep();
		system("gametimectl.elf stop_all");
		PWR_powerOff();
	}
	
	if (PAD_justPressed(BTN_POWER)) {
		if (now - pwr.resume_tick < 1000) {
			LOG_debug("ignoring spurious power button press (just resumed)\n");
			power_pressed_at = 0;
		} else {
			power_pressed_at = now;
		}
	}
	
	#define SLEEP_DELAY 30000 // 30 seconds
	if (now-last_input_at>=SLEEP_DELAY && PWR_preventAutosleep()) last_input_at = now;
	
	if (
		pwr.requested_sleep || // hardware requested sleep
		now-last_input_at>=SLEEP_DELAY || // autosleep
		(pwr.can_sleep && PAD_justReleased(BTN_SLEEP) && power_pressed_at) // manual sleep
	) {
		pwr.requested_sleep = 0;
		if (before_sleep) before_sleep();
		PWR_sleep();
		if (after_sleep) after_sleep();
		last_input_at = now = SDL_GetTicks();
		power_pressed_at = 0;
		dirty = 1;
	}
	
	int was_dirty = dirty; // dirty list (not including settings/battery)
	
	// TODO: only delay hiding setting changes if that setting didn't require a modifier button be held, otherwise release as soon as modifier is released
	
	int delay_settings = BTN_MOD_BRIGHTNESS==BTN_MENU; // when both volume and brighness require a modifier hide settings as soon as it is released
	#define SETTING_DELAY 500
	if (show_setting && (now-setting_shown_at>=SETTING_DELAY || !delay_settings) && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS) && !PAD_isPressed(BTN_MOD_COLORTEMP)) {
		show_setting = 0;
		dirty = 1;
	}
	
	if (!show_setting && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS) && !PAD_isPressed(BTN_MOD_COLORTEMP)) {
		mod_unpressed_at = now; // this feels backwards but is correct
	}
	
	#define MOD_DELAY 250
	if (
		(
			(PAD_isPressed(BTN_MOD_VOLUME) || PAD_isPressed(BTN_MOD_BRIGHTNESS) || PAD_isPressed(BTN_MOD_COLORTEMP)) && 
			(!delay_settings || now-mod_unpressed_at>=MOD_DELAY)
		) || 
		((!BTN_MOD_VOLUME || !BTN_MOD_BRIGHTNESS || !BTN_MOD_COLORTEMP) && (PAD_justRepeated(BTN_MOD_PLUS) || PAD_justRepeated(BTN_MOD_MINUS)))
	) {
		setting_shown_at = now;
		if (PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
			show_setting = 1;
		}
		else if (PAD_isPressed(BTN_MOD_COLORTEMP)) {
			show_setting = 3;
		}
		else {
			show_setting = 2;
		}
	}
	
	int muted = GetMute();
	if (muted!=was_muted) {
		was_muted = muted;
		show_setting = 2;
		setting_shown_at = now;
	}
	
	if (show_setting) dirty = 1; // shm is slow or keymon is catching input on the next frame
	if (_dirty) *_dirty = dirty;
	if (_show_setting) *_show_setting = show_setting;
}

// TODO: this isn't whether it can sleep but more if it should sleep in response to the sleep button
void PWR_disableSleep(void) {
	pwr.can_sleep = 0;
}
void PWR_enableSleep(void) {
	pwr.can_sleep = 1;
}

void PWR_disablePowerOff(void) {
	pwr.can_poweroff = 0;
}
void PWR_powerOff(void) {
	if (pwr.can_poweroff) {
		
		int w = FIXED_WIDTH;
		int h = FIXED_HEIGHT;
		int p = FIXED_PITCH;
		if (GetHDMI()) {
			w = HDMI_WIDTH;
			h = HDMI_HEIGHT;
			p = HDMI_PITCH;
		}
		gfx.screen = GFX_resize(w,h,p);
		
		char* msg;
		if (HAS_POWER_BUTTON || HAS_POWEROFF_BUTTON) msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npowering off" : "Powering off";
		else msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npower off now" : "Power off now";
		
		// LOG_info("PWR_powerOff %s (%ix%i)\n", gfx.screen, gfx.screen->w, gfx.screen->h);
		
		// TODO: for some reason screen's dimensions end up being 0x0 in GFX_blitMessage...
		PLAT_clearVideo(gfx.screen);
		GFX_blitMessage(font.large, msg, gfx.screen,&(SDL_Rect){0,0,gfx.screen->w,gfx.screen->h}); //, NULL);
		GFX_flip(gfx.screen);
		PLAT_powerOff();
	}
}

static void PWR_enterSleep(void) {
	SDL_PauseAudio(1);
	LEDS_setIndicator(2,0,5);
	if (GetHDMI()) {
		PLAT_clearVideo(gfx.screen);
		PLAT_flip(gfx.screen, 0);
	}
	else {
		SetRawVolume(MUTE_VOLUME_RAW);
		PLAT_enableBacklight(0);
	}
	system("killall -STOP keymon.elf");
	system("killall -STOP batmon.elf");
	
	sync();
}
static void PWR_exitSleep(void) {
	PLAT_initLeds(lights);
	LEDS_updateLeds();
	if(pwr.is_charging) {
		LED_setIndicator(2,0xFF0000,-1,2);
	}
	system("killall -CONT keymon.elf");
	system("killall -CONT batmon.elf");
	if (GetHDMI()) {
		// buh
	}
	else {
		PLAT_enableBacklight(1);
		SetVolume(GetVolume());
	}
	SDL_PauseAudio(0);
	
	sync();
}

static void PWR_waitForWake(void) {
	uint32_t sleep_ticks = SDL_GetTicks();
	int deep_sleep_attempts = 0;
	while (!PAD_wake()) {
		if (pwr.requested_wake) {
			pwr.requested_wake = 0;
			break;
		}
		SDL_Delay(200);
		if (SDL_GetTicks()-sleep_ticks>=30000) { // increased to two minutes
			if (pwr.is_charging) {
				sleep_ticks += 60000; // check again in a minute
				continue;
			}
			if (PLAT_supportsDeepSleep()) {
				int ret = PWR_deepSleep();
				if (ret == 0) {
					return;
				} else if (deep_sleep_attempts < 3) {
					LOG_warn("failed to enter deep sleep - retrying in 5 seconds\n");
					sleep_ticks += 5000;
					deep_sleep_attempts++;
					continue;
				} else {
					LOG_warn("failed to enter deep sleep - powering off\n");
				}
			}
			if (pwr.can_poweroff) {
				PWR_powerOff();
			}
		}
	}
	
	return;
}
void PWR_sleep(void) {
	LOG_info("Entering hybrid sleep\n");
	
	system("gametimectl.elf stop_all");

	GFX_clear(gfx.screen);
	PAD_reset();
	PWR_enterSleep();
	PWR_waitForWake();
	PWR_exitSleep();
	PAD_reset();

	system("gametimectl.elf resume");

	pwr.resume_tick = SDL_GetTicks();
}

int PWR_deepSleep(void) {
	// Run `${BIN_PATH}/suspend` if it exists, then fall back
	// to the PLAT_deepSleep implementation.
	//
	// We assume the suspend executable exits after a full
	// suspend/resume cycle.
	char *suspend_path = BIN_PATH "/suspend";
	if (exists(suspend_path)) {
		LOG_info("suspending using platform suspend executable\n");

		int ret = system(suspend_path);
		if (ret < 0) {
			LOG_error("failed to launch suspend executable: %d\n", errno);
			return -1;
		}

		LOG_info("suspend executable exited with %d\n", ret);
		return ret == 0 ? 0 : -1;
	}

	return PLAT_deepSleep();
}

void PWR_disableAutosleep(void) {
	pwr.can_autosleep = 0;
}
void PWR_enableAutosleep(void) {
	pwr.can_autosleep = 1;
}
int PWR_preventAutosleep(void) {
	return pwr.is_charging || !pwr.can_autosleep || GetHDMI();
}

// updated by PWR_updateBatteryStatus()
int PWR_isCharging(void) {
	return pwr.is_charging;
}
int PWR_getBattery(void) { // 10-100 in 10-20% fragments
	return pwr.charge;
}

///////////////////////////////

// TODO: tmp? move to individual platforms or allow overriding like PAD_poll/PAD_wake?
int PLAT_setDateTime(int y, int m, int d, int h, int i, int s) {
	char cmd[512];
	sprintf(cmd, "date -s '%d-%d-%d %d:%d:%d'; hwclock --utc -w", y,m,d,h,i,s);
	system(cmd);
	return 0; // why does this return an int?
}

// only indicator leds may work when battery is below PWR_LOW_CHARGE
void LED_setIndicator(int effect,uint32_t color, int cycles,int ledindex) {
	int lightsize = sizeof(lights) / sizeof(lights[0]);
		lights[ledindex].effect = effect;
		lights[ledindex].color1 = color;
		lights[ledindex].cycles = cycles;
		
		PLAT_setLedInbrightness(&lights[ledindex]);
		PLAT_setLedEffectCycles(&lights[ledindex]);
		PLAT_setLedColor(&lights[ledindex]);
		PLAT_setLedEffect(&lights[ledindex]);
}
void LEDS_setIndicator(int effect,uint32_t color, int cycles) {
	int lightsize = sizeof(lights) / sizeof(lights[0]);
	for (int i = 0; i < lightsize; i++)
	{
		lights[i].effect = effect;
		if(color) {
			lights[i].color1 = color;
		}
		lights[i].cycles = cycles;

		PLAT_setLedInbrightness(&lights[i]);
		PLAT_setLedEffectCycles(&lights[i]);
		PLAT_setLedColor(&lights[i]);
		PLAT_setLedEffect(&lights[i]);
		
	}
}
void LEDS_setEffect(int effect) {
	if(pwr.charge > PWR_LOW_CHARGE) {
		int lightsize = sizeof(lights) / sizeof(lights[0]);
		for (int i = 0; i < lightsize; i++)
		{
			lights[i].effect = effect;
			PLAT_setLedEffect(&lights[i]);
		}
	}
}
void LEDS_setColor(uint32_t color) {
	if(pwr.charge > PWR_LOW_CHARGE) {
		int lightsize = sizeof(lights) / sizeof(lights[0]);
		for (int i = 0; i < lightsize; i++)
		{
			lights[i].color1 = color;
			PLAT_setLedColor(&lights[i]);
			PLAT_setLedEffect(&lights[i]);
		}
	}
}

void LED_setColor(uint32_t color,int ledindex) {
	if(pwr.charge > PWR_LOW_CHARGE) {
		lights[ledindex].color1 = color;
		PLAT_setLedColor(&lights[ledindex]);
		PLAT_setLedEffect(&lights[ledindex]);
	}
}

void LEDS_updateLeds() {
	if(pwr.charge > PWR_LOW_CHARGE) {
		int lightsize = sizeof(lights) / sizeof(lights[0]);
		for (int i = 0; i < lightsize; i++)
		{
			PLAT_setLedBrightness(&lights[i]); // set brightness of each led
			PLAT_setLedEffectCycles(&lights[i]); // set how many times animation should loop
			PLAT_setLedEffectSpeed(&lights[i]); // set animation speed
			PLAT_setLedColor(&lights[i]); // set color
			PLAT_setLedEffect(&lights[i]); // finally set the effect, on trimui devices this also applies the settings
		
			
		}
	}
}

void LEDS_initLeds() {
	PLAT_getBatteryStatusFine(&pwr.is_charging, &pwr.charge);
	PLAT_initLeds(lights);
}


