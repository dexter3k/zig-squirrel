const std = @import("std");

pub fn Squirrel(comptime int_type: type, comptime float_type: type, comptime mark_and_sweep: bool) type {
    return struct {
        pub const Integer = int_type;
        pub const Float = float_type;

        pub const ObjectType = enum(u32) {
            null = 0x00000001,
            integer = 0x00000002,
            float = 0x00000004,
            bool = 0x00000008,
            string = 0x00000010,
            table = 0x00000020,
            array = 0x00000040,
            userdata = 0x00000080,
            closure = 0x00000100,
            nativeclosure = 0x00000200,
            generator = 0x00000400,
            userpointer = 0x00000800,
            thread = 0x00001000,
            funcproto = 0x00002000,
            class = 0x00004000,
            instance = 0x00008000,
            weakref = 0x00010000,
            outer = 0x00020000,

            pub fn can_be_false(t: ObjectType) bool {
                return @intFromEnum(t) & (@intFromEnum(ObjectType.null) |
                    @intFromEnum(ObjectType.integer) |
                    @intFromEnum(ObjectType.float) |
                    @intFromEnum(ObjectType.bool)) != 0;
            }

            pub fn is_numeric(t: ObjectType) bool {
                return @intFromEnum(t) & (@intFromEnum(ObjectType.integer) |
                    @intFromEnum(ObjectType.float)) != 0;
            }

            pub fn is_ref_counted(t: ObjectType) bool {
                return @intFromEnum(t) & (@intFromEnum(ObjectType.string) |
                    @intFromEnum(ObjectType.table) |
                    @intFromEnum(ObjectType.array) |
                    @intFromEnum(ObjectType.userdata) |
                    @intFromEnum(ObjectType.closure) |
                    @intFromEnum(ObjectType.nativeclosure) |
                    @intFromEnum(ObjectType.generator) |
                    @intFromEnum(ObjectType.thread) |
                    @intFromEnum(ObjectType.funcproto) |
                    @intFromEnum(ObjectType.class) |
                    @intFromEnum(ObjectType.instance) |
                    @intFromEnum(ObjectType.weakref) |
                    @intFromEnum(ObjectType.outer)) != 0;
            }

            pub fn is_delegate(t: ObjectType) bool {
                return @intFromEnum(t) & (@intFromEnum(ObjectType.table) |
                    @intFromEnum(ObjectType.userdata) |
                    @intFromEnum(ObjectType.instance)) != 0;
            }
        };

        pub const ObjectValue = packed union {
            // Inlined types
            null: void,
            integer: Integer,
            float: Float,

            // Objects
            thread: *VM,

            // Parent types
            chainable: *Chainable,
        };

        pub const ObjectPtr = packed struct {
            value: ObjectValue,
            type: ObjectType,

            pub const @"null": ObjectPtr = .{ .type = .null, .value = .{ .null = {} } };

            pub fn @"bool"(v: bool) ObjectPtr {
                return .{ .type = .bool, .value = .{ .integer = @intFromBool(v) } };
            }

            pub fn integer(v: Integer) ObjectPtr {
                return .{ .type = .integer, .value = .{ .integer = v } };
            }

            pub fn thread(vm: *VM) ObjectPtr {
                vm.chainable.counter += 1;
                return .{ .type = .thread, .value = .{ .thread = vm } };
            }

            pub fn release(p: *ObjectPtr) void {
                if (p.type.is_ref_counted()) {
                    p.value.chainable.release();
                }
                p.* = .null;
            }
        };

        const RefCounted = packed struct {
            const VTable = struct {
                finalize: *const fn (*anyopaque) void,
            };

            vtable: *const VTable,
            counter: usize = 0,
            weak_ref: ?*WeakRef = null,

            fn release(self: *RefCounted) void {
                self.counter -= 1;
                // std.debug.assert(self.counter >= 0);
                if (self.counter == 0 and self.vtable.finalize != null) {
                    self.vtable.finalize(self);
                }
            }
        };

        const Chainable = if (mark_and_sweep)
            @compileError("Mark and sweep not implemented")
        else
            RefCounted;

        const WeakRef = packed struct {
            ref_counted: RefCounted,
        };

        const State = struct {
            allocator: std.mem.Allocator,
            root_vm: ObjectPtr,

            fn init(allocator: std.mem.Allocator) !*State {
                const state = try allocator.create(State);
                state.* = .{
                    .allocator = allocator,
                    .root_vm = .null,
                };
                return state;
            }

            fn deinit(state: *State) void {
                state.allocator.destroy(state);
            }
        };

        pub const VM = packed struct {
            pub const Config = struct {
                initial_stack: usize = 1024,
            };

            var vtable_vm: Chainable.VTable = .{
                .finalize = finalize,
            };

            chainable: Chainable,

            state: *State,

            fn passthrough(vm: *Chainable.VTable) *Chainable.VTable {
                // std.debug.print("Setting vm {*}\n", .{vm});
                return vm;
            }

            pub fn init(allocator: std.mem.Allocator, config: Config) !*VM {
                _ = config;

                const state: *State = try .init(allocator);
                errdefer state.deinit();

                const vm: *VM = try state.allocator.create(VM);
                vm.* = .{
                    .chainable = .{ .vtable = &vtable_vm },
                    .state = state,
                };
                vm.chainable.vtable = &vtable_vm; // works when I do this?

                // const tmp: Chainable = .{ .vtable = &vtable };
                std.debug.print("THE POINTER IS {*}\n", .{vm.chainable.vtable});

                state.root_vm = .thread(vm);

                return vm;
            }

            pub fn deinit(vm: *VM) void {
                vm.state.deinit();
            }

            pub fn finalize(p: *anyopaque) void {
                _ = p;
            }
        };
    };
}
