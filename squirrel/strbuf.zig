const std = @import("std");

pub const Strbuf = extern struct {
    const minimum_allocation = 16;

    len: usize,
    cap: usize,
    data: ?[*]u8,

    pub const init: Strbuf = .{
        .len = 0,
        .cap = 0,
        .data = null,
    };

    pub fn grow(self: *Strbuf) !void {
        const new_cap = @max(self.cap * 2, minimum_allocation);

        // Reconstruct fat pointer from our cap
        // as realloc expects and returns a slice
        const old_slice: []u8 = if (self.data) |ptr|
            ptr[0..self.cap]
        else
            &.{};

        const new_slice = try std.heap.c_allocator.realloc(old_slice, new_cap);
        self.data = new_slice.ptr;
        self.cap = new_slice.len;
    }

    pub fn deinit(self: *Strbuf) void {
        const old_slice: []u8 = if (self.data) |ptr|
            ptr[0..self.cap]
        else
            &.{};
        std.heap.c_allocator.free(old_slice);
    }
};

export fn strbuf_init(self: *Strbuf) void {
    self.* = .init;
}

export fn strbuf_deinit(self: *Strbuf) void {
    self.deinit();
}

export fn strbuf_reset(self: *Strbuf) void {
    self.len = 0;
}

export fn strbuf_push_back(self: *Strbuf, value: u8) void {
    if (self.cap <= self.len) {
        self.grow() catch @panic("can't grow strbuf");
    }
    self.data.?[self.len] = value;
    self.len += 1;
}
