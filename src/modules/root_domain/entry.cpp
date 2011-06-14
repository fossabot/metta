#include "default_console.h"
#include "frames_module_v1_interface.h"
#include "mmu_v1_interface.h"
#include "mmu_module_v1_interface.h"
#include "heap_module_v1_interface.h"
#include "pervasives_v1_interface.h"
#include "system_stretch_allocator_v1_interface.h"
#include "stretch_driver_module_v1_interface.h"
#include "stretch_table_module_v1_interface.h"
#include "macros.h"
#include "c++ctors.h"
#include "root_domain.h"
#include "bootinfo.h"
#include "new.h"
#include "elf_parser.h"
#include "debugger.h"
#include "module_loader.h"
#include "infopage.h"

// bootimage contains modules and namespaces
// each module has an associated namespace which defines some module attributes/parameters.
// startup module from which root_domain starts also has a namespace called "default_namespace"
// it defines general system attributes and startup configuration.

#include "mmu_module_v1_impl.h" // for debug

static pervasives_v1_rec pervasives;

//======================================================================================================================
// Look up in root_domain's namespace and load a module by given name, satisfying its dependencies, if possible.
//======================================================================================================================

/// tagged_t namesp.find(string key)

// with module_loader_t loading any modules twice is safe, and if we track module dependencies then only what is needed
// will be loaded.
static void* load_module(bootimage_t& bootimg, const char* module_name, const char* clos)
{
    bootimage_t::modinfo_t addr = bootimg.find_module(module_name);
    if (!addr.start)
        return 0;

    kconsole << " + Found module " << module_name << " at address " << addr.start << " of size " << addr.size << endl;

    bootinfo_t* bi = new(BOOTINFO_PAGE) bootinfo_t(false);
    elf_parser_t loader(addr.start);
    return bi->get_module_loader().load_module(module_name, loader, clos);
    /* FIXME: Skip dependencies for now */
}

template <class closure_type>
static inline closure_type* load_module(bootimage_t& bootimg, const char* module_name, const char* clos)
{
    return static_cast<closure_type*>(load_module(bootimg, module_name, clos));
}

//======================================================================================================================
// setup MMU and frame allocator
//======================================================================================================================

static void init_mem(bootimage_t& bootimg)
{
    kconsole << " + init_mem" << endl;

    // Load modules used for booting before we overwrite them.
    auto frames_mod = load_module<frames_module_v1_closure>(bootimg, "frames_mod", "exported_frames_module_rootdom")
    ASSERT(frames_mod);

    auto mmu_mod = load_module<mmu_module_v1_closure>(bootimg, "mmu_mod", "exported_mmu_module_rootdom");
    ASSERT(mmu_mod);

    auto heap_mod = load_module<heap_module_v1_closure>(bootimg, "heap_mod", "exported_heap_module_rootdom");
    ASSERT(heap_mod);

    auto stretch_allocator = load_module<system_stretch_allocator_v1_closure>(bootimg, "stretch_allocator_mod", "exported_system_stretch_allocator_rootdom");
    ASSERT(stretch_mod);
    
    auto stretch_table_mod = load_module<stretch_table_module_v1_closure>(bootimg, "stretch_table_mod", "exported_stretchtbl_module_rootdom");
    ASSERT(stretch_table_mod);

    auto stretch_driver_mod = load_module<stretch_driver_module_v1_closure>(bootimg, "stretch_driver_mod", "exported_stretch_driver_module_rootdom");
    ASSERT(stretch_driver_mod);

// FIXME: point of initial reservation is so that MMU_mod would configure enough pagetables to accomodate initial v2p mappings!
    // request necessary space for frames allocator
    int required = frames_mod->required_size();
    int initial_heap_size = 128*KiB;

    ramtab_v1_closure* rtab;
    memory_v1_address next_free;

    kconsole << " + Init memory region size " << int(required + initial_heap_size) << " bytes." << endl;
    mmu_v1_closure* mmu = mmu_mod->create(required + initial_heap_size, &rtab, &next_free);
    UNUSED(mmu);

//FIXME: we've just overwritten our bootimage, congratulations, gentlemen! Finding heap module will be fun.

    kconsole << " + Obtained ramtab closure @ " << rtab << ", next free " << next_free << endl;

    kconsole << " + Creating frame allocator" << endl;
    auto frames = frames_mod->create(rtab, next_free);

    kconsole << " + Creating heap" << endl;
    auto heap = heap_mod->create_raw(next_free + required, initial_heap_size);
    PVS(heap) = heap;

    frames_mod->finish_init(frames, heap);

    kconsole << " + Heap alloc test:";
    for (size_t counter = 0; counter < 100000; ++counter)
    {
        address_t p = heap->allocate(counter);
        heap->free(p);
    }
    kconsole << " done." << endl;

    kconsole << " + Frames new_client test" << endl;
    frames->create_client(0, 0, 20, 20, 20);
}
#if 0
    // create stretch allocator
    kconsole << " + Creating stretch allocator" << endl;
    // assign stretches to address ranges
    auto salloc = init_virt_mem(salloc_mod, memmap, heap, mmu);
    PVS(strech_allocator) = salloc;

    /*
     * We create a 'special' stretch allocator which produces stretches
     * for page tables, protection domains, DCBs, and so forth.
     * What 'special' means will vary from architecture to architecture,
     * but it will typcially imply at least that the stretches will
     * be backed by phyiscal memory on creation.
     */
    auto sysalloc = salloc->create_nailed(frames, heap);

    mmu_mod->finished(mmu, frames, heap, sysalloc);

    auto strtab = stretch_table_mod->create(heap);
    PVS(stretch_driver) = stretch_driver_mod->create_null(heap, strtab);

    // Create the initial address space; returns a pdom for Nemesis domain.
    kconsole << " + Creating initial address space." << endl;
    nemesis_pdid = CreateAddressSpace(frames, mmu, salloc, nexusp);
    MapInitialHeap(heap_mod, heap, initial_heap_size, nemesis_pdid);
}

//init_virt_mem
StretchAllocatorF_cl *InitVMem(SAllocMod_cl *smod, Mem_Map memmap, 
			       Heap_cl *h, MMU_cl *mmu)
{
    StretchAllocatorF_cl *salloc;
    Mem_VMemDesc allvm[2];
    Mem_VMemDesc used[32];
    int i;

    allvm[0].start_addr = 0;
    allvm[0].npages     = VA_SIZE >> PAGE_WIDTH;
    allvm[0].page_width = PAGE_WIDTH;
    allvm[0].attr       = 0;

    allvm[1].npages     = 0;  /* end of array marker */

    /* Determine the used vm from the mapping info */
    for(i=0; memmap[i].nframes; i++) {
	
	used[i].start_addr  = memmap[i].vaddr;
	used[i].npages      = memmap[i].nframes;
	used[i].page_width  = memmap[i].frame_width; 
	used[i].attr        = 0;

    }

    /* now terminate the array */
    used[i].npages = 0;

    salloc = SAllocMod$NewF(smod, h, mmu, allvm, used);
    return salloc;
}

ProtectionDomain_ID CreateAddressSpace(Frames_clp frames, MMU_clp mmu, 
					StretchAllocatorF_clp sallocF, 
					nexus_ptr_t nexus_ptr)
{
    nexus_ptr_t          nexus_end;
    struct nexus_ntsc   *ntsc  = NULL;
    struct nexus_module *mod;
    struct nexus_blob   *blob;
    struct nexus_prog   *prog;
    struct nexus_primal *primal;
    struct nexus_nexus  *nexus;
    struct nexus_name   *name;
    struct nexus_EOI    *EOI;
    ProtectionDomain_ID  pdom;
    Stretch_clp          str;
    int                  map_index;

    primal    = nexus_ptr.nu_primal;
    nexus_end = nexus_ptr.generic;
    nexus_end.nu_primal++;	/* At least one primal */
    nexus_end.nu_ntsc++;	/* At least one ntsc */
    nexus_end.nu_nexus++;	/* At least one nexus */
    nexus_end.nu_EOI++;		/* At least one EOI */

    ETRC(eprintf("Creating new protection domain (mmu at %p)\n", mmu));
    pdom = MMU$NewDomain(mmu);

    /* Intialise the pdom map to zero */
    for (map_index=0; map_index < MAP_SIZE; map_index++) {
        addr_map[map_index].address= NULL;
        addr_map[map_index].str    = (Stretch_cl *)NULL;
    }
    map_index= 0;

    /* 
    ** Sort out memory before the boot address (platform specific)
    ** In MEMSYS_EXPT, this memory has already been marked as used 
    ** (both in virtual and physical address allocators) by the 
    ** initialisation code, so all we wish to do is to map some 
    ** stretches over certain parts of it. 
    ** Most of the relevant information is provided in the nexus, 
    ** although even before this we have the PIP. 
    */

    /* First we need to map the PIP globally read-only */
    str = StretchAllocatorF$NewOver(sallocF, PAGE_SIZE, AXS_GR, 
				    (addr_t)INFO_PAGE_ADDRESS, 
				    0, PAGE_WIDTH, NULL);
    ASSERT_ADDRESS(str, INFO_PAGE_ADDRESS);

    /* Map stretches over the boot image */
    TRC_MEM(eprintf("CreateAddressSpace (EXPT): Parsing NEXUS at %p\n", 
		nexus_ptr.generic));

    while(nexus_ptr.generic < nexus_end.generic)  {
	
	switch(*nexus_ptr.tag) {
	    
	  case nexus_primal:
	    primal = nexus_ptr.nu_primal++;
	    TRC_MEM(eprintf("PRIM: %lx\n", primal->lastaddr));
	    break;
	    
	  case nexus_ntsc:
	    ntsc = nexus_ptr.nu_ntsc++;
	    TRC_MEM(eprintf("NTSC: T=%06lx:%06lx D=%06lx:%06lx "
			    "B=%06lx:%06lx\n",
			    ntsc->taddr, ntsc->tsize,
			    ntsc->daddr, ntsc->dsize,
			    ntsc->baddr, ntsc->bsize));

        /* Listen up, dudes: here's the drill:
         *
         *   |    bss     |  read/write perms
         *   |------------|
         *   |   data     |-------------
         *   |------------|  read/write & execute perms  (hack page)
         *   |   text     |-------------   <- page boundary
         *   |            |
         *   |            |  read/execute perms
	     *
	     * Now, the text and data boundary of the NTSC is not
	     * necessarily page aligned, so there may or may not be a
	     * hack page overlapping it.
	     * The next few bits of code work out whether we need a
	     * hack page, and creates it.
	     */
	    if ((ntsc->daddr - ntsc->taddr) & (FRAME_SIZE-1))
	    {
		/* If NTSC text is over 1 page, need some text pages */
		if (ALIGN(ntsc->daddr - ntsc->taddr) - FRAME_SIZE != 0)
		{
		    str = StretchAllocatorF$NewOver(
			sallocF, ALIGN(ntsc->daddr - ntsc->taddr)-FRAME_SIZE, 
			AXS_GE, (addr_t)ntsc->taddr, 0, PAGE_WIDTH, NULL);
		    ASSERT_ADDRESS(str, ntsc->taddr);
		}

		/* create hack page */
		str = StretchAllocatorF$NewOver(
		    sallocF, FRAME_SIZE, AXS_NONE, 
		    (addr_t)(ALIGN(ntsc->daddr) - FRAME_SIZE), 0, 
		    PAGE_WIDTH, NULL);
		TRC_MEM(eprintf("       -- hack page at %06lx\n",
				ALIGN(ntsc->daddr) - FRAME_SIZE));
		ASSERT_ADDRESS(str, ALIGN(ntsc->daddr) - FRAME_SIZE);
		SALLOC_SETPROT(salloc, str, pdom,
					 SET_ELEM(Stretch_Right_Read) |
					 SET_ELEM(Stretch_Right_Write) |
					 SET_ELEM(Stretch_Right_Execute));
	    }
	    else
	    {
		/* no hack page needed */
		str = StretchAllocatorF$NewOver(sallocF, ntsc->tsize, AXS_GE,
						(addr_t)ntsc->taddr, 0, 
						PAGE_WIDTH, NULL);
		ASSERT_ADDRESS(str, ntsc->taddr);
	    }
	    break;
	    
	  case nexus_nexus:
	    nexus = nexus_ptr.nu_nexus++;
	    TRC_MEM(eprintf("NEX:  N=%06lx,%06lx IGNORING\n", 
			    nexus->addr, nexus->size));
	    nexus_end.generic = (addr_t)(nexus->addr + nexus->size);

	    /* XXX Subtlety - NEXUS tacked on the end of NTSC BSS */
	    /* 
	    ** XXX More subtlety; the NEXUS is always a page and `a bit', where
	    ** the bit is whatever's left from the end of the ntsc's bss upto
	    ** a page boundary, and the page is the one following that.
	    ** This is regardless of whether or not the nexus requires this 
	    ** space, and as such nexus->size can be misleading. Simplest 
            ** way to ensure we alloc enough mem for now is to simply 
	    ** use 1 page as a lower bound for the nexus size.
	    */
	    if ((ntsc->dsize + ntsc->bsize + MAX(nexus->size, FRAME_SIZE) -
		(ALIGN(ntsc->daddr) - ntsc->daddr)) > 0)
	    {
		str = StretchAllocatorF$NewOver(
		    sallocF, ntsc->dsize + ntsc->bsize + 
		    MAX(nexus->size, FRAME_SIZE) -
		    (ALIGN(ntsc->daddr) - ntsc->daddr) /* size */, 
		    AXS_NONE, (addr_t)ALIGN(ntsc->daddr), 0, PAGE_WIDTH, NULL);
		ASSERT_ADDRESS(str, ALIGN(ntsc->daddr));
		TRC_MEM(eprintf("Setting pdom prot on ntsc data (%p)\n",
				ALIGN(ntsc->daddr)));
		SALLOC_SETPROT(salloc, str, pdom, 
					 SET_ELEM(Stretch_Right_Read));
	    }

	    break;
	    
	  case nexus_module:
	    mod = nexus_ptr.nu_mod++;
	    TRC_MEM(eprintf("MOD:  T=%06lx:%06lx\n",
			    mod->addr, mod->size));
	    str = StretchAllocatorF$NewOver(sallocF, mod->size, AXS_GE, 
					    (addr_t)mod->addr, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, mod->addr);
	    break;
	    
	  case nexus_namespace:
	    name = nexus_ptr.nu_name++;
	    nexus_ptr.generic = (char *)nexus_ptr.generic + 
		name->nmods * sizeof(addr_t);
	    TRC_MEM(eprintf("NAME: N=%06lx:%06lx\n", 
			    name->naddr, name->nsize));
	    if (name->nsize == 0){

		/* XXX If we put an empty namespace in the nemesis.nbf
		   file, nembuild still reserves a page for it in the
		   nexus, so we need to make sure that at least a page is
		   requested from the stretch allocator, otherwise the
		   _next_ entry in the nexus will cause the ASSERT_ADDRESS
		   to fail. This probably needs to be fixed in
		   nembuild.
		   */

		TRC_MEM(eprintf(
		    "NAME: Allocating pad page for empty namespace\n"));

		name ->nsize = 1;
	    }
	    

	    str = StretchAllocatorF$NewOver(sallocF, name->nsize, 
					    AXS_GR, (addr_t)name->naddr, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, name->naddr);
	    break;
	    
	  case nexus_program:
	    prog= nexus_ptr.nu_prog++;
	    TRC_MEM(eprintf("PROG: T=%06lx:%06lx D=%06lx:%06lx "
			    "B=%06lx:%06lx  \"%s\"\n",
			    prog->taddr, prog->tsize,
			    prog->daddr, prog->dsize,
			    prog->baddr, prog->bsize,
			    prog->program_name));

	    str = StretchAllocatorF$NewOver(sallocF, prog->tsize, 
					    AXS_NONE, (addr_t)prog->taddr, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, prog->taddr);

	    /* Keep record of the stretch for later mapping into pdom */
	    addr_map[map_index].address= (addr_t)prog->taddr;
	    addr_map[map_index++].str  = str;

	    if (prog->dsize + prog->bsize) {
		str = StretchAllocatorF$NewOver(
		    sallocF, ROUNDUP((prog->dsize+prog->bsize), FRAME_WIDTH), 
		    AXS_NONE, (addr_t)prog->daddr, 0, PAGE_WIDTH, NULL);
		ASSERT_ADDRESS(str, prog->daddr);
		/* Keep record of the stretch for later mapping into pdom */
		addr_map[map_index].address= (addr_t)prog->daddr;
		addr_map[map_index++].str  = str;
	    }
	    
	    break;

	case nexus_blob:
	    blob = nexus_ptr.nu_blob++;
	    TRC_MEM(eprintf("BLOB: B=%06lx:%06lx\n",
			    blob->base, blob->len));

	    /* slap a stretch over it */
	    str = StretchAllocatorF$NewOver(sallocF, blob->len, 
					    AXS_GR, (addr_t)blob->base, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, blob->base);
	    break;

	  case nexus_EOI:
	    EOI = nexus_ptr.nu_EOI++;
	    TRC_MEM(eprintf("EOI:  %lx\n", EOI->lastaddr));
	    break;
	    
	  default:
	    TRC_MEM(eprintf("Bogus NEXUS entry: %x\n", *nexus_ptr.tag));
	    ntsc_halt();
	    break;
	}
    }
    TRC_MEM(eprintf("CreateAddressSpace: Done\n"));
    return pdom;

}


/*
** At startup we create a physical heap; while this is fine, the idea
** of protection is closely tied to that of stretches. Hence this function
** maps a stretch over the existing heap.
** This allows us to map it read/write for us, and read-only to everyone else.
*/
void MapInitialHeap(HeapMod_clp hmod, Heap_clp heap, 
		    word_t heap_size, ProtectionDomain_ID pdom)
{
    Stretch_clp str;
    Heap_clp realheap; 
    addr_t a = (addr_t)((size_t)heap & ~(PAGE_SIZE-1));

    TRC(eprintf(" + Mapping stretch over heap: 0x%x bytes at %p\n", 
	    heap_size, a));
    str = StretchAllocatorF$NewOver((StretchAllocatorF_cl *)Pvs(salloc), 
				    heap_size, AXS_R, a, 0, 
				    PAGE_WIDTH, NULL);
    ASSERT_ADDRESS(str, a);
    TRC(eprintf(" + Done!\n"));

    realheap = HeapMod$Realize(hmod, heap, str);

    if(realheap != heap) 
	eprintf("WARNING: HeapMod$Realize(%p) => %p\n", 
		heap, realheap);


    /* Map our heap as local read/write */
    STR_SETPROT(str, pdom, (SET_ELEM(Stretch_Right_Read)|
			    SET_ELEM(Stretch_Right_Write)));
}

#endif



static void init_type_system(bootimage_t& /*bootimg*/)
{
#if 0
    /* Get an Exception System */
    kconsole << " + Bringing up exceptions" << endl;
    exceptions_module_v1_closure* xcp_mod;
    xcp_mod = load_module<exceptions_module_v1_closure>(bootimg, "exceptions_mod", "exported_exceptions_module_v1_rootdom");
    ASSERT(xcp_mod);

	exceptions = xcp_mod->create();
	Pervasives(xcp) = exceptions;
    kconsole <<  " + Bringing up type system" << endl;
    kconsole <<  " +-- getting safelongcardtable_mod..." << endl;
//    lctmod = load_module<longcardtable_module_v1_closure>(bootimg, "longcardtable_mod", "exported_longcardtable_module_v1_rootdom");
    kconsole <<  " +-- getting stringtable_mod..." << endl;
//    strmod = load_module<stringtable_module_v1_closure>(bootimg, "stringtable_mod", "exported_stringtable_module_v1_rootdom");
    kconsole <<  " +-- getting typesystem_mod..." << endl;
//    tsmod = load_module<typesystem_module_v1_closure>(bootimg, "typesystem_mod", "exported_typesystem_module_v1_rootdom");
    kconsole <<  " +-- creating a new type system..." << endl;
//    ts = tsmod->create(Pvs(heap), lctmod, strmod);
//    kconsole <<  " +-- done: ts is at " << ts << endl;
//    Pvs(types) = (TypeSystem_clp)ts;

    /* Preload any types in the boot image */
/*    {
        TypeSystemF_IntfInfo *info;

        kconsole << " +++ registering interfaces\n"));
        info = (TypeSystemF_IntfInfo)lookup("Types");
        while(*info) {
            TypeSystemF->RegisterIntf(ts, *info);
            info++;
        }
    }*/
#endif
}

static void init_namespaces(bootimage_t& /*bm*/)
{
#if 0
    /* Build initial name space */
    kconsole <<  " + Building initial name space: ";

    /* Build root context */
    kconsole <<  "<root>, ";
    context_module_v1_closure* context_mod;
    context_mod = load_module<context_module_v1_closure>(bootimg, "context_mod", "exported_context_module_rootdom");
    ASSERT(context_mod);

	root = context_mod->create_context(heap, Pvs(types));
/*    ContextMod = lookup("ContextModCl");
    root = ContextMod$NewContext(ContextMod, heap, Pvs(types) );
    Pvs(root)  = root;

    kconsole <<  "modules, ";
    {
        Context_clp mods = ContextMod$NewContext(ContextMod, heap, Pvs(types));
        ANY_DECL(mods_any, Context_clp, mods);
        char *cur_name;
        addr_t cur_val;

        Context$Add(root,"modules",&mods_any);

        set_namespace(nexusprimal->namespc, "modules");
        while((cur_name=lookup_next(&cur_val))!=NULL)
        {
            TRC_CTX(eprintf("\n\tadding %p with name %s", cur_val, cur_name));
            Context$Add(mods, cur_name, cur_val);
        }
        TRC_CTX(eprintf("\n"));
    }

    kconsole <<  "blob, ";
    {
        Context_clp blobs= ContextMod$NewContext(ContextMod, heap, Pvs(types));
        ANY_DECL(blobs_any, Context_clp, blobs);
        Type_Any b_any;
        char *cur_name;
        addr_t cur_val;

        Context$Add(root, "blob", &blobs_any);

        set_namespace(nexusprimal->namespc, "blob");
        while((cur_name=lookup_next(&cur_val))!=NULL)
        {
            TRC_CTX(eprintf("\n\tadding %p with name %s", cur_val, cur_name));
            ANY_INIT(&b_any, RdWrMod_Blob, cur_val);
            Context$Add(blobs, cur_name, &b_any);
        }
        TRC_CTX(eprintf("\n"));
    }

    kconsole <<  "proc, ";
    {
        Context_clp proc = ContextMod$NewContext(ContextMod, heap, Pvs(types));
        Context_clp domains = ContextMod$NewContext(ContextMod, heap, Pvs(types));
        Context_clp cmdline = ContextMod$NewContext(ContextMod, heap, Pvs(types));
        ANY_DECL(tmpany, Context_clp, proc);
        ANY_DECL(domany, Context_clp, domains);
        ANY_DECL(cmdlineany,Context_clp,cmdline);
        ANY_DECL(kstany, addr_t, kst);
        string_t ident=strduph(k_ident, heap);
        ANY_DECL(identany, string_t, ident);
        Context$Add(root, "proc", &tmpany);
        Context$Add(proc, "domains", &domany);
        Context$Add(proc, "kst", &kstany);
        Context$Add(proc, "cmdline", &cmdlineany);
        Context$Add(proc, "k_ident", &identany);
#ifdef INTEL
        parse_cmdline(cmdline, ((kernel_st *)kst)->command_line);
#endif
    }

    kconsole <<  "pvs, ";
    {
        Type_Any any;
        if (Context$Get(root,"modules>PvsContext",&any)) {
            Context$Add(root,"pvs",&any);
        } else {
            eprintf ("NemesisPrimal: WARNING: >pvs not created\n");
        }
    }

    kconsole <<  "symbols, ";
    {
        Context_clp tmp = ContextMod$NewContext(ContextMod, heap, Pvs(types) );
        ANY_DECL(tmpany,Context_clp,tmp);
        Context$Add(root,"symbols",&tmpany);
    }
*/
    /* Build system services context */
/*    kconsole <<  "sys, ";
    {
        Context_clp sys = ContextMod$NewContext(ContextMod, heap, Pvs(types) );
        ANY_DECL(sys_any,Context_clp,sys);
        ANY_DECL(salloc_any, StretchAllocator_clp, salloc);
        ANY_DECL(sysalloc_any, StretchAllocator_clp, sysalloc);
        ANY_DECL(ts_any, TypeSystem_clp, Pvs(types));
        ANY_DECL(ffany, FramesF_clp, framesF);
        ANY_DECL(strtab_any, StretchTbl_clp, strtab);
        Context$Add(root, "sys", &sys_any);
        Context$Add(sys, "StretchAllocator", &salloc_any);
        Context$Add(sys, "SysAlloc", &sysalloc_any);
        Context$Add(sys, "TypeSystem", &ts_any);
        Context$Add(sys, "FramesF", &ffany);
        Context$Add(sys, "StretchTable", &strtab_any);
    }
*/
    /* IDC stub context */
/*    kconsole <<  "IDC stubs, ";
    {
        Closure_clp stubs_register;
        Context_clp stubs =
        ContextMod$NewContext(ContextMod, heap, Pvs(types) );

        // Create the stubs context
        CX_ADD("stubs", stubs, Context_clp);

        set_namespace(nexusprimal->namespc, "primal");

        // Dig up the stubs register closure
        stubs_register = * ((Closure_clp *) lookup("StubsRegisterCl"));
        Closure$Apply(stubs_register);

        TRC_CTX(eprintf("\n"));
    }
*/
    /*
     ** Next is the program information. Need to iterate though them and
     ** get the entry points, values, and namespaces of each of them
     ** and dump 'em into a context.
     */

    /* Boot domains/programs context */
/*    kconsole <<  "progs." << endl;
    {
        Context_clp progs= ContextMod$NewContext(ContextMod, heap, Pvs(types));
        ANY_DECL(progs_any,Context_clp,progs);
        ProtectionDomain_ID prog_pdid;
        BootDomain_InfoSeq *boot_seq;
        BootDomain_Info *cur_info;
        nexus_prog *cur_prog;
        char *name;
        Type_Any *any, boot_seq_any, blob_any;

        Context$Add(root,"progs",&progs_any);

        boot_seq = SEQ_NEW(BootDomain_InfoSeq, 0, Pvs(heap));
        ANY_INIT(&boot_seq_any, BootDomain_InfoSeq, boot_seq);
*/
        /* Iterate through progs in nexus and add appropriately... */
/*        while((cur_prog= find_next_prog())!=NULL)
        {
            TRC_PRG(eprintf("Found a program, %s,  at %p\n",
                            cur_prog->program_name, cur_prog));

            if(!strcmp("Primal", cur_prog->program_name))
            {
                TRC_PRG(eprintf(
                    "NemesisPrimal: found params for nemesis domain.\n"));
*/
                /*
                 * * Allocate some space for the nemesis info stuff, and then
                 ** add the info in the 'progs' context s.t. we can get at
                 ** it from the Nemesis domain (for Builder$NewThreaded)
                 */
/*                nemesis_info= Heap$Malloc(Pvs(heap), sizeof(BootDomain_Info));
                if(!nemesis_info) {
                    eprintf("NemesisPrimal: out of memory. urk. death.\n");
                    ntsc_halt();
                }

                nemesis_info->name       = cur_prog->program_name;
                nemesis_info->stackWords = cur_prog->params.stack;
                nemesis_info->aHeapWords = cur_prog->params.astr;
                nemesis_info->pHeapWords = cur_prog->params.heap;
                nemesis_info->nctxts     = cur_prog->params.nctxts;
                nemesis_info->neps       = cur_prog->params.neps;
                nemesis_info->nframes    = cur_prog->params.nframes;
*/
                /* get the timing parameters from the nbf */
/*                nemesis_info->p = cur_prog->params.p;
                nemesis_info->s = cur_prog->params.s;
                nemesis_info->l = cur_prog->params.l;

                nemesis_info->x = (cur_prog->params.flags & BOOTFLAG_X) ?
                True : False;
                nemesis_info->k = (cur_prog->params.flags & BOOTFLAG_K) ?
                True : False;

                if(!nemesis_info->k)
                    eprintf("WARNING: Nemesis domain has no kernel priv!\n");

                CX_ADD_IN_CX(progs, "Nemesis", nemesis_info, BootDomain_InfoP);

                MapDomain((addr_t)cur_prog->taddr, (addr_t)cur_prog->daddr,
                          nemesis_pdid);
            } else {
                cur_info= Heap$Malloc(heap, sizeof(BootDomain_Info));
*/
                /* Each program has a closure at start of text... */
/*                cur_info->cl= (Closure_cl *)cur_prog->taddr;
*/
                /* Create a new pdom for this program */
/*                prog_pdid      = MMU$NewDomain(mmu);
                cur_info->pdid = prog_pdid;

                MapDomain((addr_t)cur_prog->taddr, (addr_t)cur_prog->daddr,
                          prog_pdid);

                cur_info->name       = cur_prog->program_name;
                cur_info->stackWords = cur_prog->params.stack;
                cur_info->aHeapWords = cur_prog->params.astr;
                cur_info->pHeapWords = cur_prog->params.heap;
                cur_info->nctxts     = cur_prog->params.nctxts;
                cur_info->neps       = cur_prog->params.neps;
                cur_info->nframes    = cur_prog->params.nframes;
*/
                /* get the timing parameters from the nbf */
/*                cur_info->p = cur_prog->params.p;
                cur_info->s = cur_prog->params.s;
                cur_info->l = cur_prog->params.l;

                cur_info->x = (cur_prog->params.flags & BOOTFLAG_X) ?
                True : False;
                cur_info->k = (cur_prog->params.flags & BOOTFLAG_K) ?
                True : False;
  */              /*
                 ** We create a new context to hold the stuff passed in in
                 ** the .nbf as the namespace of this program.
                 ** This will be later be inserted as the first context of
                 ** the merged context the domain calls 'root', and hence
                 ** will override any entries in subsequent contexts.
                 ** This allows the use of custom modules (e.g. a debug heap
                 ** module, a new threads package, etc) for a particular
                 ** domain, without affecting any other domains.
                 */
/*                TRC_PRG(eprintf("Creating program's environment context.\n"));
                cur_info->priv_root = ContextMod$NewContext(ContextMod, heap, Pvs(types));

                // XXX what are the other fields of cur_prog->name _for_ ??
                set_namespace((namespace_entry *)cur_prog->name->naddr, NULL);
                while((name=lookup_next((addr_t *)&any))!=(char *)NULL)
                {
                    NOCLOBBER bool_t added= False;

                    // XXX hack!!
                    if (!strncmp(name, "blob>", 5))
                    {
                        ANY_INIT(&blob_any, RdWrMod_Blob, (addr_t)any);
                        //
                        //            TRC_PRG(eprintf("  ++ adding blob '%s':"
                        //                    "base=%x, len= %x\n",
                        //                    name, any->type, any->val));
                         //
                        any = &blob_any;
                    }

                    TRC_PRG(eprintf("  ++ adding '%s': type= %qx, val= %qx\n",
                            name, any->type, any->val));
*/
                    /*
                    ** XXX SMH: the below is messy in order to deal with
                    ** compound names of the form X>Y. If we wanted to deal
                    ** with the more general case (A>B>...>Z), it would be
                    ** even messier.
                    ** Perhaps a 'grow' flag to the Context$Add method would
                    ** be a good move? Wait til after crackle though....
                    ** Also: if want to override something in e.g. >modules>,
                    ** won't work unless copy across rest of modules too.
                    ** If you don't understand why, don't do it.
                    ** If you do understand why, change Context.c
                    */
  /*                  added  = False;
                    TRY {
                        Context$Add(cur_info->priv_root, name, any);
                        added= True;
                    } CATCH_Context$NotFound(UNUSED name) {
                        TRC_PRG(eprintf(" notfound %s (need new cx)\n", name));
                        // do nothing; added is False
                    } CATCH_ALL {
                        TRC_PRG(eprintf("     (caught exception!)\n"));
                        // ff
                    } ENDTRY;

                    if(!added) { // need a subcontext
                        Context_clp new_cx;
                        char *first, *rest;

                        first= strdup(name);
                        rest = strchr(first, SEP);
                        *rest++ = '\0';
                        new_cx= CX_NEW_IN_CX(cur_info->priv_root, first);
                        Context$Add(new_cx, rest, any);
                    }
                }

                if (cur_prog->params.flags & BOOTFLAG_B)
                {
                    // Add to the end of the sequence
                    SEQ_ADDH(boot_seq, cur_info);
                }
                else
                {
                    // Not a boot domain, so just dump the info in a context
                    mk_prog_cx(progs, cur_info);
                }
            }
        }
        kconsole << " + Adding boot domain sequence to progs context...\n"));
        Context$Add(progs, "BootDomains", &boot_seq_any);
    }*/
#endif
}

static NEVER_RETURNS void start_root_domain(bootimage_t& /*bm*/)
{
    /* Find the Virtual Processor module */
/*    vp = NAME_FIND("modules>VP", VP_clp);
    kconsole << " + got VP   at %p\n", vp));

    Time = NAME_FIND("modules>Time", Time_clp);
    kconsole << " + got Time at %p\n", Time));
    Pvs(time)= Time;
*/
    /* IM: init the wall-clock time values of the PIP */
/*    INFO_PAGE.prev_sched_time = NOW();
    INFO_PAGE.ntp_time  = NOW();
    INFO_PAGE.NTPscaling_factor = 0x0000000100000000LL; // 1.0

    // DomainMgr
    kconsole << " + initialising domain manager.\n"));
    {
        DomainMgrMod_clp dmm;
        LongCardTblMod_clp LongCardTblMod;
        Type_Any dommgrany;

        dmm = NAME_FIND("modules>DomainMgrMod", DomainMgrMod_clp);
        LongCardTblMod = NAME_FIND("modules>LongCardTblMod", LongCardTblMod_clp);
        dommgr = DomainMgrMod$New(dmm, salloc, LongCardTblMod,
                                framesF, mmu, vp, Time, kst);

        ANY_INIT(&dommgrany,DomainMgr_clp,dommgr);
        Context$Add(root,"sys>DomainMgr",&dommgrany);
    }

    kconsole << " + creating Trace module.\n"));
    {
        Type_Any any;
        Trace_clp t;
        TraceMod_clp tm;

        tm = NAME_FIND("modules>TraceMod", TraceMod_clp);
        t = TraceMod$New(tm);
        kst->trace = t;
        ANY_INIT(&any, Trace_clp, t);
        Context$Add(root, "sys>Trace", &any);
    }
*/
    kconsole << " + creating first domain." << endl;

    /*
    * The Nemesis domain contains servers which look after all sorts
    * of kernel resources.  It it trusted to play with the kernel
    * state in a safe manner
    */
/*    Nemesis = NAME_FIND("modules>Nemesis", Activation_clp);
    Nemesis->st= kst;

    if(nemesis_info == (BootDomain_Info *)NULL) {
        // shafted
        eprintf("NemesisPrimal: didn't get nemesis params. Dying.\n");
        ntsc_halt();
    }

    Pvs(vp) = vp = DomainMgr$NewDomain(dommgr, Nemesis, &nemesis_pdid,
                                    nemesis_info->nctxts,
                                    nemesis_info->neps,
                                    nemesis_info->nframes,
                                    0, // no act str reqd
                                    nemesis_info->k,
                                    "Nemesis",
                                    &did,
                                    &dummy_offer);
    // Turn off activations for now
    VP$ActivationsOff(vp);

#ifdef __IX86__
    // Frob the pervasives things. This is pretty nasty.
    RW(vp)->pvs      = &NemesisPVS;
    INFO_PAGE.pvsptr = &(RW(vp)->pvs);
#endif
    kconsole << " + did NewDomain." << endl;
*/

    /* register our vp and pdom with the stretch allocators */
/*    SAllocMod$Done(SAllocMod, salloc, vp, nemesis_pdid);
    SAllocMod$Done(SAllocMod, (StretchAllocatorF_cl *)sysalloc,
                vp, nemesis_pdid);

    DomainMgr$AddContracted(dommgr, did,
                            nemesis_info->p,
                            nemesis_info->s,
                            nemesis_info->l,
                            nemesis_info->x);

    kconsole << "NemesisPrimal: done DomainMgr_AddContracted.\n"));
    kconsole << "      + domain ID      = %x\n", (word_t)did));
    kconsole << "      + activation clp = %p\n", (addr_t)Nemesis));
    kconsole << "      + vp closure     = %p\n", (addr_t)vp));
    kconsole << "      + rop            = %p\n", (addr_t)RO(vp)));

#if defined(__i386__) || defined(__x86_64)
    kconsole << "*************** ENGAGING PROTECTION ******************\n"));
    MMU$Engage(mmu, VP$ProtDomID(vp));
#else
    // install page fault handler
    // Identity map currently executing code.
    // page 0 is not mapped to catch null pointers
    //     map_identity("bottom 4Mb", PAGE_SIZE, 4*MiB - PAGE_SIZE);
    // enable paging
    //     static_cast<x86_protection_domain_t&>(protection_domain_t::privileged()).enable_paging();
    //     kconsole << "Enabled paging." << endl;
    #warning Need some protection for your architecture.
#endif

    kconsole << "NemesisPrimal: Activating Nemesis domain" << endl;
    ntsc_actdom(RO(vp), Activation_Reason_Allocated);
*/
    PANIC("root_domain entry returned!");
}

//======================================================================================================================
// load all required modules (mostly drivers)
//======================================================================================================================

//static void load_modules(UNUSED_ARG bootimage_t& bm, UNUSED_ARG const char* root_module)
//{
    // if bootpage contains devtree, we use it in building modules deps
    // find modules corresponding to devtree entries and add them to deps list
    // if no devtree present (on x86) we add "probe devices later" entry to bootpage to force
    // module probing after initial startup.

//     module_loader_t ml;
//     ml.load_modules("boot");
    // each module has .modinfo section with entry point and other meta info
    // plus .modinfo.deps section with module dependencies graph data

    // Load components from bootimage.
//     kconsole << "opening initfs @ " << bootimage->mod_start << endl;
//     initfs_t initfs(bootcp->mod_start);
//     typedef void (*comp_entry)(bootinfo_t bi_page);
//}

//======================================================================================================================
// Image bootup entry point
//======================================================================================================================

/*!
 * Image bootup starts executing without paging and with full ring0 rights.
 */

extern "C" void entry()
{
    run_global_ctors(); // remember, we don't have proper crt0 yet.

    kconsole << " + image bootup entry!" << endl;

    bootinfo_t* bi = new(BOOTINFO_PAGE) bootinfo_t(false);
    address_t start, end;
    const char* name;
    if (!bi->get_module(1, start, end, name))
    {
        PANIC("Bootimage not found! in image bootup");
    }

    bootimage_t bootimage(name, start, end);

    INFO_PAGE.pervasives = &pervasives;

    init_mem(bootimage);
    init_type_system(bootimage);
    init_namespaces(bootimage);
    start_root_domain(bootimage);
}
// Load the modules.
// Module "boot" depends on all modules that must be probed at startup.
// Dependency resolution will bring up modules in an appropriate order.
//    load_modules(bootimage, "boot");
