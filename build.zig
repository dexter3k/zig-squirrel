const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const base_c_flags: []const []const u8 = &.{
        "-Wall",
        "-Wextra",
        "-Werror",
        "-fno-exceptions",
        "-fno-rtti",
        // "-Wcast-align",
        "-Wstrict-aliasing",
        "-fno-strict-aliasing",
        "-ferror-limit=1",
    };

    // Main Squirrel lib
    const squirrel_lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
        .root_source_file = b.path("squirrel/root.zig"),
    });
    squirrel_lib_mod.addCMacro("_SQ64", "1");
    // squirrel_lib_mod.addCMacro("_DEBUG_DUMP", "1");
    squirrel_lib_mod.addIncludePath(b.path("include/"));
    squirrel_lib_mod.addIncludePath(b.path("squirrel/"));
    squirrel_lib_mod.addCSourceFiles(.{
        .root = b.path("squirrel/"),
        .files = &.{
            "strtab.c",

            "sqapi.cpp",
            "sqfuncstate.cpp",
            "sqdebug.cpp",
            "sqobject.cpp",
            "sqcompiler.cpp",
            "sqstate.cpp",
            "sqmem.c",
            "sqvector.hpp",

            "GC.hpp",
            "GC.cpp",
            "RefTable.hpp",
            "RefTable.cpp",

            "SQArray.hpp",
            "SQArray.cpp",
            "SQClass.hpp",
            "SQClass.cpp",
            "SQClosure.hpp",
            "SQClosure.cpp",
            "SQCollectable.hpp",
            "SQCollectable.cpp",
            "SQDelegable.hpp",
            "SQDelegable.cpp",
            "SQFunctionProto.hpp",
            "SQFunctionProto.cpp",
            "SQGenerator.hpp",
            "SQGenerator.cpp",
            "SQInstance.hpp",
            "SQInstance.cpp",
            "SQNativeClosure.hpp",
            "SQNativeClosure.cpp",
            "SQOuter.hpp",
            "SQOuter.cpp",
            "SQString.hpp",
            "SQString.cpp",
            "SQTable.hpp",
            "SQTable.cpp",
            "SQUserData.hpp",
            "SQUserData.cpp",
            "SQVM.hpp",
            "SQVM.cpp",

            "builtins.hpp",
            "builtins.cpp",
            "builtins_array.cpp",
            "builtins_base.cpp",
            "builtins_class.cpp",
            "builtins_closure.cpp",
            "builtins_generator.cpp",
            "builtins_instance.cpp",
            "builtins_number.cpp",
            "builtins_string.cpp",
            "builtins_table.cpp",
            "builtins_thread.cpp",
            "builtins_weakref.cpp",
        },
        .flags = base_c_flags,
    });
    const squirrel_lib = b.addLibrary(.{
        .linkage = .static,
        .name = "squirrel",
        .root_module = squirrel_lib_mod,
    });
    b.installArtifact(squirrel_lib);

    // Squirrel stdlib
    const sqstdlib_lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });
    sqstdlib_lib_mod.addCMacro("_SQ64", "1");
    // sqstdlib_lib_mod.addCMacro("_DEBUG_DUMP", "1");
    sqstdlib_lib_mod.addIncludePath(b.path("include/"));
    sqstdlib_lib_mod.addCSourceFiles(.{
        .root = b.path("sqstdlib/"),
        .files = &.{
            "sqstdblob.cpp",
            "sqstdio.cpp",
            "sqstdstream.cpp",
            "sqstdmath.cpp",
            "sqstdsystem.cpp",
            "sqstdstring.cpp",
            "sqstdaux.cpp",
            "sqstdrex.cpp",
        },
        .flags = base_c_flags,
    });
    const sqstdlib_lib = b.addLibrary(.{
        .linkage = .static,
        .name = "sqstdlib",
        .root_module = sqstdlib_lib_mod,
    });
    b.installArtifact(sqstdlib_lib);

    // Interpreter exe
    const sq_exe_mod = b.createModule(.{
        .root_source_file = b.path("sq/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    sq_exe_mod.addCMacro("_SQ64", "1");
    // sq_exe_mod.addCMacro("_DEBUG_DUMP", "1");
    sq_exe_mod.addIncludePath(b.path("include/"));
    sq_exe_mod.addCSourceFiles(.{
        .root = b.path("sq/"),
        .files = &.{
            "sq.c",
        },
        .flags = base_c_flags,
    });
    sq_exe_mod.linkLibrary(squirrel_lib);
    sq_exe_mod.linkLibrary(sqstdlib_lib);
    const sq_exe = b.addExecutable(.{
        .name = "sq",
        .root_module = sq_exe_mod,
    });
    b.installArtifact(sq_exe);

    const run_cmd = b.addRunArtifact(sq_exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the interpreter");
    run_step.dependOn(&run_cmd.step);

    // const sq_exe_unit_tests = b.addTest(.{
    //     .root_module = sq_exe_mod,
    // });
    // const run_sq_exe_unit_tests = b.addRunArtifact(sq_exe_unit_tests);

    // const test_step = b.step("test", "Run unit tests");
    // // test_step.dependOn(&run_lib_unit_tests.step);
    // test_step.dependOn(&run_sq_exe_unit_tests.step);
}
