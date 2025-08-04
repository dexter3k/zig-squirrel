const std = @import("std");

const Gc = @This();

allocator: std.mem.Allocator,

pub fn init(allocator: std.mem.Allocator) !*Gc {
    const gc = try allocator.create(Gc);
    gc.* = .{
        .allocator = allocator,
    };
    return gc;
}

pub fn deinit(gc: *Gc) void {
    gc.allocator.destroy(gc);
}

const Object = struct {
    refs: usize,
    weak: *anyopaque,
    owner: *Gc,
    next: *Object,
    prev: *Object,
};

pub fn create(gc: *Gc, comptime T: type) !*T {
    if (@sizeOf(T) == 0) @compileError("T must have non-zero size");

    _ = gc;
    unreachable;
}
