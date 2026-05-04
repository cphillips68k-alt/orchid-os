# paging.S

.globl paging_load_directory
paging_load_directory:
    movl 4(%esp), %eax
    movl %eax, %cr3
    retl

.globl paging_enable_paging
paging_enable_paging:
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0
    retl