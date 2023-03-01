#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <arpa/inet.h>

/* xcb headers */
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xproto.h>

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
main(int argc, char *argv[])
{
    int sn;
    xcb_screen_t *s;
    xcb_connection_t *c;
    xcb_gcontext_t dgc;
    uint32_t vm, vl[4];
    int sr, sg, fr, fg, fb;
    uint32_t pix, w, h;
    uint16_t rgba[4];
    xcb_visualtype_t *vt;

    /* initialize connection */
    c = xcb_connect(NULL, &sn);
    assert(!(xcb_connection_has_error(c) > 0));

    /* get the screen */
    s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    /* setup graphics context */
    dgc = xcb_generate_id(c);
    vm = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
    vl[0] = s->black_pixel;
    vl[1] = s->white_pixel;
    xcb_create_gc(c, dgc, s->root, vm, vl);

    /* get root window geometry */
    xcb_get_geometry_reply_t *gm = xcb_get_geometry_reply(c,
                                                          xcb_get_geometry(c, s->root), NULL);

    xcb_image_t *img = xcb_image_get(c,
                                     s->root,
                                     0, 0, gm->width, gm->height,
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
    xcb_free_gc(c, dgc);
    xcb_image_destroy(img);
    return 0;
}
