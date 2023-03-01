#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

/* xcb headers */
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xproto.h>
#include <xcb/randr.h>

enum ScreenshotType {
    Monitor, Draw
};

typedef struct {
    enum ScreenshotType type;
    uint32_t x, y, h, w;
    int ms, me;
} Screenshot;

xcb_visualtype_t *
get_root_visual_type(xcb_screen_t *s) {
    /* Get root visual */
    xcb_visualtype_t *vt;
    xcb_depth_iterator_t di;

    vt = NULL;
    di = xcb_screen_allowed_depths_iterator(s);
    for (; di.rem; xcb_depth_next(&di)) {
        xcb_visualtype_iterator_t vi;
        vi = xcb_depth_visuals_iterator (di.data);
        for (; vi.rem; xcb_visualtype_next (&vi)) {
            if (s->root_visual == vi.data->visual_id) {
                vt = vi.data;
                break;
            }
        }
    }

    return vt;
}

int
get_monitor_sz(xcb_connection_t *c, xcb_screen_t *s, int start, int end, uint32_t *mx, uint32_t *my, uint32_t *mw, uint32_t *mh)
{
    xcb_randr_get_screen_resources_current_reply_t *rs;
    xcb_randr_output_t *op;
    int x, y, w, h;
    int nm; /* number of monitors */

    /* query resources */
    rs = xcb_randr_get_screen_resources_current_reply(c,
        xcb_randr_get_screen_resources_current(c, s->root), NULL);
    op = xcb_randr_get_screen_resources_current_outputs(rs);

    x = y = w = h = 0;

    nm = xcb_randr_get_screen_resources_current_outputs_length(rs);

    if (end > nm - 1) { /* ensure end index makes sense */
        fprintf(stderr, "invalid end index for monitor: xrandr sais there is %d monitors\n", nm);
        return 1;
    }
    if (start > nm - 1) {
        fprintf(stderr, "invalid start index for monitor: xrandr sais there is %d monitors\n", nm);
        return 1;
    }

    for (int i = 0; i < nm; i++) {
        xcb_randr_get_output_info_reply_t *info = \
            xcb_randr_get_output_info_reply(c,
                xcb_randr_get_output_info(c, op[i], XCB_CURRENT_TIME), NULL);
        if (!info || info->crtc == XCB_NONE) {
            continue;
        }
        xcb_randr_get_crtc_info_reply_t *crtc = \
            xcb_randr_get_crtc_info_reply(c, \
                xcb_randr_get_crtc_info(c,
                                        info->crtc, XCB_CURRENT_TIME), NULL);
        if (!crtc) {
            continue;
        }
        if (i == start) {
            x = crtc->x;
            y = crtc->y;
        }
        if (i == end) {
            w = crtc->x + crtc->width - x;
            h = crtc->y + crtc->height - y;
        }
        free(crtc);
        free(info);
    }
    free(rs);

    *mx = x;
    *my = y;
    *mw = w;
    *mh = h;
    return 0;
}

int
get_screenshot(int argc, char *argv[], Screenshot *result)
{
    uint32_t x, y, w, h;
    int ms, me;

    if (argc < 2) { /* no args */
        result->type = Draw;
        return 0;
    }

    ms = me = -1;
    for (int i = 0; i < argc; i++) {
        if (i + 1 < argc && !strcmp("start", argv[i])) {
            ms = atoi(argv[i + 1]);
        } else if (i + 1 < argc && !strcmp("end", argv[i])) {
            me = atoi(argv[i + 1]);
        }
    }

    me = me == -1 ? ms : me;
    ms = ms == -1 ? me : ms;

    result->type = Monitor;
    result->me = me;
    result->ms = ms;

    return 0;
}

int
main(int argc, char *argv[])
{
    int rc;
    int sn;
    xcb_screen_t *s;
    xcb_connection_t *c;
    uint32_t vm, vl[4];
    int sr, sg, fr, fg, fb;
    uint32_t pix, w, h;
    uint16_t rgba[4];
    xcb_visualtype_t *vt;
    uint32_t mx, my, mw, mh; /* monitor dimensions */
    Screenshot ss;

    get_screenshot(argc, argv, &ss);

    /* initialize connection */
    c = xcb_connect(NULL, &sn);
    assert(!(xcb_connection_has_error(c) > 0));

    /* get the screen */
    s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    switch (ss.type) {
        case Monitor:
            rc = get_monitor_sz(c, s, ss.ms, ss.me, &mx, &my, &mw, &mh);
            if (rc != 0) {
                xcb_disconnect(c);
                return 1;
            }
            break;
        case Draw:
            /* TODO implement user drawn area screenshot */
            fprintf(stderr, "Feature for user drawn screenshot not yet implemented\n");
            xcb_disconnect(c);
            return 1;
            break;
    }

    xcb_image_t *img = xcb_image_get(c,
                                     s->root,
                                     mx, my, mw, mh,
                                     ~0, XCB_IMAGE_FORMAT_Z_PIXMAP);
    xcb_disconnect(c);

    switch (img->bpp) {
        case 16: /* only 5-6-5 format supported */
            sr = 11;
            sg = 5;
            fr = fb = 2047;
            fg = 1023;
            break;
        case 24:
        case 32: /* ignore alpha in case of 32-bit */
            sr = 16;
            sg = 8;
            fr = fg = fb = 257;
            break;
        default:
            errx(1, "unsupported bpp: %d", img->bpp);
    }

    vt = get_root_visual_type(s);

    /* write header with big endian width and height-values */
    fprintf(stdout, "farbfeld");
    pix = htonl(img->width);
    fwrite(&pix, sizeof(uint32_t), 1, stdout);
    pix = htonl(img->height);
    fwrite(&pix, sizeof(uint32_t), 1, stdout);

    /* write pixels */
    for (h = 0; h < (uint32_t)img->height; h++) {
        for (w = 0; w < (uint32_t)img->width; w++) {
            pix = xcb_image_get_pixel(img, w, h);
            rgba[0] = htons(((pix & vt->red_mask) >> sr) * fr);
            rgba[1] = htons(((pix & vt->green_mask) >> sg) * fg);
            rgba[2] = htons((pix & vt->blue_mask) * fb);
            rgba[3] = htons(65535);

            if (fwrite(&rgba, 4 * sizeof(uint16_t), 1, stdout) != 1)
                err(1, "fwrite");
        }
    }

    /* clean up */
    xcb_image_destroy(img);
    return 0;
}
