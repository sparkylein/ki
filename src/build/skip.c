#include "../all.h"

void skip_body(Fc *fc, char until_ch) {
    //
    int depth = 0;
    if (until_ch == '}' || until_ch == ')')
        depth = 1;
    char *token = fc->token;
    while (true) {
        //
        tok(fc, token, false, true);

        if (token[0] == 0)
            break;

        char ch = token[0];

        if (ch == '"' || ch == '\'') {
            skip_string(fc, ch);
            continue;
        }

        if (ch == '/' && get_char(fc, 0) == '/') {
            skip_until_char(fc, '\n');
            continue;
        }

        if (ch == '{' || ch == '(') {
            depth++;
            continue;
        }
        if (ch == '}' || ch == ')') {
            depth--;
            if (depth == 0 && until_ch == ch)
                break;
            if (depth < 0)
                break;
            continue;
        }
    }
    if (depth != 0) {
        sprintf(fc->sbuf, "Unexpected end of code, missing bracket somewhere");
        fc_error(fc);
    }
}

void skip_string(Fc *fc, char end_char) {
    //
    Chunk *chunk = fc->chunk;
    char ch;
    int i = chunk->i;
    const char *content = chunk->content;
    while (chunk->i < chunk->length) {
        //
        ch = chunk->content[i];
        i++;

        if (ch == '\\') {
            i++;
            continue;
        }

        if (ch == end_char) {
            break;
        }
    }

    chunk->i = i;

    if (chunk->i == chunk->length) {
        sprintf(fc->sbuf, "Unexpected end of code, string not closed");
        fc_error(fc);
    }
}

void skip_until_char(Fc *fc, char find) {
    //
    Chunk *chunk = fc->chunk;
    char ch;
    int i = chunk->i;
    const char *content = chunk->content;
    while (chunk->i < chunk->length) {
        //
        ch = chunk->content[i];
        i++;
        if (ch == find) {
            break;
        }
    }
    chunk->i = i;
}

void skip_whitespace(Fc *fc) {
    //
    Chunk *chunk = fc->chunk;
    char ch;
    int i = chunk->i;
    const char *content = chunk->content;
    while (chunk->i < chunk->length) {
        //
        ch = chunk->content[i];
        if (!is_whitespace(ch)) {
            break;
        }
        i++;
    }
    chunk->i = i;
}

void skip_macro_if(Fc *fc) {
    //
    Chunk *chunk = fc->chunk;
    const int length = chunk->length;
    const char *content = chunk->content;
    char ch;
    char *token = fc->token;
    int depth = 1;
    while (chunk->i < length) {
        //
        char ch = content[chunk->i];
        chunk->i++;

        if (ch == '"' || ch == '\'') {
            skip_string(fc, ch);
            continue;
        }

        if (!is_newline(ch)) {
            continue;
        }

        tok(fc, token, false, true);

        if (strcmp(token, "#") != 0) {
            continue;
        }

        tok(fc, token, true, false);
        if (strcmp(token, "if") == 0) {
            depth++;
        } else if (strcmp(token, "elif") == 0 || strcmp(token, "else") == 0) {
            if (depth == 1) {
                depth--;
                chunk_move(chunk, -5);
                break;
            }
        } else if (strcmp(token, "end") == 0) {
            depth--;
            if (depth == 0) {
                chunk_move(chunk, -4);
                break;
            }
        }
    }

    if (depth != 0) {
        sprintf(fc->sbuf, "End of file, missing #end macro");
        fc_error(fc);
    }
}

void skip_traits(Fc *fc) {
    //
    char *token = fc->token;
    while (true) {
        tok(fc, token, false, true);
        if (is_valid_varname_char(token[0])) {
            rtok(fc);
            read_id(fc, false, true, true);
            continue;
        }
        if (token[0] != ',') {
            rtok(fc);
            break;
        }
    }
}