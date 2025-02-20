
#include "../all.h"

void cmd_fmt_help();
void fmt_indent(Fmt *fmt);

void cmd_fmt(int argc, char *argv[]) {

    Allocator *alc = alc_make();
    Str *str_buf = str_make(alc, 5000);
    char *char_buf = al(alc, 5000);

    Array *args = array_make(alc, argc);
    Map *options = map_make(alc);
    Array *has_value = array_make(alc, 8);

    parse_argv(argv, argc, has_value, args, options);

    if (array_contains(args, "-h", arr_find_str) || array_contains(args, "--help", arr_find_str)) {
        cmd_fmt_help();
    }

    if (args->length < 3) {
        cmd_fmt_help();
    }

    Array *files = array_make(alc, argc);
    int argc_ = args->length;
    for (int i = 2; i < argc_; i++) {
        char *arg = array_get_index(args, i);
        if (arg[0] == '-') {
            continue;
        }

        char *full = al(alc, KI_PATH_MAX);
        bool success = get_fullpath(arg, full);

        if (!success || !file_exists(full)) {
            sprintf(char_buf, "fmt: file not found: '%s'", arg);
            die(char_buf);
        }

        if (!ends_with(arg, ".ki")) {
            sprintf(char_buf, "fmt: filename must end with .ki : '%s'", arg);
            die(char_buf);
        }

        array_push(files, full);
    }

    if (files->length == 0) {
        sprintf(char_buf, "Nothing to format, add some files to your fmt command");
        die(char_buf);
    }

    int filec = files->length;
    for (int i = 0; i < filec; i++) {
        char *path = array_get_index(files, i);
        if (!file_exists(path))
            continue;
        Str *content = str_make(alc, 10000);
        file_get_contents(content, path);
        char *result = fmt_format(alc, str_to_chars(alc, content), false, 4);
        // printf("%s\n", result);
        write_file(path, result, false);
    }
}

char *fmt_format(Allocator *alc, char *data, bool use_tabs, int spaces) {
    //
    Str *content = str_make(alc, 10000);
    int len = strlen(data);

    Fmt *fmt = al(alc, sizeof(Fmt));
    fmt->content = content;
    fmt->depth = 0;
    fmt->spaces = spaces;
    fmt->use_tabs = use_tabs;

    int i = 0;
    int newlines = 0;
    int ctx = fmtc_root;
    Array *contexts = array_make(alc, 50);
    char ch = '\0';
    char pch = '\0';
    bool start_of_line = true;
    bool added_spacing = false;
    bool expects_newline = false;
    while (i < len) {
        pch = ch;
        ch = data[i];
        i++;
        // Newlines
        if (is_newline(ch)) {
            if (newlines < (fmt->depth == 0 ? 3 : 2)) {
                str_append_char(content, '\n');
                start_of_line = true;
            }
            newlines++;
            expects_newline = false;
            continue;
        }
        // Skip whitespace
        if (is_whitespace(ch)) {
            if (start_of_line) {
                continue;
            }
            if (!added_spacing) {
                str_append_char(content, ' ');
                added_spacing = true;
            }
            continue;
        }
        // Indent
        if (ch == '{' && data[i] != '{') {
            if (start_of_line) {
                fmt_indent(fmt);
            }
            fmt->depth++;
            str_append_chars(content, "{");
            start_of_line = true;
            expects_newline = true;
            continue;
        }
        if (ch == '}') {
            if (pch != '}') {
                if (fmt->depth == 0) {
                    return NULL;
                }
                fmt->depth--;
                if (!start_of_line) {
                    str_append_char(content, '\n');
                }
                fmt_indent(fmt);
            }
            str_append_chars(content, "}");
            if (data[i] != '{') {
                start_of_line = true;
                expects_newline = true;
            }
            continue;
        }
        //
        if (start_of_line && ch != ';') {
            if (expects_newline)
                str_append_char(content, '\n');
            fmt_indent(fmt);
        }
        newlines = 0;
        start_of_line = false;
        added_spacing = false;
        //
        str_append_char(content, ch);
        // String
        if (ch == '"') {
            while (i < len) {
                ch = data[i];
                i++;
                str_append_char(content, ch);
                if (ch == '\\') {
                    str_append_char(content, data[i]);
                    i++;
                    continue;
                }
                if (ch == '"') {
                    break;
                }
            }
            continue;
        }
        // Char
        if (ch == '\'') {
            while (i < len) {
                ch = data[i];
                i++;
                str_append_char(content, ch);
                if (ch == '\\') {
                    str_append_char(content, data[i]);
                    i++;
                    continue;
                }
                if (ch == '\'') {
                    break;
                }
            }
            continue;
        }
        // Comment
        if (ch == '/' && pch == '/') {
            str_append_char(content, ' ');
            while (i < len) {
                ch = data[i];
                if (!is_whitespace(ch) || is_newline(ch))
                    break;
                i++;
            }
            while (i < len) {
                ch = data[i];
                i++;
                str_append_char(content, ch);
                if (is_newline(ch)) {
                    start_of_line = true;
                    newlines = 1;
                    break;
                }
            }
            continue;
        }
        if (ch == ',') {
            str_append_char(content, ' ');
            added_spacing = true;
        }
    }

    if (fmt->depth > 0) {
        return NULL;
    }

    char *result = str_to_chars(alc, content);
    return result;
}

void fmt_indent(Fmt *fmt) {
    for (int i = 0; i < fmt->depth; i++) {
        if (fmt->use_tabs) {
            str_append_chars(fmt->content, "\t");
        } else {
            for (int o = 0; o < fmt->spaces; o++) {
                str_append_chars(fmt->content, " ");
            }
        }
    }
}

void cmd_fmt_help() {
    //
    printf("> ki fmt {ki file paths}\n");
    printf("\n");

    exit(1);
}
