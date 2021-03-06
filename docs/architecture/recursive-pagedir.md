#### Recursive PDE/PTE [explained by Brendan](http://forum.osdev.org/viewtopic.php?f=15&t=19387)

The area from 0xFFFFF000 to 0xFFFFFFFF becomes a mapping of all page directory entries, and the area from 0xFFC00000 to 0xFFFFFFFF becomes a mapping of all page table entries (if the corresponding page table is present). These areas overlap because the page directory is being used as a page table - the area from 0xFFFFF000 to 0xFFFFFFFF contains the page directory entries and the page table entries for the last page table (which is the same thing).

To access the page directory entry for any virtual address you'd be able to do `mov eax,[0xFFFFF000 + (virtualAddress >> 20) * 4]`, and (if the page table is present) to access the page table entry for any virtual address you'd be able to do `mov eax,[0xFFC00000 + (virtualAddress >> 12) * 4]`. Most C programmers would use these areas as arrays and let the compiler do some of the work, so they can access any page directory entry as `PDE = pageDirectoryMapping[virtualAddress >> 20]` and any page table entry as `PTE = pageTableMapping[virtualAddress >> 12]`.

Now imagine that someone wants to allocate a new page at 0x12345000. You need to check if a page table exists for this area (and if the page table doesn't exist, allocate a new page of RAM to use as a page table, create a page directory entry for it, and INVLPG the mapping for the new page table), then check if the page already exists (and if the page doesn't exist, allocate a new page of RAM, create a page table entry for the new page, and INVLPG the new page).

The same sort of lookups are needed for lots of things - freeing a page, checking accessed/dirty bits, changing page attributes (read, write, execute, etc), page fault handling, etc. Basically, having fast access to page directory entries and page table entries is important, because it speeds up everything your virtual memory management code does.

Without making a page directory entry refer to the page directory, you need to find some other way to access page directory entries and page table entries, and there is no better way (all the other ways are slower, more complicated and waste more RAM).

##### Combuster@osdev

To avoid deadlocks due to unmapped structures, I reserve an unmapped region where the pagetable for it is guaranteed to exist (and use algorithms that use fixed amounts of address space to complete) - which is essentially equivalent to granting access to physical memory without tripping the VMM into recursion. You may find a way to fix your implementation based on this concept.

##### Brendan@osdev

The first method (plain free page stack) looks simple but it isn't because `currentPageStackTop` is a physical address and `currentPageStackTop->nextFreePage;` doesn't work. You have to map `currentPageStackTop` into linear memory before you can get the next free page. This usually means combining it with the linear memory manager - when you're allocating a linear page you'd store `currentPageStackTop` into the page table, then use INVLPG to flush the TLB entry, then get the address of the next free page from whereever you mapped `currentPageStackTop`.

[More detailed article on recursive page directory](http://www.rohitab.com/discuss/index.php?showtopic=31139)

#### More details about RPD from osdev forums:

Re: Is there any easy way to read/write physical address?
Postby torshie on August 16th, 2009, 3:40 pm
> Brendan wrote:Hi,

> For accessing memory mapped I/O areas, you map the areas into the address space and leave them mapped for next time. The BIOS won't help you, so there's no real reason to temporarily map that into an address space. That only really leaves paging structures (page tables, page directories, page directory pointer tables, etc).

> If you do the "self reference" thing (e.g. use a PML4 entry to point to the PML4) then you can access all the paging structures (page tables, page directories, page directory pointer tables, etc) for the current address space.

> Now we're only left with how to access the paging structures that belong to a different address space; so my question is, how often do you need to do this and why?


> Cheers,

> Brendan


Hi,
The "self reference" trick seems to be OK for PML4, but if applied to other levels of paging structures will make the linear address space non-continuous. How to you solve this problem?

Cheers,
torshie

--------------------------------

Re: Is there any easy way to read/write physical address?
Postby Brendan on August 16th, 2009, 5:19 pm
Hi,

> torshie wrote:The "self reference" trick seems to be OK for PML4, but if applied to other levels of paging structures > will make the linear address space non-continuous. How to you solve this problem?


Consider something like a page directory. You could put the physical address of the page directory into any page directory entry you like (and create a 1 GiB mapping of all the page directory entries and page tables in that are effected by that page directory), and you can place this 1 GiB mapping anywhere in the address space (on any 1 GiB boundary).

However, I assume that you're asking this because you're still thinking of doing temporary mappings. There's no real need for temporary mappings though. Think of it like this...

If you put the physical address of the PML4 into a PLM4 entry (and do this for every address space), then you have a permanent "master map of everything"; which costs no RAM at all. The only real cost is linear space - for example, by using the highest PLM4 entries for the "self reference" you'd end up with a 512 GiB "master map of everything" at the top of the address space (from 0xFFFFFF8000000000 to 0xFFFFFFFFFFFFFFFF). Note: 512 GiB might sound huge, but it's really a small percentage of the entire usable address space (512 GiB out of 262144 GiB).

Also note that this "self reference" thing is recursive. The same 4 KiB of physical RAM that's used for the PLM4 actually becomes a page directory pointer table, and a page directory, and a page table, and a page. This creates an address space like this:

```
0xFFFFFFFFFFFFF000 to 0xFFFFFFFFFFFFFFFF - mapping of all page map level 4 entries in the address space
0xFFFFFFFFFFE00000 to 0xFFFFFFFFFFFFFFFF - mapping of all page directory pointer table entries in the address space
0xFFFFFFFFC0000000 to 0xFFFFFFFFFFFFFFFF - mapping of all page directory entries in the address space
0xFFFFFF8000000000 to 0xFFFFFFFFFFFFFFFF - mapping of all page table entries in the address space
0xFFFF800000000000 to 0xFFFFFF7FFFFFFFFF - normal "kernel space"
0x0000800000000000 to 0xFFFF7FFFFFFFFFFF - unusable (not canonical)
0x0000000000000000 to 0x00007FFFFFFFFFFF - "User space"
```

One single (permanent) PLM4 entry, and you can access everything (for the current address space) you could ever possibly want to access.

The only real problem is accessing the paging structures for other address spaces. However, there's many variations of the same trick. You can take any type of paging structure (PLM4, page directory pointer table, page directory or page table) from any address space, and map it as anything else in any other address space; as long as it's at the same or lower level (e.g. you can map a page directory into an address space as if it's a page table, but you can't map a page table into an address space as if it's a page directory).

However, what you need depends on your OS (more specifically, it depends on how your OS supports "shared memory"). For my OS I don't support shared memory at all. This means I need to be able to change the PLM4 entries in any address space from any other address space, but nothing else. To allow this I have an array of PLM4s (e.g. for N address spaces I've got N pages containing PLM4 data mapped into kernel space).

---------------------------------------

Re: Is there any easy way to read/write physical address?
Postby torshie on August 17th, 2009, 3:08 pm
Hi Brendan,
I finally get your "self reference" trick. I just didn't get the fact that I could forge different linear addresses to read/write all four levels of page map structures with only one PML4 "self reference" entry. I thought I need a PML4 entry to read/write PML4 structure, a PDP entry to read/write PDP structure, etc. Of course, I was completely wrong.
This is really a greeeeeeeeeaaaaaaaaaaaaat trick.
Thank you very much.

----------------------------------------

Re: Is there any easy way to read/write physical address?
Postby Brendan on August 17th, 2009, 5:18 pm
Hi,

> torshie wrote:I finally get your "self reference" trick.


Hmm - it's not really "my" trick. I first found out about it about 15 years ago, but I wouldn't be surprised if Intel planned the paging structures to allow this sort of thing back when they designed the 80386.

I should also provide some warnings though.

The first thing to be careful of is TLB invalidations. Basically, if you change a page directory entry, page directory pointer table entry or PML4 entry, then you need to invalidate anything in the area you've changed (like you normally would/should) but you *also* need to invalidate the area in the "master map of everything". Of course in most of these cases it's usually faster to flush the entire TLB anyway. For example, if you change a page directory entry then you'd need to do "INVLPG" up to 512 times in the address space plus once in the mapping (or flush the entire TLB), and if you change a page directory pointer table entry then you'd need to do "INVLPG" up to 262144 times in the address space plus 512 times in the mapping (or flush the entire TLB).

The next thing to consider is the "accessed" flags. For something like a page directory entry there's one "accessed" flag in RAM, plus up to 2 copies of that "accessed" flag in TLB entries (a normal TLB entry plus a TLB entry for the mapping); and it's the operating systems responsibility to ensure that the "accessed" flags don't become out-of-sync, or to ensure that out-of-sync "accessed" flags don't cause problems for the OS (mainly for the code to decide if a page should/shouldn't be sent to swap space). If you only look at the "accessed" flag in page table entries for 4 KiB pages (and in page directory entries for 2 MiB pages) then you should be able to ignore this problem; but if you check the "accessed" flag in page directory entries, page directory pointer table entries or PML4 entries for any reason, then you may need to be careful.

Finally, for a 2 MiB page the page directory entry has bit 7 set (to indicate that it is a "large" page) and bit 12 contains the highest PAT bit; but when the CPU interprets this as a page table entry (in the "master map of everything") then bit 7 becomes the highest PAT bit and bit 12 becomes part of the physical address of a page. This makes a mess. For example, imagine you've programmed the PAT like this:

```
PAT value   Cache Type
0           Write-back (default, for compatibility)
1           Write-through (default, for compatibility)
2           Uncached (default, for compatibility)
3           Uncached (default, for compatibility)
4           Write-combining (modified by the OS and used by device drivers)
5           Write-through (default, for compatibility)
6           Uncached (default, for compatibility)
7           Uncached (default, for compatibility)
```

Also imagine that you map some normal RAM into an address space using a 2 MiB page, and you set the PAT so that the 2 MiB page uses "write-back" caching (as you would for all normal RAM). If the 2 MiB page is at physical address 0x123000000, then you create a page directory entry that contains 0x800000001230108F (or, "not executable, physical address = 0x0000000012300000, large page, PAT = 0 = write-back, read/write, user, present"), and when the CPU interprets this as a page table entry (in the "master map of everything") the CPU reads it as "not executable, physical address = 0x0000000012301000, PAT = 4 = write-combining, read/write, user, present". Assuming that your kernel only uses the "master map of everything" to access paging structures, then it shouldn't access this messed up page anyway, and it should be OK (but if your kernel accidentally does write to the messed up page then good luck trying to debug what happened).
