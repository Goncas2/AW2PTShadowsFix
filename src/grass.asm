BITS 64
DEFAULT REL
; Build addresses are supplied by tools/generate_data.py.
ORG PATCH_VA

wrapper:
    push rbx
    sub rsp, strict dword 0xa0
prolog_end:
    mov rbx, rcx
    call trampoline
    test dword [rbx+0x4c], 0x800000
    jz finished
    test dword [rbx+0x4c], 0x400000
    jnz finished
    mov r10, [rbx+8]
    test r10, r10
    jz finished
    movzx eax, byte [r10+0x270]
    test eax, eax
    jz finished
    cmp eax, 8
    ja finished
    movzx ecx, byte [r10+0x272]
    test ecx, ecx
    jz finished
    cmp ecx, 8
    ja finished
    mov r8, [rbx+0x18]
    test r8, r8
    jnz override_tables
    mov ecx, [rbx+0x4c]
    shr ecx, 24
    cmp ecx, [r10+0x178]
    ja finished
    imul ecx, eax
    shl rcx, 3
    mov r8, [r10+0x380]
    mov r9, [r10+0x390]
    test r8, r8
    jz finished
    test r9, r9
    jz finished
    add r8, rcx
    add r9, rcx
    mov r11d, 8
    jmp collect_begin
override_tables:
    lea r9, [r8+8]
    mov r11d, 16
collect_begin:
    xor ecx, ecx
    xor edx, edx
collect_lod:
    ; Match the engine's clamp to the last deformation entry.
    movzx eax, byte [r10+0x272]
    dec eax
    cmp ecx, eax
    cmovb eax, ecx
    cmp dword [r10+rax*4+0x1c8], 0
    jne next_lod
    mov rax, [r8]
    test rax, rax
    jz collect_transparent
    mov [rsp+rdx*8+0x20], rax
    inc edx
collect_transparent:
    mov rax, [r9]
    test rax, rax
    jz next_lod
    mov [rsp+rdx*8+0x20], rax
    inc edx
next_lod:
    add r8, r11
    add r9, r11
    inc ecx
    movzx eax, byte [r10+0x270]
    cmp ecx, eax
    jb collect_lod
    test edx, edx
    jz finished
    mov r9d, edx
    mov edx, [rbx+0x48]
    lea r8, [rsp+0x20]
    mov rcx, [rel wrapper + (STATIC_MANAGER-PATCH_VA)]
    call wrapper + (STATIC_REGISTRAR-PATCH_VA)
    or dword [rbx+0x4c], 0x400000
finished:
    add rsp, strict dword 0xa0
    pop rbx
    ret
wrapper_end:

trampoline:
    mov [rsp+8], rbx
    jmp wrapper + (REGISTER_RETURN-PATCH_VA)

ALIGN 4, db 0
unwind_info:
    db 1, prolog_end-wrapper, 3, 0
    db prolog_end-wrapper, 1
    dw 0xa0/8
    db 1, 0x30
    dw 0

ALIGN 16, db 0
helper_main:
    mov rdx, rbx
    lea r10, [rel wrapper + (MAIN_RETURN-PATCH_VA)]
    jmp helper_mark_static
helper_visibility:
    mov rax, [rbp+0x18]
    lea rcx, [rdi+rbx]
    mov rdx, [rax+rcx*8]
    lea r10, [rel wrapper + (VISIBILITY_RETURN-PATCH_VA)]
helper_mark_static:
    ; These entries borrow the original frame. Preserve RSP and R11.
    cmp byte [rsp+0x98], 0
    je helper_done
    test rdx, rdx
    jz helper_done
    mov eax, [rdx+0x4c]
    and eax, 0xc00000
    cmp eax, 0xc00000
    jne helper_done
    mov ecx, [rdx+0x48]
    shl rcx, 6
    add rcx, [rbp+0x160]
    mov r9, [r13+0x228]
    mov r8d, 16
helper_mark_loop:
    mov eax, [rcx]
    cmp eax, -1
    je helper_done
    mov edx, eax
    shr eax, 6
    and edx, 0x3f
    bts qword [r9+rax*8], rdx
    add rcx, 4
    dec r8d
    jnz helper_mark_loop
helper_done:
    mov rdx, [rsp+0x90]
    jmp r10
ALIGN 4, db 0
helper_unwind:
    ; Version 1, CHAININFO; inherit the full body fragment with R12/R15 saves.
    db 0x21, 0, 0, 0
    dd PARENT_BEGIN, PARENT_END, PARENT_UNWIND
