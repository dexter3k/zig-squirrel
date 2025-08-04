const std = @import("std");

const squirrel = @cImport("squirrel.h");
const cLexer = @cImport({
    @cInclude("sqlexer.h");
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

export fn lexer_string_to_token(s: [*]const u8, len: usize) u16 {
    switch (len) {
        2 => {
            if (std.mem.eql(u8, s[0..len], "do")) return cLexer.TK_DO;
            if (std.mem.eql(u8, s[0..len], "if")) return cLexer.TK_IF;
            if (std.mem.eql(u8, s[0..len], "in")) return cLexer.TK_IN;
        },
        3 => {
            if (std.mem.eql(u8, s[0..len], "for")) return cLexer.TK_FOR;
            if (std.mem.eql(u8, s[0..len], "try")) return cLexer.TK_TRY;
        },
        4 => {
            if (std.mem.eql(u8, s[0..len], "base")) return cLexer.TK_BASE;
            if (std.mem.eql(u8, s[0..len], "case")) return cLexer.TK_CASE;
            if (std.mem.eql(u8, s[0..len], "else")) return cLexer.TK_ELSE;
            if (std.mem.eql(u8, s[0..len], "enum")) return cLexer.TK_ENUM;
            if (std.mem.eql(u8, s[0..len], "null")) return cLexer.TK_NULL;
            if (std.mem.eql(u8, s[0..len], "this")) return cLexer.TK_THIS;
            if (std.mem.eql(u8, s[0..len], "true")) return cLexer.TK_TRUE;
        },
        5 => {
            if (std.mem.eql(u8, s[0..len], "break")) return cLexer.TK_BREAK;
            if (std.mem.eql(u8, s[0..len], "catch")) return cLexer.TK_CATCH;
            if (std.mem.eql(u8, s[0..len], "class")) return cLexer.TK_CLASS;
            if (std.mem.eql(u8, s[0..len], "clone")) return cLexer.TK_CLONE;
            if (std.mem.eql(u8, s[0..len], "const")) return cLexer.TK_CONST;
            if (std.mem.eql(u8, s[0..len], "false")) return cLexer.TK_FALSE;
            if (std.mem.eql(u8, s[0..len], "local")) return cLexer.TK_LOCAL;
            if (std.mem.eql(u8, s[0..len], "throw")) return cLexer.TK_THROW;
            if (std.mem.eql(u8, s[0..len], "while")) return cLexer.TK_WHILE;
            if (std.mem.eql(u8, s[0..len], "yield")) return cLexer.TK_YIELD;
        },
        6 => {
            if (std.mem.eql(u8, s[0..len], "delete")) return cLexer.TK_DELETE;
            if (std.mem.eql(u8, s[0..len], "resume")) return cLexer.TK_RESUME;
            if (std.mem.eql(u8, s[0..len], "return")) return cLexer.TK_RETURN;
            if (std.mem.eql(u8, s[0..len], "static")) return cLexer.TK_STATIC;
            if (std.mem.eql(u8, s[0..len], "switch")) return cLexer.TK_SWITCH;
            if (std.mem.eql(u8, s[0..len], "typeof")) return cLexer.TK_TYPEOF;
        },
        7 => {
            if (std.mem.eql(u8, s[0..len], "default")) return cLexer.TK_DEFAULT;
            if (std.mem.eql(u8, s[0..len], "extends")) return cLexer.TK_EXTENDS;
            if (std.mem.eql(u8, s[0..len], "foreach")) return cLexer.TK_FOREACH;
            if (std.mem.eql(u8, s[0..len], "rawcall")) return cLexer.TK_RAWCALL;
        },
        8 => {
            if (std.mem.eql(u8, s[0..len], "__FILE__")) return cLexer.TK___FILE__;
            if (std.mem.eql(u8, s[0..len], "__LINE__")) return cLexer.TK___LINE__;
            if (std.mem.eql(u8, s[0..len], "continue")) return cLexer.TK_CONTINUE;
            if (std.mem.eql(u8, s[0..len], "function")) return cLexer.TK_FUNCTION;
        },
        10 => {
            if (std.mem.eql(u8, s[0..len], "instanceof")) return cLexer.TK_INSTANCEOF;
        },
        11 => {
            if (std.mem.eql(u8, s[0..len], "constructor")) return cLexer.TK_CONSTRUCTOR;
        },
        else => {},
    }

    return cLexer.TK_IDENTIFIER;
}

export fn lexer_deinit(lexer: *cLexer.SQLexer) void {
    const buf: *strbuf.Strbuf = @ptrCast(&lexer.state.string_buffer);
    buf.deinit();
}

export fn lexer_error(lexer: *cLexer.SQLexer, err: [*:0]const u8) void {
    lexer.error_func.?(lexer.error_context, err);
}

export fn lexer_next(lexer: *cLexer.SQLexer) void {
    const t = lexer.reader_func.?(lexer.reader_context);

    lexer.state.current_column += 1;
    lexer._currdata = t;

    if (t == 0) {
        lexer._reached_eof = true;
    }
}
