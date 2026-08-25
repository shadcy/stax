#ifndef BROWSER_HTML_H
#define BROWSER_HTML_H

#include <stdint.h>

#define HTML_ST_TEXT  0
#define HTML_ST_H1    1
#define HTML_ST_H2    2
#define HTML_ST_H3    3
#define HTML_ST_LINK  4
#define HTML_ST_PRE   5
#define HTML_ST_LI    6
#define HTML_ST_QUOTE 7

typedef struct {
    uint32_t off;
    uint16_t n;
    uint8_t style;
    uint8_t _pad;
    uint16_t url_id;
    int32_t y;
    int16_t x;
} html_span_t;

void html_doc_reset(void);
void html_tokenizer_reset(void);
void html_set_base(const char *host, const char *path, unsigned port);
void html_feed(unsigned char b);
void html_emit_text(const char *s);

uint32_t html_text_len(void);
int html_truncated(void);
const char *html_title(void);
const char *html_text_ptr(void);

uint16_t html_intern_url(const char *url);
const char *html_url(uint16_t id);
void html_resolve(const char *href, char *out, int out_max);

void html_layout(int wrap_w);
int html_layout_height(void);
int html_span_count(void);
const html_span_t *html_span_at(int i);

#endif
