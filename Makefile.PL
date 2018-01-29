use strict;
use warnings;
use inc::Module::Install;
use Module::Install::XSUtil; # for co-developpers
use constant {
    DEBUG => 1,
    EMCC  => 1
};

use_ppport 3.19;
all_from 'lib/Compiler/CodeGenerator/LLVM.pm';
requires_cplusplus;
cc_warnings;
cc_src_paths qw(src);
cc_include_paths qw(include);
cc_define qw();
makemaker_args->{CC} = 'g++';
makemaker_args->{LD} = 'g++';
my @ignore_warnings_options = qw(missing-field-initializers unused-variable);
makemaker_args->{CCFLAGS} .= join('', map { ' -Wno-' . $_ } @ignore_warnings_options);
chomp(my $bin_dir = `llvm-config --bindir`);
my $clang = "$bin_dir/clang";
die "could not find clang" unless ($clang);

my $llvm_lib_dir = 'lib/Compiler/CodeGenerator/LLVM';
`$clang -emit-llvm -S -o $llvm_lib_dir/runtime_api.ll gen/runtime_api.c`;
`$clang -emit-llvm -arch i386 -S -o $llvm_lib_dir/runtime_api_32.ll gen/runtime_api_32.c`;
if (EMCC) {
    `$clang -target wasm32-unknown-unknown-elf -D__EMSCRIPTEN_major__=1 -D__EMSCRIPTEN_minor__=37 -D__EMSCRIPTEN_tiny__=28 -D_LIBCPP_ABI_VERSION=2 -D__EMSCRIPTEN__ -Dunix -D__unix -D__unix__ -Werror=implicit-function-declaration -nostdinc -Xclang -nobuiltininc -Xclang -nostdsysteminc -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/include/libcxx -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/lib/libcxxabi/include -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/include/compat -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/include -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/include/SSE -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/include/libc -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/lib/libc/musl/arch/emscripten -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/local/include -Oz -fno-inline-functions -Xclang -isystem/usr/local/Cellar/emscripten/HEAD-941e9de/libexec/system/include/SDL -emit-llvm -S -o $llvm_lib_dir/runtime_api_emcc.ll gen/runtime_api.c`;
}
`cp gen/*.h  $llvm_lib_dir`;

if (DEBUG) {
    makemaker_args->{OPTIMIZE} = '-O0';
    makemaker_args->{CCFLAGS} .= ' -g3';
}

chomp(my $llvm_cflags = `llvm-config --cxxflags`);
$llvm_cflags =~ s/-fno-rtti//;
chomp(my $llvm_ldflags = `llvm-config --ldflags`);
chomp(my $llvm_libs = `llvm-config --libs`);
chomp(my  $llvm_version = `llvm-config --version`);
my ($captured_version) = $llvm_version =~ /([0-9.]+)/;
$captured_version =~ s/\./_/g;

makemaker_args->{CCFLAGS} .= " $llvm_cflags -DLLVM_VERSION_$captured_version";
makemaker_args->{LDLOADLIBS} = "$llvm_ldflags $llvm_libs -lLLVMIRReader";

WriteAll(check_nmake => 0);
