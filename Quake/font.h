#ifndef FONT_H
#define FONT_H
#include "quakedef.h"

#define CHAR_WIDTH 16
#define CHAR_HEIGHT 16
#define DRAW_WIDTH 8
#define DRAW_HEIGHT 8
#define COLUMNS 128
#define FONT_SIZE 14

typedef struct gltexture_s gltexture_t;

typedef struct {
    unsigned int start;    // Starting code point
    unsigned int end;      // Ending code point
    gltexture_t* texture;  // OpenGL texture
    int columns;     // Number of columns in the texture grid
    int tex_width;   // Texture width in pixels
    int tex_height;  // Texture height in pixels
} UnicodeBlock;

extern UnicodeBlock unicode_blocks[];

int font_index;

void select_font(void);
int generate_font_pngs();
int setup_fonts();
void draw_character_quad_ex(int x, int y, Uint32 codepoint, int r, int g, int b);
char* to_utf8(const char* str);
Uint32 utf8_decode_nth(const char* input,int index,size_t length);
void get_texture_data(GLuint textureID, SDL_Surface** surface);
int copy_glyph(UnicodeBlock* block, SDL_Surface* glyph, SDL_Surface* destination, int columns, Uint32 codepoint);
int copy_single_glyph(UnicodeBlock* block, SDL_Surface* source, SDL_Surface* destination, int columns, Uint32 codepoint);
#endif