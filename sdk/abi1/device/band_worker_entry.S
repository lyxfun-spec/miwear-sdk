.syntax unified
.arch armv8-m.main
.thumb
.section .text.entry,"ax",%progbits
.global probe_entry
.type probe_entry,%function
.thumb_func
probe_entry:
 b.w .Lbody
.Lmailbox:
 .word 0
.Lbody:
 push {r4,lr}
 adr r4,.Lmailbox
 ldr r0,[r4]
 bl probe_main
 dsb sy
 pop {r4,pc}
.size probe_entry,.-probe_entry
