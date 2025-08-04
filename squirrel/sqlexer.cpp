#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <squirrel.h>

#include "sqlexer.h"

extern "C" {

extern "C" uint16_t lexer_string_to_token(char const * s, size_t len);
extern "C" void lexer_error(SQLexer * lexer, char const * err);
extern "C" void lexer_next(SQLexer * lexer);

}

void lexer_add_utf8(SQLexer * lexer, uint32_t ch) {
    if (ch < 0x80) {
        strbuf_push_back(&lexer->state.string_buffer, char(ch));
    } else if (ch < 0x800) {
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch >> 6) | 0xC0));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch & 0x3F) | 0x80));
    } else if (ch < 0x10000) {
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch >> 12) | 0xE0));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)(((ch >> 6) & 0x3F) | 0x80));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch & 0x3F) | 0x80));
    } else if (ch < 0x110000) {
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch >> 18) | 0xF0));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)(((ch >> 12) & 0x3F) | 0x80));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)(((ch >> 6) & 0x3F) | 0x80));
        strbuf_push_back(&lexer->state.string_buffer, (SQChar)((ch & 0x3F) | 0x80));
    }
}

inline static bool is_digit(uint8_t v) {
    return v >= '0' && v <= '9';
}

inline static bool is_hex_digit(uint8_t v) {
    if (is_digit(v)) {
        return true;
    }

    if (v >= 'a' && v <= 'f') {
        return true;
    }

    if (v >= 'A' && v <= 'F') {
        return true;
    }

    return false;
}

void lexer_process_string_hex_escape(SQLexer * lexer, char * dest, SQInteger maxdigits) {
    lexer_next(lexer);

    if (!is_hex_digit(lexer->_currdata)) {
        lexer_error(lexer, "hexadecimal number expected");
        // noreturn
    }

    SQInteger n = 0;
    while (is_hex_digit(lexer->_currdata) && n < maxdigits) {
        dest[n] = lexer->_currdata;
        n++;
        lexer_next(lexer);
    }
    dest[n] = 0;
}

SQInteger lexer_read_string(SQLexer * lexer, SQInteger ndelim, bool verbatim) {
    strbuf_reset(&lexer->state.string_buffer);
    lexer_next(lexer);

    if (lexer->_currdata <= SQUIRREL_EOB) {
        return -1;
    }

    for(;;) {
        while(lexer->_currdata != ndelim) {
            SQInteger x = lexer->_currdata;
            switch (x) {
            case SQUIRREL_EOB:
                lexer_error(lexer, "unfinished string");
                // noreturn
                return -1;
            case '\n':
                if(!verbatim) {
                    lexer_error(lexer, "newline in a constant");
                    // noreturn
                }
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
                lexer->state.current_line++;
                break;
            case '\\':
                if(verbatim) {
                    strbuf_push_back(&lexer->state.string_buffer, '\\');
                    lexer_next(lexer);
                }
                else {
                    lexer_next(lexer);
                    switch(lexer->_currdata) {
                    case 'x':  {
                        const SQInteger maxdigits = sizeof(SQChar) * 2;
                        SQChar temp[maxdigits + 1];
                        lexer_process_string_hex_escape(lexer, temp, maxdigits);
                        strbuf_push_back(&lexer->state.string_buffer, (SQChar)strtoul(temp, nullptr, 16));
                    }
                    break;
                    case 'U':
                    case 'u':  {
                        const SQInteger maxdigits = lexer->_currdata == 'u' ? 4 : 8;
                        SQChar temp[8 + 1];
                        lexer_process_string_hex_escape(lexer, temp, maxdigits);
                        strbuf_push_back_utf8(&lexer->state.string_buffer, strtoul(temp, nullptr, 16));
                    }
                    break;
                    case 't':
                        strbuf_push_back(&lexer->state.string_buffer, '\t');
                        lexer_next(lexer);
                        break;
                    case 'a':
                        strbuf_push_back(&lexer->state.string_buffer, '\a');
                        lexer_next(lexer);
                        break;
                    case 'b':
                        strbuf_push_back(&lexer->state.string_buffer, '\b');
                        lexer_next(lexer);
                        break;
                    case 'n':
                        strbuf_push_back(&lexer->state.string_buffer, '\n');
                        lexer_next(lexer);
                        break;
                    case 'r':
                        strbuf_push_back(&lexer->state.string_buffer, '\r');
                        lexer_next(lexer);
                        break;
                    case 'v':
                        strbuf_push_back(&lexer->state.string_buffer, '\v');
                        lexer_next(lexer);
                        break;
                    case 'f':
                        strbuf_push_back(&lexer->state.string_buffer, '\f');
                        lexer_next(lexer);
                        break;
                    case '0':
                        strbuf_push_back(&lexer->state.string_buffer, '\0');
                        lexer_next(lexer);
                        break;
                    case '\\':
                        strbuf_push_back(&lexer->state.string_buffer, '\\');
                        lexer_next(lexer);
                        break;
                    case '"':
                        strbuf_push_back(&lexer->state.string_buffer, '"');
                        lexer_next(lexer);
                        break;
                    case '\'':
                        strbuf_push_back(&lexer->state.string_buffer, '\'');
                        lexer_next(lexer);
                        break;
                    default:
                        lexer_error(lexer, "unrecognised escaper char");
                        break;
                    }
                }
                break;
            default:
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
                break;
            }
        }
        lexer_next(lexer);
        if(verbatim && lexer->_currdata == '"') { //double quotation
            strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
            lexer_next(lexer);
        }
        else {
            break;
        }
    }
    strbuf_push_back(&lexer->state.string_buffer, '\0');
    SQInteger len = lexer->state.string_buffer.len-1;

    if (ndelim == '\'') {
        if (len == 0) {
            lexer_error(lexer, "empty constant");
            // noreturn
        }
        if (len > 1) {
            lexer_error(lexer, "constant too long");
            // noreturn
        }

        // sign-extend the value!
        uint8_t const value = lexer->state.string_buffer.data[0];
        lexer->state.uint_value = uint64_t(int64_t(int8_t(value)));

        return TK_INTEGER;
    }
    lexer->state.string_value = reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]);
    return TK_STRING_LITERAL;
}

static void lex_integer(uint8_t const * s, uint64_t * res) {
    *res = 0;

    while (*s != 0) {
        *res = (*res) * 10 + (*s - '0');
        s++;
    }
}

void lex_integer_hex(uint8_t const * s, uint64_t * res) {
    *res = 0;

    while (*s) {
        if (is_digit(*s)) {
            *res = *res * 16 + (*s - '0');
            s++;
        } else if (is_hex_digit(*s)) {
            *res = *res * 16 + (toupper(*s) - 'A' + 10);
            s++;
        } else {
            assert(0);
        }
    }
}

SQInteger scisodigit(SQInteger c) {
    return c >= '0' && c <= '7';
}

void LexOctal(const SQChar *s,SQUnsignedInteger *res) {
    *res = 0;
    while(*s != 0) {
        if(scisodigit(*s)) *res = (*res)*8+((*s++)-'0');
        else { assert(0); }
    }
}

SQInteger isexponent(SQInteger c) {
    return c == 'e' || c=='E';
}

uint16_t lexer_read_number(SQLexer * lexer) {
    uint8_t const firstchar = lexer->_currdata;
    strbuf_reset(&lexer->state.string_buffer);
    lexer_next(lexer);

    // Hex
    if (firstchar == '0' && toupper(lexer->_currdata) == 'X') {
        lexer_next(lexer);

        while (is_hex_digit(lexer->_currdata)) {
            strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
            lexer_next(lexer);
        }

        if (lexer->state.string_buffer.len > 16) {
            lexer_error(lexer, "too many digits for a hex number");
            // noreturn
        }

        strbuf_push_back(&lexer->state.string_buffer, '\0');
        lex_integer_hex(lexer->state.string_buffer.data, &lexer->state.uint_value);

        return TK_INTEGER;
    }

    // Oct
    if (firstchar == '0' && scisodigit(lexer->_currdata)) {
        while(scisodigit(lexer->_currdata)) {
            strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
            lexer_next(lexer);
        }

        if (is_digit(lexer->_currdata)) {
            lexer_error(lexer, "invalid octal number");
        }

        strbuf_push_back(&lexer->state.string_buffer, '\0');
        LexOctal(reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]), &lexer->state.uint_value);

        return TK_INTEGER;
    }

    bool is_float = false;
    strbuf_push_back(&lexer->state.string_buffer, firstchar);
    while (lexer->_currdata == '.' || is_digit(lexer->_currdata) || isexponent(lexer->_currdata)) {
        if (lexer->_currdata == '.') {
            is_float = true;
        } else if (isexponent(lexer->_currdata)) {
            is_float = true;

            strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
            lexer_next(lexer);

            if (lexer->_currdata == '+' || lexer->_currdata == '-') {
                strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
                lexer_next(lexer);
            }

            if (!is_digit(lexer->_currdata)) {
                lexer_error(lexer, "exponent expected");
                // noreturn
            }
        }

        strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
        lexer_next(lexer);
    }
    strbuf_push_back(&lexer->state.string_buffer, '\0');

    if (is_float) {
        lexer->state.float_value = strtod(reinterpret_cast<char const *>(lexer->state.string_buffer.data), nullptr);
        return TK_FLOAT;
    }
    
    lex_integer(lexer->state.string_buffer.data, &lexer->state.uint_value);
    return TK_INTEGER;
}

uint16_t lexer_read_id(SQLexer * lexer) {
    uint16_t res;
    strbuf_reset(&lexer->state.string_buffer);
    do {
        strbuf_push_back(&lexer->state.string_buffer, lexer->_currdata);
        lexer_next(lexer);
    } while(scisalnum(lexer->_currdata) || lexer->_currdata == '_');

    strbuf_push_back(&lexer->state.string_buffer, '\0');

    res = lexer_string_to_token(reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]), lexer->state.string_buffer.len - 1);
    if(res == TK_IDENTIFIER || res == TK_CONSTRUCTOR) {
        lexer->state.string_value = reinterpret_cast<char const *>(&lexer->state.string_buffer.data[0]);
    }

    return res;
}

void lexer_block_comment(SQLexer * lexer) {
    while (true) {
        switch(lexer->_currdata) {
        case '*':
            lexer_next(lexer);
            if (lexer->_currdata == '/') {
                lexer_next(lexer);
                return;
            }
            continue;
        case '\n':
            lexer->state.current_line++;
            lexer_next(lexer);
            continue;
        case SQUIRREL_EOB:
            lexer_error(lexer, "missing \"*/\" in comment");
            // bug? lexer_error is noreturn probably.. because longjmp in compiler
        default:
            lexer_next(lexer);
        }
    }
}

void lexer_line_comment(SQLexer * lexer) {
    do {
        lexer_next(lexer);
    } while (lexer->_currdata != '\n' && lexer->_currdata > SQUIRREL_EOB);
}

void lexer_init(SQLexer * lexer, SQLEXREADFUNC rg, void * up, CompilerErrorFunc efunc, void *ed) {
    lexer->state.token = 0;
    lexer->state.prev_token = 0;

    lexer->state.current_line = 1;
    lexer->state.current_column = 0;
    lexer->state.last_token_line = 1;
    lexer->state.uint_value = 0;
    lexer->state.float_value = 0;
    lexer->state.string_value = nullptr;
    strbuf_init(&lexer->state.string_buffer);

    lexer->reader_func = rg;
    lexer->reader_context = up;
    lexer->error_func = efunc;
    lexer->error_context = ed;

    lexer->state.token = 0;
    lexer->_reached_eof = SQFalse;
    lexer->_currdata = 0;

    lexer_next(lexer);
    lexer->state.current_column--;
}

LexerState * lexer_lex(SQLexer * lexer) {

#define RETURN_TOKEN(t) { lexer->state.token = t; return &lexer->state;}

    lexer->state.last_token_line = lexer->state.current_line;

    while (lexer->_currdata != 0) {
        lexer->state.prev_token = lexer->state.token;

        switch(lexer->_currdata){
        case '\t':
        case '\r':
        case ' ':
            lexer_next(lexer);
            continue;
        case '\n':
            lexer->state.current_line++;
            lexer->state.token = '\n';
            lexer_next(lexer);
            lexer->state.current_column=1;
            continue;
        case '#':
            lexer_line_comment(lexer);
            continue;
        case '/':
            lexer_next(lexer);

            switch (lexer->_currdata) {
            case '*':
                lexer_next(lexer);
                lexer_block_comment(lexer);
                continue;
            case '/':
                lexer_line_comment(lexer);
                continue;
            case '=':
                lexer_next(lexer);
                RETURN_TOKEN(TK_DIVEQ);
            case '>':
                lexer_next(lexer);
                RETURN_TOKEN(TK_ATTR_CLOSE);
            default:
                RETURN_TOKEN('/');
            }
        case '=':
            lexer_next(lexer);
            switch (lexer->_currdata) {
            case '=':
                lexer_next(lexer);
                RETURN_TOKEN(TK_EQ);
            default:
                RETURN_TOKEN('=');
            }
        case '<':
            lexer_next(lexer);
            switch(lexer->_currdata) {
            case '=':
                lexer_next(lexer);
                switch (lexer->_currdata) {
                case '>':
                    lexer_next(lexer);
                    RETURN_TOKEN(TK_3WAYSCMP);
                default:
                    RETURN_TOKEN(TK_LE)
                }
            case '-':
                lexer_next(lexer);
                RETURN_TOKEN(TK_NEWSLOT);
            case '<':
                lexer_next(lexer);
                RETURN_TOKEN(TK_SHIFTL);
            case '/':
                lexer_next(lexer);
                RETURN_TOKEN(TK_ATTR_OPEN);
            default:
                RETURN_TOKEN('<');
            }
        case '>':
            lexer_next(lexer);
            switch (lexer->_currdata) {
            case '=':
                lexer_next(lexer);
                RETURN_TOKEN(TK_GE);
            case '>':
                lexer_next(lexer);
                switch (lexer->_currdata) {
                case '>':
                    lexer_next(lexer);
                    RETURN_TOKEN(TK_USHIFTR);
                default:
                    RETURN_TOKEN(TK_SHIFTR);
                }
            default:
                RETURN_TOKEN('>');
            }
        case '!':
            lexer_next(lexer);
            switch (lexer->_currdata) {
            case '=':
                lexer_next(lexer);
                RETURN_TOKEN(TK_NE);
            default:
                RETURN_TOKEN('!')
            }
        case '@':
            lexer_next(lexer);
            switch (lexer->_currdata) {
            case '"': {
                SQInteger const stype = lexer_read_string(lexer, '"', true);
                if (stype == -1) {
                    lexer_error(lexer, "error parsing the string");
                    // noreturn
                }
                RETURN_TOKEN(stype);
            }
            default:
                RETURN_TOKEN('@');
            }
        case '"':
        case '\'': {
            SQInteger const stype = lexer_read_string(lexer, lexer->_currdata, false);
            if (stype == -1) {
                lexer_error(lexer, "error parsing the string");
                // noreturn
            }
            RETURN_TOKEN(stype);
        }
        case '{': case '}': case '(': case ')': case '[': case ']':
        case ';': case ',': case '?': case '^': case '~': {
            uint16_t const token = lexer->_currdata;
            lexer_next(lexer);
            RETURN_TOKEN(token);
        }
        case '.':
            lexer_next(lexer);
            if (lexer->_currdata != '.'){ RETURN_TOKEN('.') }
            lexer_next(lexer);
            if (lexer->_currdata != '.'){ lexer_error(lexer, "invalid token '..'"); }
            lexer_next(lexer);
            RETURN_TOKEN(TK_VARPARAMS);
        case '&':
            lexer_next(lexer);
            if (lexer->_currdata != '&'){ RETURN_TOKEN('&') }
            else { lexer_next(lexer); RETURN_TOKEN(TK_AND); }
        case '|':
            lexer_next(lexer);
            if (lexer->_currdata != '|'){ RETURN_TOKEN('|') }
            else { lexer_next(lexer); RETURN_TOKEN(TK_OR); }
        case ':':
            lexer_next(lexer);
            if (lexer->_currdata != ':'){ RETURN_TOKEN(':') }
            else { lexer_next(lexer); RETURN_TOKEN(TK_DOUBLE_COLON); }
        case '*':
            lexer_next(lexer);
            if (lexer->_currdata == '='){ lexer_next(lexer); RETURN_TOKEN(TK_MULEQ);}
            else RETURN_TOKEN('*');
        case '%':
            lexer_next(lexer);
            if (lexer->_currdata == '='){ lexer_next(lexer); RETURN_TOKEN(TK_MODEQ);}
            else RETURN_TOKEN('%');
        case '-':
            lexer_next(lexer);
            if (lexer->_currdata == '='){ lexer_next(lexer); RETURN_TOKEN(TK_MINUSEQ);}
            else if  (lexer->_currdata == '-'){ lexer_next(lexer); RETURN_TOKEN(TK_MINUSMINUS);}
            else RETURN_TOKEN('-');
        case '+':
            lexer_next(lexer);
            if (lexer->_currdata == '='){ lexer_next(lexer); RETURN_TOKEN(TK_PLUSEQ);}
            else if (lexer->_currdata == '+'){ lexer_next(lexer); RETURN_TOKEN(TK_PLUSPLUS);}
            else RETURN_TOKEN('+');
        default:
            if (is_digit(lexer->_currdata)) {
                uint16_t ret = lexer_read_number(lexer);
                RETURN_TOKEN(ret);
            } else if (scisalpha(lexer->_currdata) || lexer->_currdata == '_') {
                uint16_t t = lexer_read_id(lexer);
                RETURN_TOKEN(t);
            } else {
                SQInteger c = lexer->_currdata;
                if (sciscntrl((int)c)) {
                    lexer_error(lexer, "unexpected character(control)");
                }
                lexer_next(lexer);
                RETURN_TOKEN(c);
            }
        }
    }

    RETURN_TOKEN(0);
}
