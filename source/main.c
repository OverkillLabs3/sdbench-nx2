/* SD Bench for the Nintendo Switch 2: sequential and random speed of the SD card in the console.
 * Runs on start and again on A. Results are shown on screen and appended to sdmc:/switch/sdbench/results.txt. */
#include <SDL.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include <switch.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

extern const uint8_t font_bin[];

#define DIR_PATH   "sdmc:/switch/sdbench"
#define TMP_PATH   DIR_PATH "/test.bin"
#define LOG_PATH   DIR_PATH "/results.txt"
#define FILE_MB    512
#define CHUNK      (4u << 20)
#define RND_BLOCK  4096
#define RND_READS  4000
#define RND_WRITES 1500
#define SMALL_FILES 300
#define NROWS      6

/* ---- text ---- */
typedef struct { SDL_Texture *tex; stbtt_bakedchar g[96]; float px; } Font;
enum { F_S, F_M, F_L, NFONTS };
static Font fonts[NFONTS];
static const float font_px[NFONTS] = { 22.0f, 30.0f, 44.0f };
#define ATLAS_W 1024
#define ATLAS_H 512

static SDL_Window *win;
static SDL_Renderer *ren;

static int init_fonts(void)
{
    uint8_t *alpha = malloc(ATLAS_W * ATLAS_H);
    uint32_t *rgba = malloc(ATLAS_W * ATLAS_H * 4);
    if (!alpha || !rgba) return -1;
    for (int f = 0; f < NFONTS; f++) {
        fonts[f].px = font_px[f];
        if (stbtt_BakeFontBitmap(font_bin, 0, font_px[f], alpha, ATLAS_W, ATLAS_H, 32, 96, fonts[f].g) <= 0) return -1;
        for (int i = 0; i < ATLAS_W * ATLAS_H; i++) rgba[i] = ((uint32_t)alpha[i] << 24) | 0x00FFFFFF;
        fonts[f].tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, ATLAS_W, ATLAS_H);
        if (!fonts[f].tex) return -1;
        SDL_UpdateTexture(fonts[f].tex, NULL, rgba, ATLAS_W * 4);
        SDL_SetTextureBlendMode(fonts[f].tex, SDL_BLENDMODE_BLEND);
    }
    free(alpha);
    free(rgba);
    return 0;
}

static float text_w(int f, const char *s)
{
    float x = 0, y = 0;
    stbtt_aligned_quad q;
    for (; *s; s++)
        if (*s >= 32 && *s < 128) stbtt_GetBakedQuad(fonts[f].g, ATLAS_W, ATLAS_H, *s - 32, &x, &y, &q, 1);
    return x;
}

typedef struct { uint8_t r, g, b; } Col;
static const Col C_TEXT = { 236, 240, 252 }, C_DIM = { 128, 140, 176 }, C_TEAL = { 88, 226, 204 },
                 C_VIOLET = { 160, 124, 255 }, C_TRACK = { 44, 52, 86 }, C_WARN = { 255, 206, 110 };

static void text(int f, float x, float y, const char *s, Col c, int align)
{
    if (align == 1) x -= text_w(f, s);
    else if (align == 2) x -= text_w(f, s) / 2;
    float cx = x, cy = y + fonts[f].px * 0.8f;
    SDL_SetTextureColorMod(fonts[f].tex, c.r, c.g, c.b);
    for (; *s; s++) {
        if (*s < 32 || *s >= 128) continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(fonts[f].g, ATLAS_W, ATLAS_H, *s - 32, &cx, &cy, &q, 1);
        SDL_Rect src = { (int)(q.s0 * ATLAS_W + 0.5f), (int)(q.t0 * ATLAS_H + 0.5f),
                         (int)((q.s1 - q.s0) * ATLAS_W + 0.5f), (int)((q.t1 - q.t0) * ATLAS_H + 0.5f) };
        SDL_Rect dst = { (int)q.x0, (int)q.y0, (int)(q.x1 - q.x0 + 0.5f), (int)(q.y1 - q.y0 + 0.5f) };
        SDL_RenderCopy(ren, fonts[f].tex, &src, &dst);
    }
}

/* ---- shapes ---- */
static void color(Col c, int a) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, a); }

static void rrect(int x, int y, int w, int h, int r, Col c, int a)
{
    color(c, a);
    if (r * 2 > h) r = h / 2;
    for (int yy = 0; yy < h; yy++) {
        int inset = 0;
        if (yy < r) inset = r - (int)sqrtf((float)(r * r - (r - yy - 0.5f) * (r - yy - 0.5f)));
        else if (yy >= h - r) { float d = yy - (h - r) + 0.5f; inset = r - (int)sqrtf((float)(r * r - d * d)); }
        SDL_Rect l = { x + inset, y + yy, w - 2 * inset, 1 };
        SDL_RenderFillRect(ren, &l);
    }
}

static Col lerp(Col a, Col b, float t)
{
    Col c = { (uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t), (uint8_t)(a.b + (b.b - a.b) * t) };
    return c;
}

/* horizontal gradient bar: teal to violet, round ends */
static void bar(int x, int y, int w, int h, float frac, Col from, Col to)
{
    rrect(x, y, w, h, h / 2, C_TRACK, 255);
    int fw = (int)(w * (frac < 0 ? 0 : frac > 1 ? 1 : frac));
    if (fw < h) return;
    rrect(x, y, h, h, h / 2, from, 255);
    rrect(x + fw - h, y, h, h, h / 2, lerp(from, to, (float)fw / w), 255);
    for (int i = h / 2; i < fw - h / 2; i += 4) {
        int sw = i + 4 > fw - h / 2 ? fw - h / 2 - i : 4;
        color(lerp(from, to, (float)i / w), 255);
        SDL_Rect s = { x + i, y, sw, h };
        SDL_RenderFillRect(ren, &s);
    }
}

/* ---- state ---- */
typedef struct {
    const char *label;
    char main[32], sub[32];
    float ref, frac;
    int done;
} Row;
static Row rows[NROWS] = {
    { "Sequential write", "", "", 300, 0, 0 },     { "Sequential read", "", "", 300, 0, 0 },
    { "Random read 4 KB", "", "", 10000, 0, 0 },   { "Random write 4 KB", "", "", 10000, 0, 0 },
    { "Create small files", "", "", 1000, 0, 0 },  { "Delete small files", "", "", 1000, 0, 0 },
};
#define NSROWS 7
static Row srows[NSROWS] = {
    { "64 KB blocks", "", "", 300, 0, 0 },          { "256 KB blocks", "", "", 300, 0, 0 },
    { "1 MB blocks", "", "", 300, 0, 0 },           { "4 MB blocks", "", "", 300, 0, 0 },
    { "16 MB blocks", "", "", 300, 0, 0 },          { "4 MB blocks, no flush", "", "", 300, 0, 0 },
    { "4 MB blocks, file pre-sized", "", "", 300, 0, 0 },
};
static int sweep_mode;
static int cur = -1;
static float progress;
static char status[128] = "Starting";
static char card_total[48] = "", card_free[48] = "";
static int running, quit;

static void set_row(int i, double v, const char *fmt_main, const char *sub_fmt, double sub)
{
    Row *r = sweep_mode ? &srows[i] : &rows[i];
    snprintf(r->main, sizeof r->main, fmt_main, v);
    if (sub_fmt) snprintf(r->sub, sizeof r->sub, sub_fmt, sub);
    r->frac = (float)(log10(1.0 + v) / log10(1.0 + r->ref));
    r->done = 1;
}

static void chip(int x, int y, const char *key, const char *label, int *outw)
{
    rrect(x, y, 34, 34, 17, C_VIOLET, 255);
    text(F_S, x + 17, y + 3, key, (Col){ 20, 16, 40 }, 2);
    text(F_S, x + 46, y + 3, label, C_TEXT, 0);
    *outw = 46 + (int)text_w(F_S, label);
}

static void draw(void)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < 720; y += 4) {
        Col c = lerp((Col){ 9, 13, 30 }, (Col){ 24, 14, 50 }, y / 720.0f);
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
        SDL_Rect s = { 0, y, 1280, 4 };
        SDL_RenderFillRect(ren, &s);
    }
    text(F_L, 48, 26, "SD Bench", C_TEXT, 0);
    text(F_S, 50, 82, sweep_mode ? "Write speed sweep, 256 MB per step" : "Nintendo Switch 2 storage benchmark", C_DIM, 0);
    text(F_M, 1232, 30, card_total, C_TEXT, 1);
    text(F_S, 1232, 70, card_free, C_DIM, 1);
    bar(48, 118, 1184, 4, 1.0f, C_TEAL, C_VIOLET);

    Row *R = sweep_mode ? srows : rows;
    int N = sweep_mode ? NSROWS : NROWS, pitch = sweep_mode ? 68 : 80, rh = sweep_mode ? 58 : 70;
    for (int i = 0; i < N; i++) {
        Row *r = &R[i];
        int y = 142 + i * pitch;
        int active = running && i == cur;
        rrect(48, y, 1184, rh, 14, (Col){ 30, 38, 72 }, active ? 255 : 215);
        if (active) rrect(48, y, 5, rh, 2, C_VIOLET, 255);
        int ty = y + (rh == 70 ? 8 : 4);
        text(F_M, 76, ty, r->label, r->done || active ? C_TEXT : C_DIM, 0);
        if (r->done) {
            text(F_M, 1204, ty, r->main, C_TEAL, 1);
            if (r->sub[0]) text(F_S, 1204 - text_w(F_M, r->main) - 18, ty + 4, r->sub, C_DIM, 1);
            bar(76, y + rh - 18, 1128, 8, r->frac, C_TEAL, C_VIOLET);
        } else if (active) {
            text(F_M, 1204, ty, "Running", C_WARN, 1);
            bar(76, y + rh - 18, 1128, 8, progress, C_VIOLET, C_TEAL);
        } else {
            text(F_M, 1204, ty, "-", C_DIM, 1);
            bar(76, y + rh - 18, 1128, 8, 0, C_TEAL, C_VIOLET);
        }
    }

    int w, x = 48;
    chip(x, 640, "A", "Run again", &w);
    x += w + 36;
    chip(x, 640, "X", sweep_mode ? "Run sweep again" : "Write sweep", &w);
    x += w + 36;
    if (sweep_mode) { chip(x, 640, "Y", "Back", &w); x += w + 36; }
    chip(x, 640, "+", "Exit", &w);
    text(F_S, 1232, 643, status, running ? C_WARN : C_DIM, 1);
    SDL_RenderPresent(ren);
}

static void pump(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) quit = 1;
        if (e.type == SDL_JOYBUTTONDOWN && (e.jbutton.button == 10 || e.jbutton.button == 1)) quit = 1;
    }
}

static void tick(float p)
{
    progress = p;
    pump();
    draw();
}

/* ---- tests ---- */
static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static uint64_t rng = 88172645463325252ull;
static uint64_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}

static int seq_test(int wr, uint8_t *buf, double *mbs)
{
    uint64_t total = (uint64_t)FILE_MB << 20, done = 0;
    int fd = open(TMP_PATH, wr ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY, 0644);
    if (fd < 0) return -1;
    double t0 = now_s();
    int n = 0;
    while (done < total) {
        ssize_t r = wr ? write(fd, buf, CHUNK) : read(fd, buf, CHUNK);
        if (r <= 0) { close(fd); return -2; }
        done += r;
        if ((++n & 3) == 0) tick((float)done / total);
    }
    if (wr) fsync(fd);
    close(fd);
    *mbs = done / 1e6 / (now_s() - t0);
    return 0;
}

static int rand_test(int wr, uint8_t *buf, double *iops, double *mbs)
{
    int count = wr ? RND_WRITES : RND_READS;
    uint64_t blocks = ((uint64_t)FILE_MB << 20) / RND_BLOCK;
    int fd = open(TMP_PATH, wr ? O_WRONLY : O_RDONLY);
    if (fd < 0) return -1;
    double t0 = now_s();
    for (int i = 0; i < count; i++) {
        off_t off = (off_t)(rnd() % blocks) * RND_BLOCK;
        if (lseek(fd, off, SEEK_SET) < 0) { close(fd); return -2; }
        ssize_t r = wr ? write(fd, buf, RND_BLOCK) : read(fd, buf, RND_BLOCK);
        if (r != RND_BLOCK) { close(fd); return -3; }
        if ((i & 63) == 0) tick((float)i / count);
    }
    if (wr) fsync(fd);
    close(fd);
    double dt = now_s() - t0;
    *iops = count / dt;
    *mbs = count * (double)RND_BLOCK / 1e6 / dt;
    return 0;
}

static int small_test(uint8_t *buf, double *create_s, double *delete_s)
{
    char p[160];
    double t0 = now_s();
    for (int i = 0; i < SMALL_FILES; i++) {
        snprintf(p, sizeof p, DIR_PATH "/s%04d.tmp", i);
        int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return -1;
        if (write(fd, buf, 4096) != 4096) { close(fd); return -2; }
        close(fd);
        if ((i & 7) == 0) tick((float)i / SMALL_FILES * 0.5f);
    }
    double t1 = now_s();
    cur = 5;
    for (int i = 0; i < SMALL_FILES; i++) {
        snprintf(p, sizeof p, DIR_PATH "/s%04d.tmp", i);
        unlink(p);
        if ((i & 7) == 0) tick(0.5f + (float)i / SMALL_FILES * 0.5f);
    }
    double t2 = now_s();
    *create_s = SMALL_FILES / (t1 - t0);
    *delete_s = SMALL_FILES / (t2 - t1);
    return 0;
}

static void run_all(void)
{
    for (int i = 0; i < NROWS; i++) { rows[i].done = 0; rows[i].main[0] = rows[i].sub[0] = 0; rows[i].frac = 0; }
    running = 1;
    cur = 0;
    uint8_t *buf = memalign(4096, CHUNK);
    if (!buf) { snprintf(status, sizeof status, "Out of memory"); running = 0; draw(); return; }
    for (size_t i = 0; i < CHUNK; i++) buf[i] = (uint8_t)(i * 2654435761u >> 24);
    mkdir("sdmc:/switch", 0777);
    mkdir(DIR_PATH, 0777);

    struct statvfs sv;
    char card[96];
    if (statvfs("sdmc:/", &sv) == 0) {
        double tot = (double)sv.f_blocks * sv.f_frsize / 1e9, fre = (double)sv.f_bavail * sv.f_frsize / 1e9;
        snprintf(card_total, sizeof card_total, "%.1f GB card", tot);
        snprintf(card_free, sizeof card_free, "%.1f GB free", fre);
        snprintf(card, sizeof card, "%.1f GB total, %.1f GB free", tot, fre);
    } else {
        snprintf(card_total, sizeof card_total, "SD card");
        snprintf(card_free, sizeof card_free, "size unknown");
        snprintf(card, sizeof card, "size unknown");
    }

    double a, b, c;
    int rc;
    FILE *lf = fopen(LOG_PATH, "a");
    if (lf) fprintf(lf, "--- run at unix time %lld, card %s\n", (long long)time(NULL), card);

    snprintf(status, sizeof status, "Testing %d MB file", FILE_MB);
    cur = 0; tick(0);
    rc = seq_test(1, buf, &a);
    if (rc) { snprintf(status, sizeof status, "Write failed (%d), is the card full?", rc); goto end; }
    set_row(0, a, "%.1f MB/s", NULL, 0);
    if (lf) fprintf(lf, "sequential write %.1f MB/s\n", a);

    cur = 1; tick(0);
    rc = seq_test(0, buf, &a);
    if (rc) { snprintf(status, sizeof status, "Read failed (%d)", rc); goto end; }
    set_row(1, a, "%.1f MB/s", NULL, 0);
    if (lf) fprintf(lf, "sequential read %.1f MB/s\n", a);

    cur = 2; tick(0);
    rc = rand_test(0, buf, &a, &b);
    if (rc) { snprintf(status, sizeof status, "Random read failed (%d)", rc); goto end; }
    set_row(2, a, "%.0f IOPS", "%.1f MB/s", b);
    if (lf) fprintf(lf, "random read 4K %.0f IOPS %.1f MB/s\n", a, b);

    cur = 3; tick(0);
    rc = rand_test(1, buf, &a, &b);
    if (rc) { snprintf(status, sizeof status, "Random write failed (%d)", rc); goto end; }
    set_row(3, a, "%.0f IOPS", "%.1f MB/s", b);
    if (lf) fprintf(lf, "random write 4K %.0f IOPS %.1f MB/s\n", a, b);

    cur = 4; tick(0);
    rc = small_test(buf, &b, &c);
    if (rc) { snprintf(status, sizeof status, "Small files failed (%d)", rc); goto end; }
    set_row(4, b, "%.0f files/s", NULL, 0);
    set_row(5, c, "%.0f files/s", NULL, 0);
    if (lf) fprintf(lf, "create small files %.0f/s, delete %.0f/s\n", b, c);

    snprintf(status, sizeof status, "Done, saved to results.txt");
end:
    if (rc && lf) fprintf(lf, "stopped: %s\n", status);
    if (lf) fclose(lf);
    unlink(TMP_PATH);
    free(buf);
    running = 0;
    cur = -1;
    draw();
}

/* Write speed against block size, flush and pre-sizing, 256 MB per step. */
static int write_step(unsigned block, int flush, int presize, uint8_t *buf, double *mbs)
{
    uint64_t total = 256ull << 20, done = 0;
    int fd = open(TMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (presize && ftruncate(fd, (off_t)total) != 0) { close(fd); return -3; }
    double t0 = now_s();
    unsigned n = 0;
    while (done < total) {
        ssize_t r = write(fd, buf, block);
        if (r != (ssize_t)block) { close(fd); return -2; }
        done += r;
        if ((++n & 3) == 0 || block >= (1u << 20)) tick((float)done / total);
    }
    if (flush) fsync(fd);
    close(fd);
    *mbs = done / 1e6 / (now_s() - t0);
    unlink(TMP_PATH);
    return 0;
}

static void sweep_all(void)
{
    static const unsigned blk[NSROWS] = { 64u << 10, 256u << 10, 1u << 20, 4u << 20, 16u << 20, 4u << 20, 4u << 20 };
    static const int flush[NSROWS] = { 1, 1, 1, 1, 1, 0, 1 }, presize[NSROWS] = { 0, 0, 0, 0, 0, 0, 1 };
    for (int i = 0; i < NSROWS; i++) { srows[i].done = 0; srows[i].main[0] = srows[i].sub[0] = 0; srows[i].frac = 0; }
    sweep_mode = 1;
    running = 1;
    uint8_t *buf = memalign(4096, 16u << 20);
    if (!buf) { snprintf(status, sizeof status, "Out of memory"); running = 0; draw(); return; }
    for (size_t i = 0; i < (16u << 20); i++) buf[i] = (uint8_t)(i * 2654435761u >> 24);
    mkdir("sdmc:/switch", 0777);
    mkdir(DIR_PATH, 0777);
    FILE *lf = fopen(LOG_PATH, "a");
    if (lf) fprintf(lf, "--- write sweep at unix time %lld, card %s, %s\n", (long long)time(NULL), card_total, card_free);
    snprintf(status, sizeof status, "Sweeping");
    int rc = 0;
    for (int i = 0; i < NSROWS && !quit; i++) {
        double a;
        cur = i;
        tick(0);
        rc = write_step(blk[i], flush[i], presize[i], buf, &a);
        if (rc) { snprintf(status, sizeof status, "Write failed (%d)", rc); break; }
        set_row(i, a, "%.1f MB/s", NULL, 0);
        if (lf) fprintf(lf, "write %s: %.1f MB/s\n", srows[i].label, a);
    }
    if (!rc) snprintf(status, sizeof status, "Done, saved to results.txt");
    if (lf) fclose(lf);
    unlink(TMP_PATH);
    free(buf);
    running = 0;
    cur = -1;
    draw();
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) return 1;
    for (int i = 0; i < SDL_NumJoysticks(); i++) SDL_JoystickOpen(i);
    win = SDL_CreateWindow("SD Bench", 0, 0, 1280, 720, SDL_WINDOW_SHOWN);
    if (!win) { SDL_Quit(); return 2; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren || init_fonts() < 0) { SDL_Quit(); return 3; }
    SDL_RenderSetLogicalSize(ren, 1280, 720);

    draw();
    run_all();
    while (!quit) {
        SDL_Event e;
        while (SDL_WaitEventTimeout(&e, 100)) {
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type == SDL_JOYBUTTONDOWN) {
                if (e.jbutton.button == 10 || e.jbutton.button == 1) quit = 1;
                if (e.jbutton.button == 0) { sweep_mode = 0; run_all(); }
                if (e.jbutton.button == 2) sweep_all();
                if (e.jbutton.button == 3 && sweep_mode) sweep_mode = 0;
            }
        }
        draw();
    }
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
