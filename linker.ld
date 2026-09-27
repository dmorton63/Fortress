OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS {
    . = 0xffffffff80000000;

    . = ALIGN(0x1000);
    .text : {
        *(.text .text.*)
    }

    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(0x1000);
    .data : {
        *(.data .data.*)
        *(.init_array .init_array.*)
    }

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    }

    .limine_requests_start_marker : {
        KEEP(*(.limine_requests_start_marker))
    }

    .limine_requests : {
        KEEP(*(.limine_requests))
    }

    .limine_requests_end_marker : {
        KEEP(*(.limine_requests_end_marker))
    }
}
