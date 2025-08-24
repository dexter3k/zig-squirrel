comptime {
    _ = @import("strtab.zig");
    _ = @import("lexer.zig");
    _ = @import("strbuf.zig");
}

const std = @import("std");
const builtin = @import("builtin");

// Garbage Collected / Reference Counted
// - String Table: manages string allocation and storage
//
// Reference Counted
// - SQString: manages refcount and storage for strings inside String Table
// - SQWeakRef: manages a weakref to another refcounted value
// - SQCollectable: wraps RefCounted to facilitate Garbage Collection
//   - holds next and prev pointer for the GC chain, refcounter is reused for Marking
//
// Garbage Collected
// - SQArray
// - SQClass
// - SQClosure
// - SQDelegable
// - SQFunctionProto
// - SQGenerator
// - SQInstance:SQDelegable
// - SQNativeClosure
// - SQOuter
// - SQTable:SQDelegable
// - SQUserData:SQDelegable
// - SQVM
//
// Other types
// SQObject: fat pointer, type and union of values (including plain types)
// SQObjectPtr:SQObject: facilitates automatic C++ style interaction with SQObject
// - RAII, copy constructors, set operator. Automatic ref management

const Integer = i64;
const Number = f64;

const GC = struct {
    allocator: std.mem.Allocator,

    chain_root: ?*Collectable,

    fn init(allocator: std.mem.Allocator) GC {
        return .{
            .allocator = allocator,
            .chain_root = null,
        };
    }
};

const ObjectType = enum(u32) {
    null,
    integer,
    boolean,
    number,

    string,

    const flag_ref_counted: u32 = 0x08000000;

    pub fn isRefCounted(t: ObjectType) bool {
        if (@intFromEnum(t) & flag_ref_counted) {
            return true;
        }

        return false;
    }
};

const ObjectPtr = extern struct {
    const Value = extern union {
        integer: Integer,
        number: Number,

        ref_counted: *RefCounted,
        string: *String,
    };

    type: ObjectType,
    value: Value,

    pub const @"null": ObjectPtr = .{ .type = .null, .value = std.mem.zeroes(Value) };

    pub fn integer(v: Integer) ObjectPtr {
        var p: ObjectPtr = .{ .type = .integer, .value = std.mem.zeroes(Value) };
        p.value.integer = v;
        return p;
    }

    pub fn boolean(v: bool) ObjectPtr {
        var p: ObjectPtr = .{ .type = .boolean, .value = std.mem.zeroes(Value) };
        p.value.integer = @intFromBool(v);
        return p;
    }

    pub fn number(v: Number) ObjectPtr {
        var p: ObjectPtr = .{ .type = .number, .value = std.mem.zeroes(Value) };
        p.value.number = v;
        return p;
    }

    fn increaseRefCount(ptr: *ObjectPtr) void {
        if (ptr.type.isRefCounted()) {
            ptr.value.ref_counted.increaseRefCount();
        }
    }

    fn decreaseRefCount(ptr: *ObjectPtr) void {
        if (ptr.type.isRefCounted()) {
            ptr.value.ref_counted.decreaseRefCount();
        }
    }
};

const RefCounted = extern struct {
    const RefCount = packed struct {
        mark: bool,
        ref_count: u31,

        const init_one: RefCount = .{ .mark = false, .ref_count = 1 };
    };

    const VTable = extern struct {
        destructor: *const fn (ptr: *anyopaque) callconv(.c) void,
        release: *const fn (ptr: *anyopaque) callconv(.c) void,
    };

    vtable: *const VTable,
    gc: *GC, // todo: make comptime optional
    ref_count: RefCount = .init_one,
    weak_ref: ?*WeakRef = null,

    fn destructor(ptr: *anyopaque) callconv(.c) void {
        const self: *RefCounted = @ptrCast(@alignCast(ptr));
        if (self.weak_ref) |weak_ref| {
            weak_ref.invalidate();
        }
    }

    fn increaseRefCount(this: *RefCounted) void {
        // TODO: we should probably guard this in release too
        this.ref_count.ref_count += 1;
    }

    fn decreaseRefCount(this: *RefCounted) void {
        this.ref_count.ref_count -= 1;

        if (this.ref_count.ref_count == 0) {
            this.vtable.release(this);
        }
    }

    fn getWeakRef(this: *RefCounted, kind: ObjectType) !*WeakRef {
        // TODO: maybe we can reasonably type this ourselves?
        if (this.weak_ref == null) {
            this.weak_ref = try .create(this.gc, kind, this);
        }
        return this.weak_ref.?;
    }
};

const WeakRef = extern struct {
    ref_counted: RefCounted,
    object: ObjectPtr,

    const vtable: RefCounted.VTable = .{
        .destructor = RefCounted.destructor,
        .release = release,
    };

    fn create(gc: *GC, kind: ObjectType, obj: *RefCounted) !*WeakRef {
        const r: *WeakRef = try gc.allocator.create(WeakRef);
        r.* = .{
            .ref_counted = .{ .vtable = &vtable, .gc = gc },

            .object = .{ .type = kind, .value = std.mem.zeroes(ObjectPtr.Value) },
        };
        r.object.value.ref_counted = obj;

        return r;
    }

    fn invalidate(self: *WeakRef) void {
        self.object = .null;
    }

    fn release(ptr: *anyopaque) callconv(.c) void {
        const self: *WeakRef = @ptrCast(@alignCast(ptr));
        self.ref_counted.vtable.destructor(self);

        self.ref_counted.gc.allocator.destroy(self);
    }
};

const String = extern struct {
    ref_counted: RefCounted,

    data: [*]u8,
    len: usize,

    fn create(gc: *GC, s: []const u8) !*String {
        const self = try gc.allocator.create(String);
        errdefer gc.allocator.destroy(self);

        const str = try gc.allocator.alloc(u8, s.len);
        @memcpy(str, s);

        self.* = .{
            .ref_counted = .{ .vtable = &vtable, .gc = gc },

            .data = str.ptr,
            .len = str.len,
        };
        return self;
    }

    const vtable: RefCounted.VTable = .{
        .destructor = destructor,
        .release = release,
    };

    fn destructor(ptr: *anyopaque) callconv(.c) void {
        // const self: *String = @ptrCast(@alignCast(ptr));
        RefCounted.destructor(ptr);
    }

    fn release(ptr: *anyopaque) callconv(.c) void {
        const self: *String = @ptrCast(@alignCast(ptr));
        self.ref_counted.vtable.destructor(self);

        self.ref_counted.gc.allocator.free(self.data[0..self.len]);
        self.ref_counted.gc.allocator.destroy(self);
    }

    fn getValue(self: *String) []u8 {
        return self.data[0..self.len];
    }
};

const Collectable = extern struct {
    ref_counted: RefCounted,

    next: ?*Collectable = null,
    prev: ?*Collectable = null,

    const vtable: RefCounted.VTable = .{
        .destructor = RefCounted.destructor,
        .release = release,
    };

    fn release(p: *anyopaque) callconv(.c) void {
        _ = p;
    }
};

const Array = extern struct {
    collectable: Collectable,
    values: [*]ObjectPtr,
    cap: usize,

    fn create(gc: *GC, cap: usize) !*Array {
        _ = gc;
        _ = cap;
        unreachable;
    }

    fn constructor(self: *Array) void {
        _ = self;
    }
};

export fn test1() void {
    var debug_allocator: std.heap.DebugAllocator(.{}) = .init;
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

    var gc: GC = .init(gpa);
    const s: *String = String.create(&gc, "some funny string") catch unreachable;
    defer s.ref_counted.decreaseRefCount();

    const w = s.ref_counted.getWeakRef(.string) catch unreachable;
    defer w.ref_counted.decreaseRefCount();

    const b: ObjectPtr = .boolean(true);
    _ = b;

    std.debug.print("s = {s}\n", .{w.object.value.string.getValue()});
}
