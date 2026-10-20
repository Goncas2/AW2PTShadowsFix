BITS 64
DEFAULT REL
SECTION .text
GLOBAL Aw2RunNotification
; Windows ABI: code,object,manager,builder,visibility,rt_enabled.
; Run the real relocated helper with a synthetic parent-frame layout.
Aw2RunNotification:
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 0xb8
    mov r12, rcx
    mov rbx, rdx
    mov rbp, r8
    mov r13, r9
    xor edi, edi
    cmp dword [rsp+0x120], 0
    je .main
    mov ebx, 3
.main:
    mov [rsp+0x20], rbx
    mov [rsp+0x28], rbp
    mov [rsp+0x30], r13
    mov eax, [rsp+0x128]
    mov [rsp+0x90], al ; helper RSP+0x98, accounting for CALL return address
    mov rax, 0x1020304050607080
    mov [rsp+0x88], rax ; displaced MOV replay value
    mov r11, 0x123456789abcdef0
    call r12
    xor eax, eax
    mov rcx, 0x123456789abcdef0
    cmp r11, rcx
    je .r11ok
    or eax, 1
.r11ok:
    mov rcx, 0x1020304050607080
    cmp rdx, rcx
    je .rdxok
    or eax, 2
.rdxok:
    cmp rbx, [rsp+0x20]
    jne .badnonvolatile
    cmp rbp, [rsp+0x28]
    jne .badnonvolatile
    cmp r13, [rsp+0x30]
    je .done
.badnonvolatile:
    or eax, 4
.done:
    add rsp, 0xb8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
