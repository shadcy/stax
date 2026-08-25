#include "browser_html.h"
#include "string.h"

#define TEXT_CAP      (96 * 1024)
#define URL_ARENA_CAP (24 * 1024)
#define MAX_RUNS      4096
#define MAX_SPANS     8192
#define MAX_URLS      192
#define TAG_NAME_MAX  24
#define ATTR_VAL_MAX  256
#define URL_MAX       256

typedef struct {
    uint32_t start;
    uint16_t len;
    uint8_t style;
    uint8_t _pad;
    uint16_t url_id;
} run_t;

static char text_arena[TEXT_CAP];
static uint32_t text_len;
static int text_truncated;
static run_t runs[MAX_RUNS];
static int run_count;

static char url_arena[URL_ARENA_CAP];
static uint32_t url_arena_len;
static uint32_t url_off[MAX_URLS + 1];
static uint16_t url_slen[MAX_URLS + 1];
static int url_count;

static html_span_t spans[MAX_SPANS];
static int span_count;
static int layout_w = -1;
static uint32_t layout_off;
static int layout_x, layout_y, layout_height;
static int lay_run;
static uint32_t pend_off;
static uint16_t pend_n, pend_url;
static uint8_t pend_st;
static int pend_x, pend_y;
static int layout_full;

static uint8_t cur_style;
static uint16_t cur_url_id;
static int pre_depth, quote_depth;
static int last_space, nl_run;
static int in_head;          /* discard head chrome (keeps title via HS_TITLE) */
static int seen_content;     /* suppress leading blank lines before first glyph */

static char base_host[128];
static char base_path[192];
static unsigned base_port = 80;

static char title_buf[96];
static int title_len;

enum {
    HS_DATA, HS_ENTITY, HS_TAG_OPEN, HS_TAG_NAME, HS_BEFORE_ATTR,
    HS_ATTR_NAME, HS_AFTER_ATTR_NAME, HS_BEFORE_VALUE,
    HS_VALUE_DQ, HS_VALUE_SQ, HS_VALUE_UQ, HS_SELF_SLASH,
    HS_COMMENT, HS_COMMENT_DASH, HS_COMMENT_END,
    HS_BANG, HS_BOGUS, HS_RAW, HS_RAW_LT, HS_RAW_END,
    HS_TITLE, HS_TITLE_LT
};

static int hs;
static int is_end_tag;
static char tag_name[TAG_NAME_MAX];
static int tag_name_len;
static char attr_name[TAG_NAME_MAX];
static int attr_name_len;
static char attr_val[ATTR_VAL_MAX];
static int attr_val_len;
static char href_buf[ATTR_VAL_MAX];
static char alt_buf[ATTR_VAL_MAX];
static char entity_buf[16];
static int entity_len;
static char raw_end[TAG_NAME_MAX];
static char raw_collect[TAG_NAME_MAX];
static int raw_collect_len;
static uint32_t utf8_cp;
static int utf8_need;
static int bang_dash;

static int str_ieq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int str_istart(const char *s, const char *pfx) {
    while (*pfx) {
        char ca = *s, cb = *pfx;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (!ca || ca != cb) return 0;
        s++; pfx++;
    }
    return 1;
}

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_alnum(char c) {
    return is_alpha(c) || (c >= '0' && c <= '9');
}

static void copy_str(char *dst, const char *src, int max) {
    int i = 0;
    if (max <= 0) return;
    while (src && src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

uint32_t html_text_len(void) { return text_len; }
int html_truncated(void) { return text_truncated; }
const char *html_title(void) { return title_buf; }
const char *html_text_ptr(void) { return text_arena; }
int html_layout_height(void) { return layout_height; }
int html_span_count(void) { return span_count; }

const html_span_t *html_span_at(int i) {
    if (i < 0 || i >= span_count) return 0;
    return &spans[i];
}

const char *html_url(uint16_t id) {
    if (id == 0 || id > (uint16_t)url_count) return "";
    return url_arena + url_off[id];
}

uint16_t html_intern_url(const char *url) {
    int i, n;
    if (!url || !url[0]) return 0;
    n = (int)strlen(url);
    for (i = 1; i <= url_count; i++) {
        if (url_slen[i] == (uint16_t)n &&
            memcmp(url_arena + url_off[i], url, (size_t)n) == 0)
            return (uint16_t)i;
    }
    if (url_count >= MAX_URLS) return 0;
    if (url_arena_len + (uint32_t)n + 1 > URL_ARENA_CAP) return 0;
    url_count++;
    url_off[url_count] = url_arena_len;
    url_slen[url_count] = (uint16_t)n;
    memcpy(url_arena + url_arena_len, url, (size_t)n);
    url_arena_len += (uint32_t)n;
    url_arena[url_arena_len++] = '\0';
    return (uint16_t)url_count;
}

void html_set_base(const char *host, const char *path, unsigned port) {
    copy_str(base_host, host ? host : "", (int)sizeof(base_host));
    copy_str(base_path, (path && path[0]) ? path : "/", (int)sizeof(base_path));
    base_port = port ? port : 80;
}

void html_resolve(const char *href, char *out, int out_max) {
    int i = 0, j, slash;
    if (!out || out_max < 8) return;
    out[0] = '\0';
    if (!href || !href[0]) return;
    if (str_istart(href, "javascript:") || str_istart(href, "mailto:") ||
        str_istart(href, "data:") || href[0] == '#')
        return;
    if (str_istart(href, "http://") || str_istart(href, "https://")) {
        copy_str(out, href, out_max);
        return;
    }
    if (href[0] == '/' && href[1] == '/') {
        copy_str(out, "http:", out_max);
        i = (int)strlen(out);
        while (*href && i < out_max - 1) out[i++] = *href++;
        out[i] = '\0';
        return;
    }
    {
        const char *pre = "http://";
        while (*pre && i < out_max - 1) out[i++] = *pre++;
    }
    for (j = 0; base_host[j] && i < out_max - 1; j++) out[i++] = base_host[j];
    if (base_port != 80) {
        unsigned p = base_port;
        char d[6];
        int di = 0;
        if (i < out_max - 1) out[i++] = ':';
        do { d[di++] = (char)('0' + (p % 10)); p /= 10; } while (p && di < 6);
        while (di > 0 && i < out_max - 1) out[i++] = d[--di];
    }
    if (href[0] == '/') {
        while (*href && i < out_max - 1) out[i++] = *href++;
        out[i] = '\0';
        return;
    }
    slash = 0;
    for (j = 0; base_path[j]; j++) if (base_path[j] == '/') slash = j;
    for (j = 0; j <= slash && base_path[j] && i < out_max - 1; j++)
        out[i++] = base_path[j];
    while (*href && i < out_max - 1) out[i++] = *href++;
    out[i] = '\0';
}

static void run_putc(char c) {
    run_t *r;
    if (text_len >= TEXT_CAP - 1) {
        text_truncated = 1;
        return;
    }
    if (run_count > 0) {
        r = &runs[run_count - 1];
        if (r->style == cur_style && r->url_id == cur_url_id &&
            r->start + r->len == text_len && r->len < 0xFFF0) {
            text_arena[text_len++] = c;
            r->len++;
            return;
        }
    }
    if (run_count >= MAX_RUNS) {
        text_truncated = 1;
        return;
    }
    r = &runs[run_count++];
    r->start = text_len;
    r->len = 1;
    r->style = cur_style;
    r->url_id = cur_url_id;
    r->_pad = 0;
    text_arena[text_len++] = c;
}

static void emit_char(char c) {
    if (text_truncated) return;
    if (in_head) return; /* title uses HS_TITLE; scripts/styles use RAW */
    if (c == '\r') return;
    if (c == '\t') c = ' ';

    /* Drop blank lines/spaces before the first visible character so the
     * first screenful isn't an empty white page under a tall <head>. */
    if (!seen_content) {
        if (c == ' ' || c == '\n') return;
        seen_content = 1;
    }

    if (c == '\n') {
        if (!pre_depth && nl_run >= 2) return;
        nl_run++;
        last_space = 1;
        run_putc('\n');
        return;
    }
    nl_run = 0;
    if (c == ' ' && !pre_depth) {
        if (last_space) return;
        last_space = 1;
        run_putc(' ');
        return;
    }
    last_space = (c == ' ');
    run_putc(c);
}

void html_emit_text(const char *s) {
    while (s && *s) emit_char(*s++);
}

static void emit_cp(uint32_t cp) {
    char a = '?';
    if (cp == 0 || cp == 0xFEFF) return;
    if (cp == 0xA0 || cp == 0x2007 || cp == 0x202F) { emit_char(' '); return; }
    if (cp < 128) { emit_char((char)cp); return; }
    if (cp >= 0xC0 && cp <= 0xC6) a = 'A';
    else if (cp == 0xC7) a = 'C';
    else if (cp >= 0xC8 && cp <= 0xCB) a = 'E';
    else if (cp >= 0xCC && cp <= 0xCF) a = 'I';
    else if (cp == 0xD1) a = 'N';
    else if (cp >= 0xD2 && cp <= 0xD8) a = 'O';
    else if (cp >= 0xD9 && cp <= 0xDC) a = 'U';
    else if (cp >= 0xE0 && cp <= 0xE6) a = 'a';
    else if (cp == 0xE7) a = 'c';
    else if (cp >= 0xE8 && cp <= 0xEB) a = 'e';
    else if (cp >= 0xEC && cp <= 0xEF) a = 'i';
    else if (cp == 0xF1) a = 'n';
    else if (cp >= 0xF2 && cp <= 0xF8) a = 'o';
    else if (cp >= 0xF9 && cp <= 0xFC) a = 'u';
    else if (cp == 0x2013 || cp == 0x2014) a = '-';
    else if (cp == 0x2018 || cp == 0x2019) a = '\'';
    else if (cp == 0x201C || cp == 0x201D) a = '"';
    else if (cp == 0x2026) { html_emit_text("..."); return; }
    else if (cp == 0x2022) a = '*';
    emit_char(a);
}

static void emit_utf8(unsigned char b) {
    if (utf8_need) {
        if ((b & 0xC0) != 0x80) {
            utf8_need = 0;
            emit_char('?');
            emit_utf8(b);
            return;
        }
        utf8_cp = (utf8_cp << 6) | (uint32_t)(b & 0x3F);
        utf8_need--;
        if (!utf8_need) emit_cp(utf8_cp);
        return;
    }
    if (b < 0x80) emit_cp(b);
    else if ((b & 0xE0) == 0xC0) { utf8_need = 1; utf8_cp = (uint32_t)(b & 0x1F); }
    else if ((b & 0xF0) == 0xE0) { utf8_need = 2; utf8_cp = (uint32_t)(b & 0x0F); }
    else if ((b & 0xF8) == 0xF0) { utf8_need = 3; utf8_cp = (uint32_t)(b & 0x07); }
    else emit_char('?');
}

static int named_entity(const char *n, uint32_t *cp) {
    if (str_ieq(n, "amp")) { *cp = '&'; return 1; }
    if (str_ieq(n, "lt")) { *cp = '<'; return 1; }
    if (str_ieq(n, "gt")) { *cp = '>'; return 1; }
    if (str_ieq(n, "quot")) { *cp = '"'; return 1; }
    if (str_ieq(n, "apos")) { *cp = '\''; return 1; }
    if (str_ieq(n, "nbsp")) { *cp = 0xA0; return 1; }
    if (str_ieq(n, "copy")) { html_emit_text("(C)"); *cp = 0; return 1; }
    if (str_ieq(n, "ndash") || str_ieq(n, "mdash")) { *cp = '-'; return 1; }
    if (str_ieq(n, "hellip")) { html_emit_text("..."); *cp = 0; return 1; }
    if (str_ieq(n, "bull")) { *cp = '*'; return 1; }
    if (str_ieq(n, "rsquo") || str_ieq(n, "lsquo")) { *cp = '\''; return 1; }
    if (str_ieq(n, "rdquo") || str_ieq(n, "ldquo")) { *cp = '"'; return 1; }
    return 0;
}

static void flush_entity(int semi) {
    uint32_t cp = 0;
    if (entity_len <= 0) { emit_char('&'); return; }
    entity_buf[entity_len] = '\0';
    if (entity_buf[0] == '#') {
        int i = 1, hex = 0;
        if (entity_buf[1] == 'x' || entity_buf[1] == 'X') { hex = 1; i = 2; }
        while (entity_buf[i]) {
            int v = hex ? hex_digit(entity_buf[i])
                        : (entity_buf[i] >= '0' && entity_buf[i] <= '9' ? entity_buf[i] - '0' : -1);
            if (v < 0) break;
            cp = hex ? (cp * 16u + (uint32_t)v) : (cp * 10u + (uint32_t)v);
            i++;
        }
        if (cp) emit_cp(cp);
        else { emit_char('&'); html_emit_text(entity_buf); }
        return;
    }
    if (named_entity(entity_buf, &cp)) emit_cp(cp);
    else {
        emit_char('&');
        html_emit_text(entity_buf);
        if (semi) emit_char(';');
    }
}

static int is_void_tag(const char *t) {
    return str_ieq(t, "br") || str_ieq(t, "hr") || str_ieq(t, "img") ||
           str_ieq(t, "meta") || str_ieq(t, "link") || str_ieq(t, "input") ||
           str_ieq(t, "area") || str_ieq(t, "base") || str_ieq(t, "col") ||
           str_ieq(t, "embed") || str_ieq(t, "source") || str_ieq(t, "wbr");
}

static int is_raw_tag(const char *t) {
    return str_ieq(t, "script") || str_ieq(t, "style") || str_ieq(t, "noscript") ||
           str_ieq(t, "svg") || str_ieq(t, "iframe") || str_ieq(t, "template");
}

static void capture_attr(void) {
    attr_name[attr_name_len] = '\0';
    attr_val[attr_val_len] = '\0';
    if (str_ieq(attr_name, "href")) copy_str(href_buf, attr_val, ATTR_VAL_MAX);
    else if (str_ieq(attr_name, "src") && !href_buf[0]) copy_str(href_buf, attr_val, ATTR_VAL_MAX);
    else if (str_ieq(attr_name, "alt")) copy_str(alt_buf, attr_val, ATTR_VAL_MAX);
}

static void on_tag(void) {
    char resolved[URL_MAX];
    tag_name[tag_name_len] = '\0';
    if (attr_name_len > 0) capture_attr();

    if (is_end_tag) {
        if (str_ieq(tag_name, "head")) {
            in_head = 0;
            return;
        }
        if (str_ieq(tag_name, "a")) {
            cur_style = pre_depth ? HTML_ST_PRE : (quote_depth ? HTML_ST_QUOTE : HTML_ST_TEXT);
            cur_url_id = 0;
        } else if (tag_name[0] == 'h' && tag_name[1] >= '1' && tag_name[1] <= '6' && !tag_name[2]) {
            cur_style = HTML_ST_TEXT;
            emit_char('\n'); emit_char('\n');
        } else if (str_ieq(tag_name, "p") || str_ieq(tag_name, "div") ||
                   str_ieq(tag_name, "section") || str_ieq(tag_name, "article") ||
                   str_ieq(tag_name, "header") || str_ieq(tag_name, "footer") ||
                   str_ieq(tag_name, "nav") || str_ieq(tag_name, "main") ||
                   str_ieq(tag_name, "tr") || str_ieq(tag_name, "table") ||
                   str_ieq(tag_name, "ul") || str_ieq(tag_name, "ol") ||
                   str_ieq(tag_name, "form")) {
            emit_char('\n');
            if (str_ieq(tag_name, "p")) emit_char('\n');
        } else if (str_ieq(tag_name, "li")) {
            emit_char('\n');
            cur_style = HTML_ST_TEXT;
        } else if (str_ieq(tag_name, "pre") || str_ieq(tag_name, "textarea")) {
            if (pre_depth > 0) pre_depth--;
            if (!pre_depth) cur_style = HTML_ST_TEXT;
            emit_char('\n');
        } else if (str_ieq(tag_name, "blockquote")) {
            if (quote_depth > 0) quote_depth--;
            cur_style = HTML_ST_TEXT;
            emit_char('\n');
        } else if (str_ieq(tag_name, "td") || str_ieq(tag_name, "th")) {
            html_emit_text("  ");
        }
        return;
    }

    if (str_ieq(tag_name, "head")) { in_head = 1; return; }
    if (str_ieq(tag_name, "body")) { in_head = 0; return; }

    if (str_ieq(tag_name, "br")) { emit_char('\n'); return; }
    if (str_ieq(tag_name, "hr")) {
        emit_char('\n');
        html_emit_text("----------------------------------------");
        emit_char('\n');
        return;
    }
    if (str_ieq(tag_name, "img")) {
        emit_char(' ');
        emit_char('[');
        html_emit_text(alt_buf[0] ? alt_buf : "image");
        emit_char(']');
        emit_char(' ');
        return;
    }
    if (is_void_tag(tag_name)) return;

    if (str_ieq(tag_name, "h1")) { emit_char('\n'); emit_char('\n'); cur_style = HTML_ST_H1; }
    else if (str_ieq(tag_name, "h2")) { emit_char('\n'); emit_char('\n'); cur_style = HTML_ST_H2; }
    else if (tag_name[0] == 'h' && tag_name[1] >= '3' && tag_name[1] <= '6' && !tag_name[2]) {
        emit_char('\n'); cur_style = HTML_ST_H3;
    } else if (str_ieq(tag_name, "p")) {
        emit_char('\n'); emit_char('\n');
    } else if (str_ieq(tag_name, "div") || str_ieq(tag_name, "section") ||
               str_ieq(tag_name, "article") || str_ieq(tag_name, "header") ||
               str_ieq(tag_name, "footer") || str_ieq(tag_name, "nav") ||
               str_ieq(tag_name, "main") || str_ieq(tag_name, "center") ||
               str_ieq(tag_name, "figure") || str_ieq(tag_name, "address")) {
        emit_char('\n');
    } else if (str_ieq(tag_name, "li")) {
        emit_char('\n');
        cur_style = HTML_ST_LI;
        html_emit_text(" * ");
    } else if (str_ieq(tag_name, "ul") || str_ieq(tag_name, "ol") || str_ieq(tag_name, "tr")) {
        emit_char('\n');
    } else if (str_ieq(tag_name, "td") || str_ieq(tag_name, "th")) {
        html_emit_text(" | ");
    } else if (str_ieq(tag_name, "pre") || str_ieq(tag_name, "textarea")) {
        emit_char('\n');
        pre_depth++;
        cur_style = HTML_ST_PRE;
    } else if (str_ieq(tag_name, "blockquote")) {
        emit_char('\n');
        quote_depth++;
        cur_style = HTML_ST_QUOTE;
        html_emit_text("> ");
    } else if (str_ieq(tag_name, "a") && href_buf[0]) {
        html_resolve(href_buf, resolved, URL_MAX);
        cur_url_id = html_intern_url(resolved);
        if (cur_url_id) cur_style = HTML_ST_LINK;
    }
}

static void begin_raw(void) {
    int i = 0;
    hs = HS_RAW;
    while (tag_name[i] && i < TAG_NAME_MAX - 1) {
        raw_end[i] = tag_name[i];
        i++;
    }
    raw_end[i] = '\0';
}

static void finish_start_tag(void) {
    on_tag();
    if (!is_end_tag && str_ieq(tag_name, "title")) {
        title_len = 0;
        hs = HS_TITLE;
        return;
    }
    if (!is_end_tag && is_raw_tag(tag_name)) begin_raw();
    else hs = HS_DATA;
}

void html_feed(unsigned char b) {
    char c = (char)b;

replay:
    switch (hs) {
    case HS_DATA:
        if (c == '&') { hs = HS_ENTITY; entity_len = 0; }
        else if (c == '<') hs = HS_TAG_OPEN;
        else emit_utf8(b);
        break;

    case HS_ENTITY:
        if (c == ';') { flush_entity(1); hs = HS_DATA; }
        else if (is_alnum(c) || c == '#' || (entity_len == 1 && (c == 'x' || c == 'X'))) {
            if (entity_len < 14) entity_buf[entity_len++] = c;
        } else {
            flush_entity(0);
            hs = HS_DATA;
            goto replay;
        }
        break;

    case HS_TAG_OPEN:
        if (c == '!') { hs = HS_BANG; bang_dash = 0; }
        else if (c == '/') {
            is_end_tag = 1; tag_name_len = 0; href_buf[0] = 0; alt_buf[0] = 0;
            attr_name_len = 0; attr_val_len = 0; hs = HS_TAG_NAME;
        } else if (is_alpha(c)) {
            is_end_tag = 0; tag_name_len = 0; href_buf[0] = 0; alt_buf[0] = 0;
            attr_name_len = 0; attr_val_len = 0; hs = HS_TAG_NAME;
            goto replay;
        } else if (c == '?') hs = HS_BOGUS;
        else { emit_char('<'); hs = HS_DATA; goto replay; }
        break;

    case HS_BANG:
        if (c == '>') hs = HS_DATA;
        else if (c == '-' && bang_dash == 0) bang_dash = 1;
        else if (c == '-' && bang_dash == 1) { hs = HS_COMMENT; bang_dash = 0; }
        else bang_dash = 2;
        break;

    case HS_COMMENT:
        if (c == '-') hs = HS_COMMENT_DASH;
        break;
    case HS_COMMENT_DASH:
        hs = (c == '-') ? HS_COMMENT_END : HS_COMMENT;
        break;
    case HS_COMMENT_END:
        if (c == '>') hs = HS_DATA;
        else if (c != '-') hs = HS_COMMENT;
        break;
    case HS_BOGUS:
        if (c == '>') hs = HS_DATA;
        break;

    case HS_TAG_NAME:
        if (is_alnum(c) || c == '-') {
            if (tag_name_len < TAG_NAME_MAX - 1)
                tag_name[tag_name_len++] = ascii_lower(c);
        } else if (c == '>') {
            finish_start_tag();
        } else if (c == '/') {
            hs = HS_SELF_SLASH;
        } else {
            hs = HS_BEFORE_ATTR;
            if (c != ' ' && c != '\n' && c != '\t' && c != '\r') goto replay;
        }
        break;

    case HS_SELF_SLASH:
        if (c == '>') finish_start_tag();
        else { hs = HS_BEFORE_ATTR; goto replay; }
        break;

    case HS_BEFORE_ATTR:
        if (c == '>') finish_start_tag();
        else if (c == '/') hs = HS_SELF_SLASH;
        else if (c != ' ' && c != '\n' && c != '\t' && c != '\r') {
            attr_name_len = 0; attr_val_len = 0; hs = HS_ATTR_NAME; goto replay;
        }
        break;

    case HS_ATTR_NAME:
        if (is_alnum(c) || c == '-' || c == '_') {
            if (attr_name_len < TAG_NAME_MAX - 1)
                attr_name[attr_name_len++] = ascii_lower(c);
        } else if (c == '=') hs = HS_BEFORE_VALUE;
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') hs = HS_AFTER_ATTR_NAME;
        else { capture_attr(); attr_name_len = 0; hs = HS_BEFORE_ATTR; goto replay; }
        break;

    case HS_AFTER_ATTR_NAME:
        if (c == '=') hs = HS_BEFORE_VALUE;
        else if (c != ' ' && c != '\t') {
            capture_attr(); attr_name_len = 0; hs = HS_BEFORE_ATTR; goto replay;
        }
        break;

    case HS_BEFORE_VALUE:
        if (c == '"') { attr_val_len = 0; hs = HS_VALUE_DQ; }
        else if (c == '\'') { attr_val_len = 0; hs = HS_VALUE_SQ; }
        else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            attr_val_len = 0; hs = HS_VALUE_UQ; goto replay;
        }
        break;

    case HS_VALUE_DQ:
        if (c == '"') { capture_attr(); attr_name_len = 0; hs = HS_BEFORE_ATTR; }
        else if (attr_val_len < ATTR_VAL_MAX - 1) attr_val[attr_val_len++] = c;
        break;
    case HS_VALUE_SQ:
        if (c == '\'') { capture_attr(); attr_name_len = 0; hs = HS_BEFORE_ATTR; }
        else if (attr_val_len < ATTR_VAL_MAX - 1) attr_val[attr_val_len++] = c;
        break;
    case HS_VALUE_UQ:
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/') {
            capture_attr(); attr_name_len = 0; hs = HS_BEFORE_ATTR; goto replay;
        } else if (attr_val_len < ATTR_VAL_MAX - 1) attr_val[attr_val_len++] = c;
        break;

    case HS_RAW:
        if (c == '<') hs = HS_RAW_LT;
        break;
    case HS_RAW_LT:
        if (c == '/') { raw_collect_len = 0; hs = HS_RAW_END; }
        else hs = HS_RAW;
        break;
    case HS_RAW_END:
        if (is_alnum(c) || c == '-') {
            if (raw_collect_len < TAG_NAME_MAX - 1)
                raw_collect[raw_collect_len++] = ascii_lower(c);
        } else {
            raw_collect[raw_collect_len] = '\0';
            if (str_ieq(raw_collect, raw_end)) {
                hs = (c == '>') ? HS_DATA : HS_BOGUS;
            } else {
                hs = HS_RAW;
            }
        }
        break;

    case HS_TITLE:
        if (c == '<') hs = HS_TITLE_LT;
        else if (c != '\n' && c != '\r' && title_len < (int)sizeof(title_buf) - 1)
            title_buf[title_len++] = c;
        break;
    case HS_TITLE_LT:
        if (c == '/') {
            title_buf[title_len] = '\0';
            is_end_tag = 1;
            tag_name_len = 0;
            hs = HS_TAG_NAME;
        } else {
            if (title_len < (int)sizeof(title_buf) - 1) title_buf[title_len++] = '<';
            hs = HS_TITLE;
            goto replay;
        }
        break;
    }
}

static void span_flush(void) {
    html_span_t *s;
    if (!pend_n || layout_full) { pend_n = 0; return; }
    if (span_count >= MAX_SPANS) { layout_full = 1; pend_n = 0; return; }
    s = &spans[span_count++];
    s->off = pend_off;
    s->n = pend_n;
    s->style = pend_st;
    s->url_id = pend_url;
    s->y = pend_y;
    s->x = (int16_t)pend_x;
    s->_pad = 0;
    pend_n = 0;
}

static void layout_nl(void) {
    span_flush();
    layout_x = 4;
    layout_y += 16;
}

void html_layout(int wrap_w) {
    int margin = 8;
    if (wrap_w < 48) wrap_w = 48;
    if (wrap_w != layout_w) {
        layout_w = wrap_w;
        span_count = 0;
        layout_off = 0;
        layout_x = 4;
        layout_y = 0;
        lay_run = 0;
        pend_n = 0;
        layout_full = 0;
    }
    while (layout_off < text_len && !layout_full) {
        char ch = text_arena[layout_off];
        uint8_t st;
        uint16_t uid;
        while (lay_run + 1 < run_count &&
               layout_off >= runs[lay_run].start + runs[lay_run].len)
            lay_run++;
        st = (lay_run < run_count) ? runs[lay_run].style : HTML_ST_TEXT;
        uid = (lay_run < run_count) ? runs[lay_run].url_id : 0;

        if (ch == '\n') {
            layout_nl();
            layout_off++;
            continue;
        }
        if (layout_x + 8 > wrap_w - margin) {
            layout_nl();
            if (ch == ' ') { layout_off++; continue; }
        }
        if (pend_n == 0) {
            pend_off = layout_off; pend_st = st; pend_url = uid;
            pend_x = layout_x; pend_y = layout_y; pend_n = 1;
        } else if (pend_st != st || pend_url != uid || pend_y != layout_y) {
            span_flush();
            pend_off = layout_off; pend_st = st; pend_url = uid;
            pend_x = layout_x; pend_y = layout_y; pend_n = 1;
        } else {
            pend_n++;
        }
        layout_x += 8;
        layout_off++;
    }
    span_flush();
    layout_height = layout_y + 16;
}

void html_doc_reset(void) {
    text_len = 0;
    text_truncated = 0;
    run_count = 0;
    url_arena_len = 0;
    url_count = 0;
    cur_style = HTML_ST_TEXT;
    cur_url_id = 0;
    pre_depth = quote_depth = 0;
    last_space = 1;
    nl_run = 0;
    in_head = 0;
    seen_content = 0;
    title_len = 0;
    title_buf[0] = '\0';
    utf8_need = 0;
    layout_w = -1;
    span_count = 0;
    layout_off = 0;
    layout_x = 4;
    layout_y = 0;
    layout_height = 16;
    lay_run = 0;
    pend_n = 0;
    layout_full = 0;
}

void html_tokenizer_reset(void) {
    hs = HS_DATA;
    is_end_tag = 0;
    tag_name_len = attr_name_len = attr_val_len = 0;
    href_buf[0] = alt_buf[0] = '\0';
    entity_len = 0;
    utf8_need = 0;
    bang_dash = 0;
}
