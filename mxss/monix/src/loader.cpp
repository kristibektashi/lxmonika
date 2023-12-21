#include <array>
#include <cstddef>
#include <cstdint>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>

#include <monika_constants.h>

template <typename... Args>
inline static
intptr_t
LinuxSyscall(
    intptr_t number,
    Args... args
)
{
    std::array argsArray = { (intptr_t)args... };

    static_assert(argsArray.size() <= 6, "Too many arguments for a Linux syscall.");

#define SET_REGISTER_IF_PRESENT(reg, index)         \
    do                                              \
    {                                               \
        if constexpr (argsArray.size() > index)     \
        {                                           \
            reg = argsArray[index];                 \
        }                                           \
    }                                               \
    while (0)

#ifdef __x86_64__
    // DON'T intialize these registers, otherwise
    // the compiler will generate additional code to zero them out!
    register intptr_t rax asm("rax");
    register intptr_t rdi asm("rdi");
    register intptr_t rsi asm("rsi");
    register intptr_t rdx asm("rdx");
    register intptr_t r10 asm("r10");
    register intptr_t r8 asm("r8");
    register intptr_t r9 asm("r9");

    // The macros below expand to `if constexpr` statements.
    // The registers will only be attached to existing arguments.
    rax = number;
    SET_REGISTER_IF_PRESENT(rdi, 0);
    SET_REGISTER_IF_PRESENT(rsi, 1);
    SET_REGISTER_IF_PRESENT(rdx, 2);
    SET_REGISTER_IF_PRESENT(r10, 3);
    SET_REGISTER_IF_PRESENT(r8, 4);
    SET_REGISTER_IF_PRESENT(r9, 5);

    asm volatile(
        "syscall"
        : "+r"(rax)
        : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );

    return rax;
#elifdef __i386__
    register intptr_t eax asm("eax");
    register intptr_t ebx asm("ebx");
    register intptr_t ecx asm("ecx");
    register intptr_t edx asm("edx");
    register intptr_t esi asm("esi");
    register intptr_t edi asm("edi");
    register intptr_t ebp asm("ebp");

    eax = number;
    SET_REGISTER_IF_PRESENT(ebx, 0);
    SET_REGISTER_IF_PRESENT(ecx, 1);
    SET_REGISTER_IF_PRESENT(edx, 2);
    SET_REGISTER_IF_PRESENT(esi, 3);
    SET_REGISTER_IF_PRESENT(edi, 4);
    SET_REGISTER_IF_PRESENT(ebp, 5);

    asm volatile(
        "int $0x80"
        : "+a"(eax)
        : "b"(ebx), "c"(ecx), "d"(edx), "S"(esi), "D"(edi), "g"(ebp)
        : "memory"
    );

    return eax;
#elifdef __aarch64__
    register intptr_t x8 asm("x8");
    register intptr_t x0 asm("x0");
    register intptr_t x1 asm("x1");
    register intptr_t x2 asm("x2");
    register intptr_t x3 asm("x3");
    register intptr_t x4 asm("x4");
    register intptr_t x5 asm("x5");

    x8 = number;
    SET_REGISTER_IF_PRESENT(x0, 0);
    SET_REGISTER_IF_PRESENT(x1, 1);
    SET_REGISTER_IF_PRESENT(x2, 2);
    SET_REGISTER_IF_PRESENT(x3, 3);
    SET_REGISTER_IF_PRESENT(x4, 4);
    SET_REGISTER_IF_PRESENT(x5, 5);

    asm volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory"
    );

    return x0;
#elifdef __arm__
    // This code only supports arm thumb code.
    // Without thumb mode, r7 is reserved as the frame pointer and using it for the syscall may
    // make some compilers complain.
    // See https://github.com/bminor/musl/blob/master/arch/arm/syscall_arch.h for more details.
    // Since Windows on ARM only supports thumb mode
    // (https://learn.microsoft.com/en-us/cpp/build/overview-of-arm-abi-conventions), this should
    // not be a problem.
    register intptr_t r7 asm("r7");
    register intptr_t r0 asm("r0");
    register intptr_t r1 asm("r1");
    register intptr_t r2 asm("r2");
    register intptr_t r3 asm("r3");
    register intptr_t r4 asm("r4");
    register intptr_t r5 asm("r5");

    r7 = number;
    SET_REGISTER_IF_PRESENT(r0, 0);
    SET_REGISTER_IF_PRESENT(r1, 1);
    SET_REGISTER_IF_PRESENT(r2, 2);
    SET_REGISTER_IF_PRESENT(r3, 3);
    SET_REGISTER_IF_PRESENT(r4, 4);
    SET_REGISTER_IF_PRESENT(r5, 5);

    asm volatile(
        "swi #0"
        : "+r"(r0)
        : "r"(r7), "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
        : "memory"
    );

    return r0;
#else
#error Write the syscall code for this architecture!
#endif

}

[[noreturn]]
inline static
void LinuxFail(
    const char* message,
    size_t messageSize,
    int status
)
{
    char statusString[32] = ": ";
    int length = 2;

    status = -status;

    do
    {
        statusString[length] = (status % 10) + '0';
        status /= 10;
        ++length;
    }
    while (status != 0);

    statusString[length] = '\n';
    ++length;

    LinuxSyscall(SYS_write, 2, message, messageSize);
    LinuxSyscall(SYS_write, 2, statusString, length);
    LinuxSyscall(SYS_exit, status);

    __builtin_unreachable();
}

const char ProviderName[MA_NAME_MAX] = "Monix";

// Monix uses the same ABI.
#define MonixSyscall LinuxSyscall

// https://github.com/itsmevjnk/sysx/blob/main/exec/syscall.h
/* syscall function numbers */
#define SYSCALL_EXIT                            0 // arg1 = return code
#define SYSCALL_READ                            1 // arg1 = size, arg2 = buffer ptr, arg3 = fd
#define SYSCALL_WRITE                           2 // arg1 = size, arg2 = buffer ptr, arg3 = fd

#define STRING_AND_SIZE(str) (str), (sizeof(str) - 1)

extern "C"
__attribute__((force_align_arg_pointer))
void _start()
{
    int status;

    status = LinuxSyscall(SYS_write, 1,
        STRING_AND_SIZE("Bootstrapping the container with Monix loader...\n"));
    if (status < 0)
    {
        LinuxFail(STRING_AND_SIZE("Cannot write Hello World"), status);
    }

    status = LinuxSyscall(SYS_mknod, "/dev/reality",
        // Writing to /dev/reality is quite dangerous, so let us make it read-only.
        S_IFCHR | (S_IRUSR | S_IRGRP | S_IROTH),
        makedev(10, MA_REALITY_MINOR)
    );
    if (status < 0 && status != -EEXIST)
    {
        LinuxFail(STRING_AND_SIZE("Cannot create the reality device"), status);
    }

    status = LinuxSyscall(SYS_open, "/dev/reality", O_RDONLY);
    if (status < 0)
    {
        LinuxFail(STRING_AND_SIZE("Cannot open the reality device"), status);
    }
    int fd = status;

    // Now that we have the handle to the reality device, we can set up our container.
    status = LinuxSyscall(SYS_chroot, "/rootfs/");
    if (status < 0)
    {
        LinuxFail(STRING_AND_SIZE("Cannot change root"), status);
    }

    status = LinuxSyscall(SYS_chdir, "/");
    if (status < 0)
    {
        LinuxFail(STRING_AND_SIZE("Cannot change directory"), status);
    }

    status = LinuxSyscall(SYS_ioctl, fd, MA_IOCTL_SET_PROVIDER, ProviderName);
    if (status < 0)
    {
        LinuxFail(STRING_AND_SIZE("Cannot set the provider name"), status);
    }

    // Now, we are in the Monix world. Linux syscalls will not work anymore.

    MonixSyscall(SYSCALL_WRITE, 1, STRING_AND_SIZE("Hello, Monix World!\n"));

    MonixSyscall(SYSCALL_EXIT, -1);

    // lxmonika should bring us back to WSL at this point.
    LinuxSyscall(SYS_write, 1, STRING_AND_SIZE("Monix container exited.\n"));
    LinuxSyscall(SYS_exit, 0);
}
