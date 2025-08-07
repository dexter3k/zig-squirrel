const std = @import("std");

const squirrel = @cImport("squirrel.h");
const cLexer = @cImport({
    @cInclude("lexer.h");
});
const strbuf = @import("./strbuf.zig");

export fn lexer_token_to_string(token: u16) [*:0]const u8 {
    return switch (token) {
        cLexer.TK_WHILE => "while",
        cLexer.TK_DO => "do",
        cLexer.TK_IF => "if",
        cLexer.TK_ELSE => "else",
        cLexer.TK_BREAK => "break",
        cLexer.TK_CONTINUE => "continue",
        cLexer.TK_RETURN => "return",
        cLexer.TK_NULL => "null",
        cLexer.TK_FUNCTION => "function",
        cLexer.TK_LOCAL => "local",
        cLexer.TK_FOR => "for",
        cLexer.TK_FOREACH => "foreach",
        cLexer.TK_IN => "in",
        cLexer.TK_TYPEOF => "typeof",
        cLexer.TK_BASE => "base",
        cLexer.TK_DELETE => "delete",
        cLexer.TK_TRY => "try",
        cLexer.TK_CATCH => "catch",
        cLexer.TK_THROW => "throw",
        cLexer.TK_CLONE => "clone",
        cLexer.TK_YIELD => "yield",
        cLexer.TK_RESUME => "resume",
        cLexer.TK_SWITCH => "switch",
        cLexer.TK_CASE => "case",
        cLexer.TK_DEFAULT => "default",
        cLexer.TK_THIS => "this",
        cLexer.TK_CLASS => "class",
        cLexer.TK_EXTENDS => "extends",
        cLexer.TK_CONSTRUCTOR => "constructor",
        cLexer.TK_INSTANCEOF => "instanceof",
        cLexer.TK_TRUE => "true",
        cLexer.TK_FALSE => "false",
        cLexer.TK_STATIC => "static",
        cLexer.TK_ENUM => "enum",
        cLexer.TK_CONST => "const",
        cLexer.TK___LINE__ => "__LINE__",
        cLexer.TK___FILE__ => "__FILE__",
        cLexer.TK_RAWCALL => "rawcall",
        else => "unknown token",
    };
}

export fn lexer_init(
    lexer: *cLexer.SQLexer,
    rf: cLexer.SQLEXREADFUNC,
    rc: cLexer.SQUserPointer,
    ef: cLexer.CompilerErrorFunc,
    ec: *anyopaque,
) void {
    lexer.state.token = 0;
    lexer.state.prev_token = 0;

    lexer.state.current_line = 1;
    lexer.state.current_column = 0;
    lexer.state.last_token_line = 1;
    lexer.state.uint_value = 0;
    lexer.state.float_value = 0;
    lexer.state.string_value = null;
    strbuf.strbuf_init(@ptrCast(&lexer.string_buffer));

    lexer.reader_func = rf;
    lexer.reader_context = rc;
    lexer.error_func = ef;
    lexer.error_context = ec;

    lexer.state.token = 0;
    lexer._currdata = 0;

    lexer_next(lexer);
    lexer.state.current_column -= 1;
}

export fn lexer_deinit(lexer: *cLexer.SQLexer) void {
    const buf: *strbuf.Strbuf = @ptrCast(&lexer.string_buffer);
    buf.deinit();
}

fn lexer_error(lexer: *cLexer.SQLexer, err: [*:0]const u8) void {
    lexer.error_func.?(lexer.error_context, err);
    unreachable;
}

fn lexer_next(lexer: *cLexer.SQLexer) void {
    const t = lexer.reader_func.?(lexer.reader_context);

    lexer.state.current_column += 1;
    lexer._currdata = t;
}

fn lexer_line_comment(lexer: *cLexer.SQLexer) void {
    while (lexer._currdata != '\n' and lexer._currdata != 0) {
        lexer_next(lexer);
    }
}

fn lexer_block_comment(lexer: *cLexer.SQLexer) void {
    while (true) switch (lexer._currdata) {
        '*' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                '/' => {
                    lexer_next(lexer);
                    return;
                },
                else => continue,
            }
        },
        '\n' => {
            lexer_next(lexer);
            lexer.state.current_line += 1;
            continue;
        },
        0 => {
            lexer_error(lexer, "missing \"*/\" in comment");
        },
        else => lexer_next(lexer),
    };
}

fn lex_integer_oct(s: [*:0]const u8, res: *u64) void {
    res.* = 0;
    var i: usize = 0;
    while (s[i] != 0) : (i += 1) {
        res.* = res.* * 8 + s[i] - '0';
    }
}

fn lex_integer_dec(s: [*:0]const u8, res: *u64) void {
    res.* = 0;
    var i: usize = 0;
    while (s[i] != 0) : (i += 1) {
        res.* = res.* * 10 + s[i] - '0';
    }
}

fn lex_integer_hex(s: [*:0]const u8, res: *u64) void {
    res.* = 0;
    var i: usize = 0;
    while (s[i] != 0) : (i += 1) {
        res.* = res.* * 16 + switch (s[i]) {
            '0'...'9' => s[i] - '0',
            'a'...'f' => s[i] - 'a' + 10,
            'A'...'F' => s[i] - 'A' + 10,
            else => unreachable,
        };
    }
}

fn lex_integer_hex_limited(lexer: *cLexer.SQLexer, limit: usize) u32 {
    std.debug.assert(limit > 0 and limit <= 8);

    if (!std.ascii.isHex(lexer._currdata)) {
        lexer_error(lexer, "hexadecimal number expected");
    }

    var res: u32 = 0;
    var i: usize = 0;
    while (std.ascii.isHex(lexer._currdata) and i < limit) : (i += 1) {
        res = res * 16 + switch (lexer._currdata) {
            '0'...'9' => lexer._currdata - '0',
            'a'...'f' => lexer._currdata - 'a' + 10,
            'A'...'F' => lexer._currdata - 'A' + 10,
            else => unreachable,
        };

        lexer_next(lexer);
    }

    return res;
}

fn lexer_read_escape(lexer: *cLexer.SQLexer) void {
    const escape = lexer._currdata;
    lexer_next(lexer);

    switch (escape) {
        'x' => strbuf.strbuf_push_back(
            @ptrCast(&lexer.string_buffer),
            @intCast(lex_integer_hex_limited(lexer, 2)),
        ),
        'u', 'U' => strbuf.strbuf_push_back_utf8(
            @ptrCast(&lexer.string_buffer),
            lex_integer_hex_limited(lexer, if (escape == 'u') 4 else 8),
        ),
        't' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\t'),
        'n' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\n'),
        'r' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\r'),
        'a' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x07'),
        'b' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x08'),
        'v' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x0b'),
        'f' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x0c'),
        '0' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x00'),
        '"' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '"'),
        '\\' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\\'),
        '\'' => strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\''),
        else => {
            lexer_error(lexer, "unrecognised escaper char");
        },
    }
}

fn lexer_read_string(lexer: *cLexer.SQLexer, delimiter: u8, verbatim: bool) u16 {
    strbuf.strbuf_reset(@ptrCast(&lexer.string_buffer));
    lexer_next(lexer);

    while (true) {
        while (lexer._currdata != delimiter) switch (lexer._currdata) {
            else => {
                strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
                lexer_next(lexer);
            },
            '\n' => {
                lexer_next(lexer);
                if (!verbatim) {
                    lexer_error(lexer, "newline in a constant");
                }
                strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\n');
                lexer.state.current_line += 1;
            },
            '\\' => {
                lexer_next(lexer);
                if (verbatim) {
                    strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\\');
                } else {
                    lexer_read_escape(lexer);
                }
            },
            0 => {
                lexer_error(lexer, "EOF before end of the string");
            },
        };

        lexer_next(lexer);
        if (verbatim and lexer._currdata == '"') {
            // continue on double quotations inside verbatim strings
            strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
            lexer_next(lexer);
            continue;
        }

        break;
    }

    strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x00');
    const len = lexer.string_buffer.len - 1;

    if (delimiter == '\'') {
        if (len == 0) {
            lexer_error(lexer, "empty constant");
        }
        if (len == 1) {
            lexer_error(lexer, "constant too long");
        }

        // sign-extend the value!
        const signed_byte: i8 = @bitCast(lexer.string_buffer.data[0]);
        lexer.state.uint_value = @bitCast(@as(i64, signed_byte));
        return cLexer.TK_INTEGER;
    }

    // *u8 -> *i8
    lexer.state.string_value = @ptrCast(&lexer.string_buffer.data[0]);
    return cLexer.TK_STRING_LITERAL;
}

fn is_oct_digit(c: u8) bool {
    return switch (c) {
        '0'...'7' => true,
        else => false,
    };
}

fn is_exponent(c: u8) bool {
    return switch (c) {
        'e', 'E' => true,
        else => false,
    };
}

fn lexer_read_number(lexer: *cLexer.SQLexer) u16 {
    const first_token = lexer._currdata;
    strbuf.strbuf_reset(@ptrCast(&lexer.string_buffer));
    lexer_next(lexer);

    if (first_token == '0' and std.ascii.toUpper(lexer._currdata) == 'X') {
        lexer_next(lexer);

        while (std.ascii.isHex(lexer._currdata)) {
            strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
            lexer_next(lexer);
        }

        if (lexer.string_buffer.len > 16) {
            lexer_error(lexer, "too many digits for a hex number");
        }

        strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x00');
        lex_integer_hex(lexer.string_buffer.data, &lexer.state.uint_value);

        return cLexer.TK_INTEGER;
    }

    if (first_token == '0' and is_oct_digit(lexer._currdata)) {
        while (is_oct_digit(lexer._currdata)) {
            strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
            lexer_next(lexer);
        }

        if (std.ascii.isDigit(lexer._currdata)) {
            lexer_error(lexer, "invalid octal number");
        }

        strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x00');
        lex_integer_oct(lexer.string_buffer.data, &lexer.state.uint_value);

        return cLexer.TK_INTEGER;
    }

    strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), first_token);

    var is_float = false;
    while (lexer._currdata == '.' or std.ascii.isDigit(lexer._currdata) or is_exponent(lexer._currdata)) {
        if (lexer._currdata == '.') {
            is_float = true;
        } else if (is_exponent(lexer._currdata)) {
            is_float = true;

            strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
            lexer_next(lexer);

            if (lexer._currdata == '+' or lexer._currdata == '-') {
                strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
                lexer_next(lexer);
            }

            if (!std.ascii.isDigit(lexer._currdata)) {
                lexer_error(lexer, "exponent expected");
            }
        }

        strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
        lexer_next(lexer);
    }

    strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x00');

    if (is_float) {
        // TODO: replicate strtod behaviour
        lexer.state.float_value = std.fmt.parseFloat(
            f64,
            lexer.string_buffer.data[0..lexer.string_buffer.len],
        ) catch 0;
        return cLexer.TK_FLOAT;
    }

    lex_integer_dec(lexer.string_buffer.data, &lexer.state.uint_value);
    return cLexer.TK_INTEGER;
}

fn lexer_string_to_token(s: []const u8) u16 {
    switch (s.len) {
        2 => {
            if (std.mem.eql(u8, s, "do")) return cLexer.TK_DO;
            if (std.mem.eql(u8, s, "if")) return cLexer.TK_IF;
            if (std.mem.eql(u8, s, "in")) return cLexer.TK_IN;
        },
        3 => {
            if (std.mem.eql(u8, s, "for")) return cLexer.TK_FOR;
            if (std.mem.eql(u8, s, "try")) return cLexer.TK_TRY;
        },
        4 => {
            if (std.mem.eql(u8, s, "base")) return cLexer.TK_BASE;
            if (std.mem.eql(u8, s, "case")) return cLexer.TK_CASE;
            if (std.mem.eql(u8, s, "else")) return cLexer.TK_ELSE;
            if (std.mem.eql(u8, s, "enum")) return cLexer.TK_ENUM;
            if (std.mem.eql(u8, s, "null")) return cLexer.TK_NULL;
            if (std.mem.eql(u8, s, "this")) return cLexer.TK_THIS;
            if (std.mem.eql(u8, s, "true")) return cLexer.TK_TRUE;
        },
        5 => {
            if (std.mem.eql(u8, s, "break")) return cLexer.TK_BREAK;
            if (std.mem.eql(u8, s, "catch")) return cLexer.TK_CATCH;
            if (std.mem.eql(u8, s, "class")) return cLexer.TK_CLASS;
            if (std.mem.eql(u8, s, "clone")) return cLexer.TK_CLONE;
            if (std.mem.eql(u8, s, "const")) return cLexer.TK_CONST;
            if (std.mem.eql(u8, s, "false")) return cLexer.TK_FALSE;
            if (std.mem.eql(u8, s, "local")) return cLexer.TK_LOCAL;
            if (std.mem.eql(u8, s, "throw")) return cLexer.TK_THROW;
            if (std.mem.eql(u8, s, "while")) return cLexer.TK_WHILE;
            if (std.mem.eql(u8, s, "yield")) return cLexer.TK_YIELD;
        },
        6 => {
            if (std.mem.eql(u8, s, "delete")) return cLexer.TK_DELETE;
            if (std.mem.eql(u8, s, "resume")) return cLexer.TK_RESUME;
            if (std.mem.eql(u8, s, "return")) return cLexer.TK_RETURN;
            if (std.mem.eql(u8, s, "static")) return cLexer.TK_STATIC;
            if (std.mem.eql(u8, s, "switch")) return cLexer.TK_SWITCH;
            if (std.mem.eql(u8, s, "typeof")) return cLexer.TK_TYPEOF;
        },
        7 => {
            if (std.mem.eql(u8, s, "default")) return cLexer.TK_DEFAULT;
            if (std.mem.eql(u8, s, "extends")) return cLexer.TK_EXTENDS;
            if (std.mem.eql(u8, s, "foreach")) return cLexer.TK_FOREACH;
            if (std.mem.eql(u8, s, "rawcall")) return cLexer.TK_RAWCALL;
        },
        8 => {
            if (std.mem.eql(u8, s, "__FILE__")) return cLexer.TK___FILE__;
            if (std.mem.eql(u8, s, "__LINE__")) return cLexer.TK___LINE__;
            if (std.mem.eql(u8, s, "continue")) return cLexer.TK_CONTINUE;
            if (std.mem.eql(u8, s, "function")) return cLexer.TK_FUNCTION;
        },
        10 => {
            if (std.mem.eql(u8, s, "instanceof")) return cLexer.TK_INSTANCEOF;
        },
        11 => {
            if (std.mem.eql(u8, s, "constructor")) return cLexer.TK_CONSTRUCTOR;
        },
        else => {},
    }

    return cLexer.TK_IDENTIFIER;
}

fn lexer_read_id(lexer: *cLexer.SQLexer) u16 {
    strbuf.strbuf_reset(@ptrCast(&lexer.string_buffer));

    while (true) {
        strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), lexer._currdata);
        lexer_next(lexer);

        if (!std.ascii.isAlphanumeric(lexer._currdata) and lexer._currdata != '_') {
            break;
        }
    }
    strbuf.strbuf_push_back(@ptrCast(&lexer.string_buffer), '\x00');

    const res = lexer_string_to_token(lexer.string_buffer.data[0 .. lexer.string_buffer.len - 1]);
    if (res == cLexer.TK_IDENTIFIER or res == cLexer.TK_CONSTRUCTOR) {
        lexer.state.string_value = @ptrCast(&lexer.string_buffer.data[0]);
    }

    return res;
}

fn lexer_lex_internal(lexer: *cLexer.SQLexer) u16 {
    lexer.state.last_token_line = lexer.state.current_line;

    while (true) switch (lexer._currdata) {
        0 => return 0,
        '\t', ' ', '\r' => lexer_next(lexer),
        '\n' => {
            lexer_next(lexer);
            lexer.state.current_line += 1;
            lexer.state.current_column = 1;
            lexer.state.token = '\n';
        },
        '#' => {
            lexer_next(lexer);
            lexer_line_comment(lexer);
        },
        '/' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '/',
                '*' => {
                    lexer_next(lexer);
                    lexer_block_comment(lexer);
                },
                '/' => {
                    lexer_next(lexer);
                    lexer_line_comment(lexer);
                },
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_DIVEQ;
                },
                '>' => {
                    lexer_next(lexer);
                    return cLexer.TK_ATTR_CLOSE;
                },
            }
        },
        '=' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '=',
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_EQ;
                },
            }
        },
        '<' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '<',
                '-' => {
                    lexer_next(lexer);
                    return cLexer.TK_NEWSLOT;
                },
                '<' => {
                    lexer_next(lexer);
                    return cLexer.TK_SHIFTL;
                },
                '/' => {
                    lexer_next(lexer);
                    return cLexer.TK_ATTR_OPEN;
                },
                '=' => {
                    lexer_next(lexer);
                    switch (lexer._currdata) {
                        else => return cLexer.TK_LE,
                        '>' => {
                            lexer_next(lexer);
                            return cLexer.TK_3WAYSCMP;
                        },
                    }
                },
            }
        },
        '>' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '>',
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_GE;
                },
                '>' => {
                    lexer_next(lexer);
                    switch (lexer._currdata) {
                        else => return cLexer.TK_SHIFTR,
                        '>' => {
                            lexer_next(lexer);
                            return cLexer.TK_USHIFTR;
                        },
                    }
                },
            }
        },
        '!' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '!',
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_NE;
                },
            }
        },
        '@' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '@',
                '"' => return lexer_read_string(lexer, '"', true),
            }
        },
        '"' => return lexer_read_string(lexer, '"', false),
        '\'' => return lexer_read_string(lexer, '\'', false),
        '{', '}', '(', ')', '[', ']', ';', ',', '?', '^', '~' => {
            const token = lexer._currdata;
            lexer_next(lexer);
            return token;
        },
        '.' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '.',
                '.' => {
                    lexer_next(lexer);
                    switch (lexer._currdata) {
                        else => {
                            lexer_error(lexer, "invalid token '..'");
                        },
                        '.' => {
                            lexer_next(lexer);
                            return cLexer.TK_VARPARAMS;
                        },
                    }
                },
            }
        },
        '&' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '&',
                '&' => {
                    lexer_next(lexer);
                    return cLexer.TK_AND;
                },
            }
        },
        '|' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '|',
                '|' => {
                    lexer_next(lexer);
                    return cLexer.TK_OR;
                },
            }
        },
        ':' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return ':',
                ':' => {
                    lexer_next(lexer);
                    return cLexer.TK_DOUBLE_COLON;
                },
            }
        },
        '*' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '*',
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_MULEQ;
                },
            }
        },
        '%' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '%',
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_MODEQ;
                },
            }
        },
        '-' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '-',
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_MINUSEQ;
                },
                '-' => {
                    lexer_next(lexer);
                    return cLexer.TK_MINUSMINUS;
                },
            }
        },
        '+' => {
            lexer_next(lexer);
            switch (lexer._currdata) {
                else => return '+',
                '=' => {
                    lexer_next(lexer);
                    return cLexer.TK_PLUSEQ;
                },
                '+' => {
                    lexer_next(lexer);
                    return cLexer.TK_PLUSPLUS;
                },
            }
        },

        '0'...'9' => return lexer_read_number(lexer),
        'a'...'z', 'A'...'Z', '_' => return lexer_read_id(lexer),

        1...8, 11...12, 14...31, '$', '\\', '`', 0x7f => {
            lexer_error(lexer, "unexpected control symbol");
        },
        0x80...0xff => {
            lexer_error(lexer, "unexpected unicode symbol");
        },
    };
}

export fn lexer_lex(lexer: *cLexer.SQLexer) *cLexer.LexerState {
    const token = lexer_lex_internal(lexer);
    lexer.state.prev_token = lexer.state.token;
    lexer.state.token = token;
    if (token == cLexer.TK_STRING_LITERAL) {
        lexer.state.string_length = lexer.string_buffer.len - 1;
    }

    return &lexer.state;
}
