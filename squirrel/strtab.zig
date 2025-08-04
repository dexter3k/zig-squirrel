const std = @import("std");
const builtin = @import("builtin");

export fn hash_strings(a: [*]const u8, a_len: usize, b: [*]const u8, b_len: usize) usize {
    const step = ((a_len + b_len) >> 5) + 1;

    var h = a_len + b_len;
    var i = a_len + b_len;

    while (i >= step and i > a_len) {
        const index = i - 1 - a_len;
        h = h ^ ((h << 5) +% (h >> 2) +% b[index]);
        i -= step;
    }

    while (i >= step) {
        const index = i - 1;
        h = h ^ ((h << 5) +% (h >> 2) +% a[index]);
        i -= step;
    }

    return h;
}

var debug_allocator: std.heap.DebugAllocator(.{}) = .init;

export fn strtab_init(user_pointer: *anyopaque) ?*anyopaque {
    const gpa, const is_debug = gpa: {
        if (builtin.os.tag == .wasi) break :gpa .{ std.heap.wasm_allocator, false };
        break :gpa switch (builtin.mode) {
            .Debug, .ReleaseSafe => .{ debug_allocator.allocator(), true },
            .ReleaseFast, .ReleaseSmall => .{ std.heap.smp_allocator, false },
        };
    };
    defer if (is_debug) {
        _ = debug_allocator.deinit();
    };

    _ = user_pointer;

    return StringTable.init(gpa) catch null;
}

export fn strtab_deinit(context: ?*anyopaque) void {
    if (context == null) {
        return;
    }

    const self: *StringTable = @ptrCast(@alignCast(context));
    self.deinit();
}

pub const StringTable = struct {
    const UnmanagedTable = std.HashMapUnmanaged([]const u8, void, std.hash_map.StringContext, 80);

    allocator: std.mem.Allocator,
    table: UnmanagedTable,

    pub fn init(allocator: std.mem.Allocator) !*StringTable {
        const this = try allocator.create(StringTable);
        this.* = .{
            .allocator = allocator,
            .table = .empty,
        };
        return this;
    }

    pub fn deinit(st: *StringTable) void {
        st.allocator.destroy(st);
    }
};
