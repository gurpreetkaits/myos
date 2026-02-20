; switch.asm - Context switch for multitasking
;
; void context_switch(uint32_t *old_esp, uint32_t new_esp);
;
; Push order: ebx, esi, edi, ebp (4 regs × 4 bytes = 16 bytes)
; After pushes, stack offsets from ESP:
;   [ESP+0]  = EBP    (last pushed)
;   [ESP+4]  = EDI
;   [ESP+8]  = ESI
;   [ESP+12] = EBX    (first pushed)
;   [ESP+16] = return address
;   [ESP+20] = arg1: old_esp
;   [ESP+24] = arg2: new_esp

[bits 32]

global context_switch
global task_start_wrapper

context_switch:
    push ebx
    push esi
    push edi
    push ebp

    mov eax, [esp + 20]    ; eax = old_esp (pointer)
    mov [eax], esp          ; *old_esp = current ESP

    mov esp, [esp + 24]     ; ESP = new_esp (read from OLD stack before switch)

    ; Now on the new task's stack
    pop ebp
    pop edi
    pop esi
    pop ebx

    ret                     ; Return to new task's saved return address

; Called the first time a new task runs.
; Enables interrupts then falls through to the actual task entry point.
task_start_wrapper:
    sti
    ret
