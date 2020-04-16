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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <unistd.h>
#include <png.h>
#include "graph.h"
#include "graph_sdl.h"
#include "graph_objects.h"
#include "graph_fonts.h"
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "input/parser.h"

/* UI POSITIONS */

/* FPS counter */
#define FPSx -0.95
#define FPSy  0.85
#define FPSs  1.0

/* Object count */
#define OBJx -0.95
#define OBJy  0.75
#define OBJs  1.0

/* Time counter */
#define TIMEx -0.95
#define TIMEy  0.65
#define TIMEs  1.0

/* dt display */
#define DTx -0.95
#define DTy  0.55
#define DTs  1.0

/* Algorithm display */
#define ALGx  -0.95
#define ALGy  0.45
#define ALGs  1.0

/* Simulation status */
#define SIMx 0.65
#define SIMy -0.95
#define SIMs  1.0

/* Algorithm stats */
#define STATSx -0.95
#define STATSy -0.30
#define STATSs  0.75

/* Object selection */
#define OSLx  0.70
#define OSLy -0.15
#define OSLs  1.00

/* Thread time counters */
#define THRx  0.73
#define THRy  0.95
#define THRs  0.75

/* Thread stats */
#define THREAD_MAPx 0.96
#define THREAD_MAPy -0.35
#define THREAD_MAPs 0.70

/* Lua printed */
#define LUAx -0.95
#define LUAy -0.90
#define LUAs 1.0

/* Axis scale */
#define AXISs 0.25

/* OSD Buffer size */
#define OSD_BUFFER 128

/* UI POSITIONS */

/* COLORS */
const GLfloat COL_WHITE[]   =  {  1.0f,   1.0f,   1.0f,   1.0f  };
const GLfloat COL_RED[]     =  {  1.0f,   0.0f,   0.0f,   1.0f  };
const GLfloat COL_GREEN[]   =  {  0.0f,   1.0f,   0.0f,   1.0f  };
const GLfloat COL_BLUE[]    =  {  0.0f,   0.0f,   1.0f,   1.0f  };
const GLfloat COL_YELLOW[]  =  {  1.0f,   1.0f,   0.0f,   1.0f  };
const GLfloat COL_ORANGE[]  =  { 0.82f,  0.41f,  0.11f,   1.0f  };
/* COLORS */

static GLuint pointvbo, textvbo;
static GLuint object_shader, text_shader;
static GLint trn_matrix, rot_matrix, scl_matrix, per_matrix;
static GLfloat *mat = NULL, *rotx = NULL, *roty = NULL, *rotz = NULL;
static GLfloat *rotation = NULL, *scale = NULL, *transl = NULL, *pers = NULL;

static void mul_matrix(GLfloat *prod, const GLfloat *a, const GLfloat *b)
{
    #define A(row,col)  a[(col<<2)+row]
    #define B(row,col)  b[(col<<2)+row]
    #define P(row,col)  p[(col<<2)+row]
    GLfloat p[16];
    for(int i = 0; i < 4; i++) {
        const GLfloat ai0=A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
        P(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
        P(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
        P(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
        P(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
    }
    memcpy(prod, p, sizeof(p));
    #undef A
    #undef B
    #undef PROD
}

static void make_z_rot_matrix(GLfloat angle, GLfloat *m)
{
    GLfloat c = cos(angle * acos(-1) / 180.0);
    GLfloat s = sin(angle * acos(-1) / 180.0);
    m[10] = m[15] = 1;
    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;
}

static void make_x_rot_matrix(GLfloat angle, GLfloat *m)
{
    GLfloat c = cos(angle * acos(-1) / 180.0);
    GLfloat s = sin(angle * acos(-1) / 180.0);
    m[0] = m[15] = 1;
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
}

static void make_y_rot_matrix(GLfloat angle, GLfloat *m)
{
    GLfloat c = cos(angle * acos(-1) / 180.0);
    GLfloat s = sin(angle * acos(-1) / 180.0);
    m[5] = m[15] = 1;
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

static void make_translation_matrix(GLfloat xpos, GLfloat ypos,
                                    GLfloat zpos, GLfloat *m)
{
    m[0] = 1;
    m[3] = xpos;
    m[5] = 1;
    m[7] = ypos;
    m[10] = 1;
    m[11] = zpos;
    m[15] = 1;
}

static void make_scale_matrix(GLfloat xs, GLfloat ys, GLfloat zs, GLfloat *m)
{
    m[0] = xs;
    m[5] = ys;
    m[10] = zs;
    m[15] = 1;
}

static void make_pers_matrix(GLfloat fov, GLfloat aspect, GLfloat near,
                            GLfloat far, GLfloat *m) {
    GLfloat D2R = acos(-1)/180.0;
    GLfloat yScale = 1.0/tan(D2R * fov / 2);
    GLfloat xScale = yScale/aspect;
    GLfloat nearmfar = near - far;
    m[0] = xScale;
    m[5] = yScale;
    m[11] = (far + near) / nearmfar;
    m[12] = -1;
    m[15] = 2*far*near / nearmfar;
}

void graph_reset_viewport(void)
{
    glViewport(0, 0, option->width, option->height);
}

void graph_set_view(graph_window *win)
{
    make_translation_matrix(win->camera.tr_x, win->camera.tr_y, win->camera.tr_z, transl);

    make_scale_matrix(win->camera.aspect_ratio*win->camera.scalefactor,
                    win->camera.scalefactor,
                    win->camera.scalefactor, scale);

    make_pers_matrix(10, option->width/option->height, -1, 10, pers);

    make_x_rot_matrix(win->camera.view_rotx, rotx);
    make_y_rot_matrix(win->camera.view_roty, roty);
    make_z_rot_matrix(win->camera.view_rotz, rotz);
    mul_matrix(mat, roty, rotx);
    mul_matrix(rotation, mat, rotz);
    glUniformMatrix4fv(trn_matrix, 1, GL_FALSE, transl);
    glUniformMatrix4fv(scl_matrix, 1, GL_FALSE, scale);
    glUniformMatrix4fv(rot_matrix, 1, GL_FALSE, rotation);
    glUniformMatrix4fv(per_matrix, 1, GL_FALSE, pers);
}

GLuint graph_compile_shader(const char *src_vert_shader,
                            const char *src_frag_shader)
{
    GLint status_vert, status_frag;
    GLuint program = glCreateProgram();
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vert_shader, 1, &src_vert_shader, NULL);
    glShaderSource(frag_shader, 1, &src_frag_shader, NULL);
    glCompileShader(vert_shader);
    glCompileShader(frag_shader);
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &status_vert);
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &status_frag);
    if(!status_vert || !status_frag) {
        if(!status_vert)
            pprintf(PRI_ERR, "[GL] VS did not compile.\n");
        if(!status_frag)
            pprintf(PRI_ERR, "[GL] FS did not compile.\n");
        exit(1);
    }

    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status_vert);
    if (!status_vert) {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(program, 1000, &len, log);
        pprintf(PRI_ERR, "[GL] Linking:\n%s\n", log);
        exit(1);
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return program;
}

void graph_draw_scene(graph_window *win)
{
    if(!win)
        return;
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    char osdtext[OSD_BUFFER];
    struct timespec ts;
    const GLfloat *fpscolor;

    /*	Text/static drawing	*/
    {
        /* Set shader and vbo */
        glUseProgram(text_shader);
        glBindBuffer(GL_ARRAY_BUFFER, textvbo);

        /* FPS */
        fpscolor = win->fps < 25 ? COL_RED : win->fps < 48 ? COL_BLUE : COL_GREEN;
        snprintf(osdtext, OSD_BUFFER, "FPS = %3.2f", win->fps);
        graph_display_text(osdtext, FPSx, FPSy, FPSs, fpscolor);

        /* Objects */
        snprintf(osdtext, OSD_BUFFER, "Objects = %u", option->obj);
        graph_display_text(osdtext, OBJx, OBJy, OBJs, COL_WHITE);

        /* Timestep display */
        snprintf(osdtext, OSD_BUFFER, "Timestep = %0.4Lf", phys_stats->progress);
        graph_display_text(osdtext, DTx, DTy, DTs, COL_WHITE);

        /* Time constant */
        snprintf(osdtext, OSD_BUFFER, "dt = %lf", option->dt);
        graph_display_text(osdtext, TIMEx, TIMEy, TIMEs, COL_WHITE);

        /* Algorithm display */
        snprintf(osdtext, OSD_BUFFER, "Algorithm = %s", option->algorithm);
        graph_display_text(osdtext, ALGx, ALGy, ALGs, COL_WHITE);

        /* Object selection */
        if (win->start_selection)
            graph_display_text(win->currentsel, OSLx, OSLy, OSLs, COL_WHITE);

        /* Chosen object */
        if (win->chosen)
            graph_display_object_info(win->object, win->chosen);

        int status = phys_ctrl(PHYS_STATUS, NULL);
        if (status == PHYS_STATUS_RUNNING || status == PHYS_STATUS_PAUSED) {
            /* Only displayed if running */

            if (status == PHYS_STATUS_PAUSED) {
                graph_display_text("Simulation paused",
                                SIMx, SIMy, SIMs, COL_YELLOW);
            }

            unsigned int len = 0;
            char res[15] = {0};
            /* Stats */
            for (unsigned int j = 0; j <  phys_stats->threads; j++) {
                len = 0;
                for (struct parser_map *i = phys_stats->t_stats[j].thread_stats_map;
                    i->name; i++) {
                    unsigned int word_len = strlen(i->name);
                    len += word_len;
                    if(!j) {
                        graph_display_text(i->name,
                                        THREAD_MAPx-((float)len/65),
                                        THREAD_MAPy, THREAD_MAPs,
                                        COL_YELLOW);
                    }
                    parser_get_value_str(*i, res, sizeof(res));
                    graph_display_text(res, THREAD_MAPx-((float)len/65),
                                    THREAD_MAPy-0.07-((float)j/14),
                                    THREAD_MAPs, COL_ORANGE);
                }

                /* Title, now that we know the final position */
                graph_display_text("Thread statistics:",
                                THREAD_MAPx-((float)len/65),
                                THREAD_MAPy+0.06, THREAD_MAPs+0.1,
                                COL_WHITE);

                /* Numbers */
                for (unsigned int j = 0; j <  phys_stats->threads; j++) {
                    snprintf(res, sizeof(res), "%i.\n", j);
                    graph_display_text(res, THREAD_MAPx-((float)len/65)-0.04,
                                    THREAD_MAPy-0.07-((float)j/14),
                                    THREAD_MAPs, COL_YELLOW);
                }
            }

            /* Thread time stats */
            for (unsigned int i = 0; i <  phys_stats->threads; i++) {
                if (phys_stats->t_stats) {
                    clock_gettime(phys_stats->t_stats[i].clockid, &ts);
                } else {
                    ts = (struct timespec){0};
                }
                snprintf(osdtext, OSD_BUFFER,
                        "Thread %u = %ld.%ld", i, ts.tv_sec, ts.tv_nsec/1000000);
                graph_display_text(osdtext,
                                THRx, THRy-((float)i/14), THRs, COL_WHITE);
            }

        } else {
            /* Only displayed if not running */

            /* Simulation status */
            graph_display_text("Simulation stopped", SIMx, SIMy, SIMs, COL_RED);
        }

        /* Algorithm stats */
        graph_display_text("Global statistics:", STATSx, STATSy, STATSs, COL_WHITE);
        graph_display_text("Name", STATSx, STATSy-.05, STATSs, COL_ORANGE);
        graph_display_text("Value", STATSx+.25, STATSy-.05, STATSs, COL_YELLOW);

        unsigned int count = 0;
        for (struct parser_map *i = phys_stats->global_stats_map; i->name; i++) {
            char res[15];
            parser_get_value_str(*i, res, sizeof(res));
            graph_display_text(i->name, STATSx,
                            STATSy-((float)count/18)-.10, STATSs, COL_ORANGE);
            graph_display_text(res, STATSx+.25,
                            STATSy-((float)count++/18)-.10, STATSs, COL_YELLOW);
        }

        /* Lua print */
        if (option->lua_print)
            graph_display_text(option->lua_print, LUAx, LUAy, LUAs, COL_WHITE);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    /*	Text/static drawing	*/

    /*	Dynamic drawing	*/
    {
        /* Shader and VBO */
        glUseProgram(object_shader);
        glBindBuffer(GL_ARRAY_BUFFER, pointvbo);

        /* Axis */
        draw_obj_axis(AXISs);

        /* Objects(as points) */
        draw_objs_mode(win->object, win->draw_mode);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /* Take screenshot if signaled by physics ctrl thread */
    if (option->write_sshot_now) {
        graph_sshot(phys_stats->total_steps);
        option->write_sshot_now = false;
    }
    /*	Dynamic drawing	*/
}

int graph_set_draw_mode(graph_window *win, const char *mode)
{
    struct parser_map draw_modes[] = {
        {"MODE_SPRITE",       NULL,   MODE_SPRITE,       LUA_TNUMBER   },
        {"MODE_SPHERE",       NULL,   MODE_SPHERE,       LUA_TNUMBER   },
        {"MODE_POINTS",       NULL,   MODE_POINTS,       LUA_TNUMBER   },
        {"MODE_POINTS_COL",   NULL,   MODE_POINTS_COL,   LUA_TNUMBER   },
        {0},
    };

    if (!mode)
        goto not_found;

    for (struct parser_map *i = draw_modes; i->name; i++) {
        if (!strcmp(i->name, mode)) {
            free(option->default_draw_mode);
            option->default_draw_mode = strdup(i->name);
            win->draw_mode = i->type;
            pprint_deb("Draw mode set to %s\n", option->default_draw_mode);
            return 0;
        }
    }

    pprint_err("Draw mode %s not found. ");
    not_found:
        pprint_err("Possible modes: ", mode);
        for (struct parser_map *i = draw_modes; i->name; i++) {
            pprint("%s ", i->name);
        }
        pprint("\nSetting mode to default MODE_POINTS_COL\n");
        free(option->default_draw_mode);
        option->default_draw_mode = strdup("MODE_POINTS_COL");
        win->draw_mode = MODE_POINTS_COL;

    return 1;
}

void graph_quit(void)
{
    free(mat);
    free(rotx);
    free(roty);
    free(rotz);
    free(rotation);
    free(scale);
    free(pers);
    free(transl);

    graph_stop_fontconfig();
    graph_stop_freetype();
}

void graph_init(void)
{
    mat       =  calloc(16, sizeof(GLfloat));
    rotx      =  calloc(16, sizeof(GLfloat));
    roty      =  calloc(16, sizeof(GLfloat));
    rotz      =  calloc(16, sizeof(GLfloat));
    rotation  =  calloc(16, sizeof(GLfloat));
    scale     =  calloc(16, sizeof(GLfloat));
    pers      =  calloc(16, sizeof(GLfloat));
    transl    =  calloc(16, sizeof(GLfloat));

    object_shader = graph_init_objects();
    text_shader = graph_init_freetype(graph_init_fontconfig());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glActiveTexture(GL_TEXTURE0);
    glGenBuffers(1, &pointvbo);
    glGenBuffers(1, &textvbo);
    glClearColor(option->bgcolor[0], option->bgcolor[1],
                option->bgcolor[2], option->bgcolor[3]);
    pprintf(PRI_SPAM, "[GL] OpenGL Version %s\n", glGetString(GL_VERSION));

    trn_matrix = glGetUniformLocation(object_shader, "translMat");
    rot_matrix = glGetUniformLocation(object_shader, "rotationMat");
    scl_matrix = glGetUniformLocation(object_shader, "scalingMat");
    per_matrix = glGetUniformLocation(object_shader, "perspectiveMat");
}

struct raw_png_read {
    /*
    * PNG stands for Portable Network Graphics. Notice that "Network" word in
    * there. And guess what? The old greybeards decided anything "Network"
    * related must absolutely be big endian. We have to use libpng's own type.
    */
    png_bytep bin;
    png_uint_32 pos;
    png_uint_32 len;
};

static void graph_read_raw_stream(png_structp png, png_bytep output,
                                png_size_t readsize)
{
    struct raw_png_read *data = (struct raw_png_read *)png_get_io_ptr(png);
    if (!data)
        return;

    if (readsize > data->len) {
        png_error(png, "EOF");
        return;
    }

    /* We could move the pointer instead, but oh well, whatever */
    memcpy(output, (data->bin+data->pos), readsize);

    data->pos += readsize;
    data->len -= readsize;
}

GLuint graph_load_c_png_texture(void *bin, size_t len, GLint *width, GLint *height)
{
    int bit_depth, color_type;
    size_t rowsize;
    png_byte header[8];
    png_uint_32 twidth, theight;
    png_structp png = NULL;
    png_infop info = NULL;
    png_infop info_end = NULL;
    GLuint texture = 0;

    /* Read header */
    memcpy(header, bin, 8);
    int is_png = !png_sig_cmp(header, 0, 8);
    if (!is_png)
        return 1;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        return 1;

    info = png_create_info_struct(png);
    if (!info)
        goto clean;

    info_end = png_create_info_struct(png);
    if (!info_end)
        goto clean;

    if (setjmp(png_jmpbuf(png)))
        goto clean;

    struct raw_png_read read_stuff = {
        .bin = bin,
        .pos = 0,          /* Don't put 8 byte header offset here, NOT A BUG. */
        .len = len,
    };

    /* Set custom input fn */
    png_set_read_fn(png, &read_stuff, graph_read_raw_stream);

    /* Skip to actual image. Yes, it will skip the 8 byte header. */
    png_read_info(png, info);

    /* Get info */
    png_get_IHDR(png, info, &twidth, &theight, &bit_depth, &color_type,
                NULL, NULL, NULL);

    /* Set width and height */
    if (width)
        *width = twidth;
    if (height)
        *height = theight;

    png_read_update_info(png, info);

    rowsize = png_get_rowbytes(png, info);

    /* Allocate */
    png_byte *image = png_malloc(png, theight*rowsize);

    /* Allocate rows */
    png_bytep *rows = png_malloc(png, sizeof(png_bytep)*theight);

    /* Set pointers */
    for (int i = 0; i < theight; i++)
        rows[theight - 1 - i] = image + i*rowsize;

    /* Actually read the image */
    png_read_image(png, rows);

    /* Create texture */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA, twidth, theight, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    clean:
        if(png) {
            png_free(png, rows);
            png_free(png, image);
            png_destroy_read_struct(&png, &info, &info_end);
        }

    return texture;
}

GLuint graph_load_png_texture(const char *filename, GLint *width, GLint *height)
{
    int bit_depth, color_type;
    size_t rowsize;
    png_byte header[8];
    png_uint_32 twidth, theight;
    png_structp png = NULL;
    png_infop info = NULL;
    png_infop info_end = NULL;
    GLuint texture = 0;

    /* Open file */
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        goto clean;

    /* Read header */
    (void)fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
        goto clean;

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        goto clean;

    info = png_create_info_struct(png);
    if (!info)
        goto clean;

    info_end = png_create_info_struct(png);
    if (!info_end)
        goto clean;

    if (setjmp(png_jmpbuf(png)))
        goto clean;

    /* Init IO */
    png_init_io(png, fp);

    /* Move offset since header was read */
    png_set_sig_bytes(png, 8);

    /* Skip to actual image */
    png_read_info(png, info);

    /* Get info */
    png_get_IHDR(png, info, &twidth, &theight, &bit_depth, &color_type,
                NULL, NULL, NULL);

    /* Set width and height */
    if (width)
        *width = twidth;
    if (height)
        *height = theight;

    png_read_update_info(png, info);

    rowsize = png_get_rowbytes(png, info);

    /* Allocate */
    png_byte *image = png_malloc(png, theight*rowsize);

    /* Allocate rows */
    png_bytep *rows = png_malloc(png, sizeof(png_bytep)*theight);

    /* Set pointers */
    for (int i = 0; i < theight; i++)
        rows[theight - 1 - i] = image + i*rowsize;

    /* Actually read the image */
    png_read_image(png, rows);

    /* Create texture */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA, twidth, theight, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    clean:
        if(png) {
            png_free(png, rows);
            png_free(png, image);
            png_destroy_read_struct(&png, &info, &info_end);
        }
        fclose(fp);

    return texture;
}

int graph_sshot(unsigned long long int total_steps)
{
    /* Open file */
    char filename[32];
    int w = option->width, h = option->height;

    /* Open file */
    snprintf(filename, sizeof(filename), option->sshot_temp, total_steps);
    if (!access(filename, R_OK))
        return 2;

    FILE *fshot = fopen(filename, "w");
    if (!fshot)
        return 2;

    /* Get pixels */
    unsigned char *pixels = malloc(sizeof(unsigned char)*w*h*4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                            NULL, NULL, NULL);
    if (!png)
        return 1;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, &info);
        return 1;
    }

    png_init_io(png, fshot);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_colorp palette = png_malloc(png, PNG_MAX_PALETTE_LENGTH*sizeof(png_color));
    if (!palette) {
        fclose(fshot);
        png_destroy_write_struct(&png, &info);
        return 1;
    }

    png_set_PLTE(png, info, palette, PNG_MAX_PALETTE_LENGTH);
    png_write_info(png, info);
    png_set_packing(png);

    png_bytepp rows = png_malloc(png, h*sizeof(png_bytep));
    for (int r = 0; r < h; r++)
        rows[r] = (pixels + (h - r)*w*4);

    png_write_image(png, rows);
    png_write_end(png, info);

    png_free(png, rows);
    png_free(png, palette);
    png_destroy_write_struct(&png, &info);

    fclose(fshot);
    free(pixels);

    pprintf(PRI_MEDIUM, "Wrote screenshot %s\n", filename);

    return 0;
}
