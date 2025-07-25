const std = @import("std");
const builtin = @import("builtin");
const allocator = std.heap.c_allocator;

const c = @cImport({
    @cInclude("stdio.h");
});

const csq = @cImport({
    @cInclude("squirrel.h");
    @cInclude("sqstdblob.h");
    @cInclude("sqstdsystem.h");
    @cInclude("sqstdio.h");
    @cInclude("sqstdmath.h");
    @cInclude("sqstdstring.h");
    @cInclude("sqstdaux.h");
});

// VaList is not implemented on x86_64-windows, so we keep it like this.
extern "c" fn printfunc(vm: csq.HSQUIRRELVM, [*c]const u8, ...) void;
extern "c" fn errorfunc(vm: csq.HSQUIRRELVM, [*c]const u8, ...) void;

fn print_version_infos() !void {
    var stdout = std.fs.File.stdout().writer(&.{});
    try stdout.interface.print("{s} {s} ({d} bits)\n", .{
        csq.SQUIRREL_VERSION,
        csq.SQUIRREL_COPYRIGHT,
        @bitSizeOf(csq.SQInteger),
    });
}

fn print_usage() void {
    std.debug.print("usage: sq <options> <scriptpath [args]\n", .{});
    std.debug.print("Available options are:\n", .{});
    std.debug.print("   -c              compiles the file to bytecode(default output 'out.cnut')\n", .{});
    std.debug.print("   -o              specifies output file for the -c option\n", .{});
    std.debug.print("   -c              compiles only\n", .{});
    std.debug.print("   -d              generates debug infos\n", .{});
    std.debug.print("   -v              displays version infos\n", .{});
    std.debug.print("   -h              prints help\n", .{});
}

fn getargs(vm: csq.HSQUIRRELVM, argc: c_int, argv: [*][*:0]u8, retval: *csq.SQInteger) c_int {
    retval.* = 0;
    if (argc <= 1) {
        return 0;
    }

    var only_compilation: bool = false;
    var compilation_path: [*:0]const u8 = "out.cnut";
    var i: usize = 1;
    while (i < argc) {
        if (argv[i][0] != '-') {
            break;
        }

        switch (argv[i][1]) {
            'd' => {
                csq.sq_enabledebuginfo(vm, 1);
            },
            'c' => {
                only_compilation = true;
            },
            'o' => {
                if (i + 1 < argc) {
                    compilation_path = argv[i + 1];
                    i += 1;
                }
            },
            'v' => {
                print_version_infos() catch unreachable;
            },
            'h' => {
                print_version_infos() catch unreachable;
                print_usage();
            },
            else => {
                print_version_infos() catch unreachable;
                std.debug.print("unknown parameter: '-{c}'\n", .{argv[i][1]});
                print_usage();
                retval.* = -1;
                return 3;
            },
        }

        i += 1;
    }

    if (i >= argc) {
        return 0;
    }

    const source_path = argv[i];
    i += 1;
    if (csq.SQ_SUCCEEDED(csq.sqstd_loadfile(vm, source_path, csq.SQTrue))) {
        if (only_compilation) {
            if (csq.SQ_SUCCEEDED(csq.sqstd_writeclosuretofile(vm, compilation_path))) {
                return 2;
            }
        } else {
            // Push provided arguments (as well as the root table)
            var call_args: c_int = 1;
            csq.sq_pushroottable(vm);
            while (i < argc) {
                const arg = argv[i];
                csq.sq_pushstring(vm, arg, -1);
                call_args += 1;

                i += 1;
            }

            if (!csq.SQ_SUCCEEDED(csq.sq_call(vm, call_args, csq.SQTrue, csq.SQTrue))) {
                return 3;
            }

            const ret_type = csq.sq_gettype(vm, -1);
            if (ret_type == csq.OT_INTEGER) {
                retval.* = ret_type;
                _ = csq.sq_getinteger(vm, -1, retval);
            }

            return 2;
        }
    }

    _ = csq.sq_getlasterror(vm);
    var err: [*c]const u8 = undefined;
    if (csq.SQ_SUCCEEDED(csq.sq_getstring(vm, -1, &err))) {
        std.debug.print("Error [{s}]\n", .{err});
        retval.* = -2;
        return 3;
    }

    return 0;
}

fn quit(vm: csq.HSQUIRRELVM) callconv(.c) csq.SQInteger {
    var done: *csq.SQInteger = undefined;
    _ = csq.sq_getuserpointer(vm, -1, @ptrCast(&done));
    done.* = 1;

    return 0;
}

fn interactive(vm: csq.HSQUIRRELVM) void {
    print_version_infos() catch unreachable;

    csq.sq_pushroottable(vm);
    csq.sq_pushstring(vm, "quit", -1);

    var done: csq.SQInteger = 0;
    csq.sq_pushuserpointer(vm, &done);
    csq.sq_newclosure(vm, quit, 1);
    _ = csq.sq_setparamscheck(vm, 1, null);
    _ = csq.sq_newslot(vm, -3, csq.SQFalse);
    _ = csq.sq_pop(vm, 1);

    prompt: while (done == 0) {
        var stdout = std.fs.File.stdout().writer(&.{});
        stdout.interface.print("\nsq>", .{}) catch unreachable;

        // This entire thing is really broken :DDD
        const max_input = 1000;
        var buffer = [_]u8{0} ** max_input;
        var i: u32 = 0;
        var blocks: u32 = 0;
        var inside_string = false;

        while (true) {
            var reader_buffer = [_]u8{0} ** 1;
            var stdin = std.fs.File.stdin().reader(reader_buffer[0..]);
            const ch = stdin.interface.takeByte() catch unreachable;
            if (ch == '\n') {
                if (i > 0 and buffer[i - 1] == '\\') {
                    buffer[i - 1] = '\n';
                } else if (blocks == 0) {
                    break;
                }

                buffer[i] = ch;
                i += 1;
            } else if (ch == '}') {
                blocks -= 1;
                buffer[i] = ch;
                i += 1;
            } else if (ch == '{' and !inside_string) {
                blocks += 1;
                buffer[i] = ch;
                i += 1;
            } else if (ch == '"' or ch == '\'') {
                inside_string = !inside_string;
                buffer[i] = ch;
                i += 1;
            } else if (i + 1 >= max_input) {
                std.debug.print("sq : input line too long\n", .{});
                continue :prompt;
            } else {
                buffer[i] = ch;
                i += 1;
            }
        }

        if (i == 0) {
            continue;
        }

        buffer[i] = 0;

        var has_retval: csq.SQBool = csq.SQFalse;
        if (buffer[0] == '=') {
            // todo: skip = and turn into 'return(expression)'
            // return({s})
            // we need 8 extra bytes in the buffer
            if (i + 8 + 1 >= max_input) {
                std.debug.print("sq : input line too long\n", .{});
                continue;
            }

            // move input forward 6 bytes, ignoring the first equals
            for (0..i) |j| {
                buffer[i - j + 6] = buffer[i - j];
            }
            @memcpy(buffer[0..7], "return(");
            buffer[i + 5] = ')';
            buffer[i + 6] = 0;
            i += 6;

            has_retval = csq.SQTrue;
        }

        const old_top = csq.sq_gettop(vm);
        defer csq.sq_settop(vm, old_top);

        if (!csq.SQ_SUCCEEDED(csq.sq_compilebuffer(vm, buffer[0..i :0], i, "interactive console", csq.SQTrue))) {
            continue;
        }

        csq.sq_pushroottable(vm);

        if (!csq.SQ_SUCCEEDED(csq.sq_call(vm, 1, has_retval, csq.SQTrue))) {
            continue;
        }

        if (has_retval == csq.SQFalse) {
            continue;
        }

        stdout.interface.print("\n", .{}) catch unreachable;
        csq.sq_pushroottable(vm);
        csq.sq_pushstring(vm, "print", -1);
        _ = csq.sq_get(vm, -2);
        csq.sq_pushroottable(vm);
        csq.sq_push(vm, -4);
        _ = csq.sq_call(vm, 2, csq.SQFalse, csq.SQTrue);
        stdout.interface.print("\n", .{}) catch unreachable;
    }
}

// const squirrel = @import("squirrel.zig");
// const Squirrel = squirrel.Squirrel(u32, f32, false);

// fn pg() !void {
//     var debug_allocator: std.heap.DebugAllocator(.{}) = .init;
//     defer _ = debug_allocator.deinit();
//     const gpa = debug_allocator.allocator();

//     const vm: *Squirrel.VM = try .init(gpa, .{ .initial_stack = 1024 });
//     defer vm.deinit();

//     std.debug.print("{any}\n", .{vm});
// }

pub fn main() !void {
    // Fix console output on Windows
    var original_cp: if (builtin.os.tag == .windows) c_uint else void = undefined;
    if (builtin.os.tag == .windows) {
        original_cp = std.os.windows.kernel32.GetConsoleOutputCP();
        _ = std.os.windows.kernel32.SetConsoleOutputCP(65001);
    }
    // Restore original encoding
    defer if (builtin.os.tag == .windows) {
        _ = std.os.windows.kernel32.SetConsoleOutputCP(original_cp);
    };

    // Get cmdline args
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    // Convert to C argv
    var argv = try allocator.alloc([*:0]u8, args.len);
    defer allocator.free(argv);
    for (args, 0..) |s, i| {
        argv[i] = s.ptr;
    }

    const vm: csq.HSQUIRRELVM = csq.sq_open(1024);
    defer csq.sq_close(vm);

    csq.sq_setprintfunc(vm, printfunc, errorfunc);

    csq.sq_pushroottable(vm);
    _ = csq.sqstd_register_bloblib(vm);
    _ = csq.sqstd_register_iolib(vm);
    _ = csq.sqstd_register_systemlib(vm);
    _ = csq.sqstd_register_mathlib(vm);
    _ = csq.sqstd_register_stringlib(vm);
    _ = csq.sqstd_seterrorhandlers(vm);

    var ret: csq.SQInteger = 0;
    if (getargs(vm, @intCast(args.len), argv.ptr, &ret) == 0) {
        interactive(vm);
    }

    std.c.exit(@intCast(ret));
}
