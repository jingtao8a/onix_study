[bits 32]
global task_switch
[section .text]
task_switch:
    ;保存上下文 start
    push ebp
    mov ebp, esp
    
    push ebx
    push esi
    push edi
    
    mov eax, esp
    and eax, 0xfffff000;current
    mov [eax], esp
    ;end
    mov eax, [ebp + 8]; next
    mov esp, [eax]
    
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret