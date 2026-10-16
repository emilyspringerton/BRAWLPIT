/* Headless tests for packages/common/custom_hat.h (the WOTAN Hat Studio ".hat" format). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../packages/common/custom_hat.h"

static int make(char *buf, size_t cap, int w, int h, int fill_alpha, int rows) {
    int n = snprintf(buf, cap, "HAT1\n%d %d\n", w, h);
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < w; x++) n += snprintf(buf + n, cap - n, "%s%s", x ? " " : "", fill_alpha ? "c07dffff" : "00000000");
        n += snprintf(buf + n, cap - n, "\n");
    }
    return n;
}

int main(void) {
    static char buf[CUSTOM_HAT_FILE_MAX * 2];
    static CustomHat h;

    int n = make(buf, sizeof buf, 32, 32, 1, 32);
    assert(custom_hat_parse(buf, n, &h));
    assert(h.opaque == 1024);
    assert(h.rgba[0] == 0xc0 && h.rgba[1] == 0x7d && h.rgba[2] == 0xff && h.rgba[3] == 0xff);

    /* a studio export's own mixed content: transparent corner, one opaque pixel */
    n = make(buf, sizeof buf, 32, 32, 0, 32);
    memcpy(buf + 11, "ff0000ff", 8);                 /* first token of the first row */
    assert(custom_hat_parse(buf, n, &h));
    assert(h.opaque == 1 && h.rgba[0] == 0xff && h.rgba[3] == 0xff);

    n = make(buf, sizeof buf, 32, 32, 0, 32);        /* completely empty -> rejected */
    assert(!custom_hat_parse(buf, n, &h));
    n = make(buf, sizeof buf, 16, 16, 1, 16);        /* wrong size */
    assert(!custom_hat_parse(buf, n, &h));
    n = make(buf, sizeof buf, 32, 32, 1, 31);        /* a row short */
    assert(!custom_hat_parse(buf, n, &h));
    n = make(buf, sizeof buf, 32, 32, 1, 32);
    buf[20] = 'z';                                   /* bad hex digit */
    assert(!custom_hat_parse(buf, n, &h));
    assert(!custom_hat_parse("NOPE\n32 32\n", 11, &h));
    assert(!custom_hat_parse("", 0, &h));

    char name[CUSTOM_HAT_NAME_MAX];
    custom_hat_name("hats/cone-of-silence.hat", name); assert(!strcmp(name, "cone-of-silence"));
    custom_hat_name("C:\\g\\hats\\a b.hat", name);     assert(!strcmp(name, "a b"));
    custom_hat_name(".hat", name);                     assert(!strcmp(name, "Custom Hat"));
    custom_hat_name("a-very-long-hat-name-that-goes-on-and-on.hat", name);
    assert(strlen(name) == CUSTOM_HAT_NAME_MAX - 1);

    /* directory scan: two valid hats + one junk + one non-.hat file */
    char dir[] = "/tmp/brawlpit_hats_XXXXXX";
    assert(mkdtemp(dir));
    char path[600];
    n = make(buf, sizeof buf, 32, 32, 1, 32);
    snprintf(path, sizeof path, "%s/one.hat", dir);   FILE *f = fopen(path, "wb"); fwrite(buf, 1, n, f); fclose(f);
    snprintf(path, sizeof path, "%s/two.hat", dir);   f = fopen(path, "wb"); fwrite(buf, 1, n, f); fclose(f);
    snprintf(path, sizeof path, "%s/junk.hat", dir);  f = fopen(path, "wb"); fputs("HAT1\nbroken", f); fclose(f);
    snprintf(path, sizeof path, "%s/note.txt", dir);  f = fopen(path, "wb"); fputs("hi", f); fclose(f);
    setenv("BRAWLPIT_HATS_DIR", dir, 1);
    custom_hats_init();
    assert(g_custom_hat_count == 2);
    custom_hats_init();                               /* re-init must not accumulate */
    assert(g_custom_hat_count == 2);
    snprintf(path, sizeof path, "rm -rf %s", dir); (void)!system(path);

    printf("custom_hat: all tests passed\n");
    return 0;
}
