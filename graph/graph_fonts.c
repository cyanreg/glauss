/*
 * glauss is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * glauss is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with glauss.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "graph.h"
#include "graph_fonts.h"
#include "shared/msg_phys.h"
#include "shared/options.h"

static const char text_vs[] =
// Generated from text_vs.glsl
#include "graph/shaders/text_vs.h"
;

static const char text_fs[] =
// Generated from text_fs.glsl
#include "graph/shaders/text_fs.h"
;

static FT_Library library;
static FT_Face face;
static FT_GlyphSlot g;

static GLuint font_tex;
static GLuint textattr_coord, textattr_texcoord, textattr_tex, textattr_color;
static GLint atlas_w;
static GLint atlas_h;

/* Generic white if NULL pointer to color */
static const GLfloat white[] = {1.0, 1.0, 1.0, 1.0};

/* Freetype - offsets for every char */
static struct character_info {
    float ax;
    float ay;
    float bw;
    float bh;
    float bl;
    float bt;
    float tx;
} ft_chr[128];

/* Freetype - coords */
typedef struct ft_p {
    GLfloat x;
    GLfloat y;
    GLfloat s;
    GLfloat t;
} point;

/* Ask the fontgod for a generic, standard issue "Arial" font */
const char *graph_init_fontconfig(void)
{
    /* Offer fontgod sacrificial pointers to hold his highness */
    FcConfig *fc_config = FcInitLoadConfigAndFonts();
    FcPattern *fc_pattern = FcPatternCreate();
    /* Ask the deity for a user-specified gift of typography */
    FcPatternAddString(fc_pattern, FC_FAMILY, (const FcChar8 *)option->fontname);
    /* Ask fontgod not to blind our eyes for our insolence */
    FcPatternAddBool(fc_pattern, FC_ANTIALIAS, 1);
    /* Summon a fontdemon which shall transmit the gifts of our god */
    FcResult fc_result;
    /* Incantation for our omnipotence to recognize our request: */
    FcDefaultSubstitute(fc_pattern);
    FcConfigSubstitute(fc_config, fc_pattern, FcMatchPattern);
    /* "We ask you, oh you in the sky, for your attention..." */
    FcPattern *fc_font_chosen = FcFontMatch(fc_config, fc_pattern, &fc_result);
    FcValue fc_value;
    /* SHOW US YOUR POWER, INVOKE ANCIENT KNOWLEDGE, GIVE US THE LOCATION! */
    FcPatternGet(fc_font_chosen, "file", 0, &fc_value);
    /* Fontgod has given us a sacred filename, hail FONTCONFIG! */
    pprintf(PRI_SPAM, "[FC] Font path received = %s\n", (char *)fc_value.u.s);
    return (const char *)fc_value.u.s;
}

void graph_stop_fontconfig(void)
{
    /* Buggy */
    //FcFini();
}

GLuint graph_init_freetype(const char *fontname)
{
    /* Initialize OpenGL */
    GLuint text_program = graph_compile_shader(text_vs, text_fs);
    glBindAttribLocation(text_program, textattr_coord, "coord");
    glBindAttribLocation(text_program, textattr_texcoord, "textcolor");
    glBindAttribLocation(text_program, textattr_tex, "tex");
    glBindAttribLocation(text_program, textattr_color, "color");
    textattr_tex = glGetUniformLocation(text_program, "tex");
    textattr_color = glGetUniformLocation(text_program, "textcolor");
    glUniform1i(textattr_tex, 0);

    /* Init Freetype and generate texture atlas */
    if (FT_Init_FreeType(&library)) {
        pprintf(PRI_ERR, "Could not init freetype library.\n");
        exit(1);
    }
    if (FT_New_Face(library, fontname, 0, &face)) {
        pprintf(PRI_ERR, "Freetype could not open font.\n");
        exit(1);
    }
    FT_Set_Pixel_Sizes(face, 0, option->fontsize);
    g = face->glyph;

    /* Load glyphs and get maximum height and width */
    for (int i = 32; i < 128; i++) {
        if(FT_Load_Char(face, i, FT_LOAD_RENDER)) {
            fprintf(stderr, "Loading character %c failed!\n", i);
            continue;
        }
        atlas_w += g->bitmap.width + 1;
        if (g->bitmap.rows > atlas_h)
            atlas_h = g->bitmap.rows;
    }

    /* Create a seperate texture to hold altas, fill it with NULL */
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &font_tex);
    glBindTexture(GL_TEXTURE_2D, font_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas_w, atlas_h, 0,
                GL_ALPHA, GL_UNSIGNED_BYTE, NULL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Fill texture and save locations(offsets) for every char */
    GLint x = 0;
    for (short i = 32; i < 128; i++) {
        if (FT_Load_Char(face, i, FT_LOAD_RENDER))
            continue;
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0, g->bitmap.width, g->bitmap.rows,
                        GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);
        ft_chr[i].ax = g->advance.x >> 6;
        ft_chr[i].ay = g->advance.y >> 6;
        ft_chr[i].bw = g->bitmap.width;
        ft_chr[i].bh = g->bitmap.rows;
        ft_chr[i].bl = g->bitmap_left;
        ft_chr[i].bt = g->bitmap_top;
        ft_chr[i].tx = (GLfloat)x/atlas_w;
        x += g->bitmap.width;
    }

    pprintf(PRI_SPAM, "[FT] Created a %i by %i atlas (%3.3lf KiB)\n",
        atlas_w, atlas_h, atlas_w*atlas_h/1024.0);

    return text_program;
}

void graph_stop_freetype(void)
{
    FT_Done_Face(face);
    FT_Done_FreeType(library);
}

void graph_display_text(const char *text, GLfloat x, GLfloat y, GLfloat s,
                        const GLfloat *col)
{
    if (!col)
        col = white;
    GLfloat sx = s/option->width, sy = s/option->height;
    point coords[6*strlen(text)];

    glEnable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, font_tex);
    glEnable(GL_TEXTURE_2D);

    glVertexAttribPointer(textattr_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glUniform4fv(textattr_color, 1, col);
    glVertexAttribPointer(textattr_color, 4, GL_FLOAT, GL_FALSE, 0, col);
    glEnableVertexAttribArray(textattr_coord);
    glEnableVertexAttribArray(textattr_color);

    /* Count glyphs */
    unsigned int n = 0;
    /* Convert to short to prevent compiler from complaining "INDEX IS CHAR!" */
    short d = 0;

    for (const char *p = text; *p; p++) {
        d = (short)*p;
        GLfloat x2 =  x + ft_chr[d].bl * sx;
        GLfloat y2 = -y - ft_chr[d].bt * sy;
        GLfloat w = ft_chr[d].bw * sx;
        GLfloat h = ft_chr[d].bh * sy;

        /* Advance */
        x += ft_chr[d].ax * sx;
        y += ft_chr[d].ay * sy;

        /* Skip glyphs that have no pixels */
        if (!w || !h)
            continue;

        /* Setup coords */
        coords[n++] = (point){x2, -y2,
                            ft_chr[d].tx,
                            0};
        coords[n++] = (point){x2 + w, -y2,
                            ft_chr[d].tx + ft_chr[d].bw/atlas_w,
                            0};
        coords[n++] = (point){x2, -y2 - h,
                            ft_chr[d].tx,
                            ft_chr[d].bh/atlas_h};
        coords[n++] = (point){x2 + w, -y2,
                            ft_chr[d].tx + ft_chr[d].bw/atlas_w,
                            0};
        coords[n++] = (point){x2, -y2 - h,
                            ft_chr[d].tx,
                            ft_chr[d].bh/atlas_h};
        coords[n++] = (point){x2 + w, -y2 - h,
                            ft_chr[d].tx + ft_chr[d].bw/atlas_w,
                            ft_chr[d].bh/atlas_h};
    }

    glBufferData(GL_ARRAY_BUFFER, sizeof(coords), coords, GL_STATIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, n);

    glDisableVertexAttribArray(textattr_coord);
    glDisableVertexAttribArray(textattr_color);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void graph_display_object_info(phys_obj *object, unsigned int num)
{
    char osdstr[30];
    snprintf(osdstr, sizeof(osdstr), "Object=%i", num);
    graph_display_text(osdstr, 0.76, -0.32, 1.0, NULL);
    snprintf(osdstr, sizeof(osdstr), "Atom=%s",
            return_atom_str(object[num].atomnumber));
    graph_display_text(osdstr, 0.76, -0.40, 1.0, NULL);
}
