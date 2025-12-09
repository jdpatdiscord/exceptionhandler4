EXTERN EH4_KiUserExceptionDispatcher_C:PROC
EXTERN g_pfnKiUserExceptionDispatcher:QWORD

PUBLIC EH4_KiUserExceptionDispatcher_ASM

.code

EH4_KiUserExceptionDispatcher_ASM PROC
    ;   rsp+0x000 = CONTEXT
    ;   rsp+0x4F0 = EXCEPTION_RECORD
    
    push    rbx
    mov     rbx, rsp
    add     rbx, 8
    
    and     rsp, 0FFFFFFFFFFFFFFF0h
    sub     rsp, 20h
    
    mov     rcx, rbx            ; rcx = PCONTEXT
    lea     rdx, [rbx + 4F0h]   ; rdx = PEXCEPTION_RECORD
    
    call    EH4_KiUserExceptionDispatcher_C
    
    lea     rsp, [rbx - 8]
    pop     rbx
    
    jmp     g_pfnKiUserExceptionDispatcher

EH4_KiUserExceptionDispatcher_ASM ENDP

END