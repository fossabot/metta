//
// Copyright 2007 - 2009, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "memutils.h"
#include "memory.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "default_console.h"
#include "elf_parser.h"
#include "registers.h"
#include "initfs.h"
#include "mmu.h"
#include "c++ctors.h"
// #include "bootinfo.h"
#include "debugger.h"
#include "linksyms.h"
#include "frame.h"
// #include "kickstart_frame_allocator.h"
// #include "nucleus.h"
#include "config.h"
// #include "page_fault_handler.h"
// -- new includes --
#include "x86_frame_allocator.h"
#include "new.h"

// page_fault_handler_t page_fault_handler;
// interrupt_descriptor_table_t interrupts_table;
// kickstart_frame_allocator_t frame_allocator;

extern "C" void kickstart(multiboot_t::header_t* mbh);
extern "C" address_t placement_address;
extern "C" address_t KICKSTART_BASE;

static void map_identity(const char* caption, address_t start, address_t end)
{
#if MEMORY_DEBUG
    kconsole << "Mapping " << caption << endl;
#endif
    end = page_align_up<address_t>(end); // one past end
    for (uint32_t k = start/PAGE_SIZE; k < end/PAGE_SIZE; k++)
        ;
//         protection_domain_t::privileged().map(k * PAGE_SIZE, k * PAGE_SIZE, 0);
}

// Scratch area for initial pagedir
uint32_t pagedir_area[1024] ALIGNED(4096);

/*!
 * Get the system going.
 */
void kickstart(multiboot_t::header_t* mbh)
{
    // No dynamic memory allocation here yet, global objects not constructed either.
    multiboot_t mb(mbh);

    ASSERT(mb.module_count() > 0);
    multiboot_t::modinfo_t* bootimage = mb.module(0);
    ASSERT(bootimage);

    x86_frame_allocator_t::set_allocation_start(page_align_up<address_t>(std::max(LINKSYM(placement_address), bootimage->mod_end)));
    // now we can allocate memory frames

    kconsole << "Run global ctors" << endl;
    run_global_ctors();

    x86_frame_allocator_t::instance().initialise_before_paging(mb.memory_map(), x86_frame_allocator_t::instance().reserved_range());
    // now we can also free dynamic memory

//     pagedir.init(pagedir_area);
    // now we can create page mappings

#if MEMORY_DEBUG
    kconsole << GREEN << "lower mem = " << (int)mb.lower_mem() << "KB" << endl
                      << "upper mem = " << (int)mb.upper_mem() << "KB" << endl;

    kconsole << WHITE << "We are loaded at " << LINKSYM(KICKSTART_BASE) << endl
                      << "    bootimage at " << bootimage->mod_start << ", end " << bootimage->mod_end << endl;
#endif

//     address_t bootinfo_page = reinterpret_cast<address_t>(new frame_t);
//     bootinfo_t bootinfo(bootinfo_page);
//     bootinfo.init_from(mb);
//     mb.set_header(bootinfo.multiboot_header());

    // Identity map currently executing code.
    // page 0 is not mapped to catch null pointers
    map_identity("bottom 4Mb", PAGE_SIZE, 4*MiB - PAGE_SIZE);

//     kconsole << endl << "Mapped." << endl;
// 
    global_descriptor_table_t gdt;
    kconsole << "Created gdt." << endl;

//     interrupts_table.set_isr_handler(14, &page_fault_handler);
//     interrupts_table.install();

//     ia32_mmu_t::set_active_pagetable(pagedir.get_physical());
//     ia32_mmu_t::enable_paged_mode();
    // now we have paging enabled.

//     elf_loader_t kernel_loader;
//     if (!kernel_loader.load_image(kernel->mod_start, kernel->mod_end - kernel->mod_start))
//         kconsole << RED << "kernel NOT loaded (bad)" << endl;
//     else
//         kconsole << GREEN << "kernel loaded (ok)" << endl;
// 
//     typedef nucleus_n::nucleus_t* (*kernel_entry)(bootinfo_t bi_page);
//     kernel_entry init_nucleus = reinterpret_cast<kernel_entry>(kernel_loader.get_entry_point());

//     kconsole << RED << "going to init nucleus" << endl;
//     nucleus_n::nucleus_t* nucleus = init_nucleus(bootinfo);
//     kconsole << GREEN << "done, instantiating components" << endl;
// 
//     kconsole << GREEN << "getting allocator" << endl;
//     frame_allocator_t* fa = &nucleus->mem_mgr().page_frame_allocator();
//     kconsole << GREEN << "setting allocator " << fa << endl;
//     frame_t::set_frame_allocator(fa);
//     kconsole << "set allocator" << endl;

//     kconsole << "pagedir @ " << nucleus->mem_mgr().get_current_directory() << endl;
//     nucleus->mem_mgr().get_current_directory()->dump();

    // Load components from bootcp.
//     kconsole << "opening initfs @ " << bootimage->mod_start << endl;
//     initfs_t initfs(bootcp->mod_start);
//     typedef void (*comp_entry)(bootinfo_t bi_page);

    // For now, boot components are all linked at different virtual addresses so they don't overlap.
//     kconsole << "iterating components" << endl;
//     for (uint32_t k = 0; k < initfs.count(); k++)
//     {
//         kconsole << YELLOW << "boot component " << initfs.get_file_name(k) << " @ " << initfs.get_file(k) << endl;
//         // here loading an image should create a separate PD with its own pagedir
// //         domain_t* d = nucleus->create_domain();
// 
//         if (!elf.load_image(initfs.get_file(k), initfs.get_file_size(k)))
//             kconsole << RED << "not an ELF file, load failed" << endl;
//         else
//             kconsole << GREEN << "ELF file loaded, entering component init" << endl;
//         comp_entry init_component = (comp_entry)elf.get_entry_point();
//         init_component(bootinfo);
//     }

    kconsole << WHITE << "...in the living memory of V2_OS" << endl;

    // TODO: run a predefined root_server_entry portal here

    /* Never reached */
    PANIC("root_server returned!");
}

// kate: indent-width 4; replace-tabs on;
// vim: set et sw=4 ts=4 sts=4 cino=(4 :
