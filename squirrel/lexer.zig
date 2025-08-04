const squirrel = @cImport("squirrel.h");
const cLexer = @cImport({
    @cInclude("sqlexer.h");
});
const strbuf = @import("./strbuf.zig");

export fn token_to_string(token: u16) [*:0]const u8 {
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

export fn lexer_deinit(lexer: *cLexer.SQLexer) void {
    const buf: *strbuf.Strbuf = @ptrCast(&lexer.state.string_buffer);
    buf.deinit();
}
