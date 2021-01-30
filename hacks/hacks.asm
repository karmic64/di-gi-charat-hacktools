            .arm.little
            .create "hacks.gba",0x8000000
            .org 0x8000000
            .incbin "../DiGi Charat - DigiCommunication (J) [!].gba"
            .thumb
            .org 0x87f1e80
            ;NOTE: at this point -
            ;r0-r3 are free for use
            ;r5 contains the text info pointer
            ;r6 contains the printed char
            ;r8 = 0
                ;text info struct:
                ;r5, #0x10 - width of text box (halfword)
                ;r5, #0x18 - x-position (halfword)
                ;r5, #0x28 - text buf pointer
                ;r5, #0x2e - amount of chars left in buf (halfword)
            ;the current char has been REMOVED from the text buffer
wordwraphook:
            cmp r6, #0x0a
            beq @newlinechar
            cmp r6, #0x20
            bne @asnormal
            ;count the amount of chars until the next newline/space
            push {r4}
            mov r0, #6 ;width in width units (6 per tile)
            mov r1, #0 ;length in chars NOT including space
            ldr r2, [r5,#0x28] ;text pointer
            ldrh r3, [r5,#0x2e] ;max chars
@countloop: ldrh r4, [r2]
            cmp r1, r3
            beq @clescape
            cmp r4, #0x0a
            beq @clescape
            cmp r4, #0x20
            beq @clescape
            add r2, #2
            add r1, #1
            add r0, #6
            lsr r4, #8 ;double-byte?
            bcc @countloop
            add r0, #6
            b @countloop
@clescape:  pop {r4}
            ;check if there's enough space on the line
            ldrh r1, [r5,#0x10] ;width
            ldrh r2, [r5,#0x18] ;x-position
            cmp r0, r1 ;can't avoid ugliness if width > windowwidth...
            bgt @asnormal
            sub r1, r2
            beq @nextchar ;do not print a newline at the end of a line
            cmp r0, r1
            bgt @newlinechar ;if width > space left, print a new line instead of space
@asnormal:
            ldr r0, [@ptr_asnormal]
            bx r0
            
@newlinechar:
            ;copied from 0x80afde2
@newline:   ldrh r0, [r5,#0x16]
            ldrh r1, [r5,#0x20]
            mov r0, r1
            mov r2, r8
            strh r2, [r5,#0x18]
            strh r0, [r5,#0x1a]
@nextchar:
            ldr r0, [@ptr_nextchar]
            bx r0
            
            .align 4
@ptr_asnormal:
            .word 0x80afdf0 | 1
@ptr_printchar:
            .word 0x80afdf6 | 1
@ptr_nextchar:
            .word 0x80aff1a | 1
            
            ;
            ;
            ;
            ;   ---------- text entry menu
            ;
            ;
            ;
            
            ;change font used by header to thin3px
            .org 0x8054bfa ;main font
            bl 0x8098e74
            .org 0x8054b6a ;outlines
            bl 0x8098e74
            
            ;change font used by entered text display
            .org 0x8053f1c
            bl 0x8098e5c
            
            ;disable switching kana
            .org 0x80569b0
            bx lr
            ;disable moving from back -> kana button
            .org 0x805693e
            b org()+4
            ;don't highlight the kana button while moving off the text part
            .org 0x80560a6
            b org()+4
            ;don't show the kana or l sprites
            .org 0x8054884
            b org()+4
            
            ;disable R
            .org 0x805604e
            b 0x80560b4
            
            ;new character arrangement
            ;note, you MUST have characters on the bottom row
            ;or else the game will break moving up from bottom buttons
            .org 0x80e6348
            .area 0x80e6648-org()
                     ;col, row, char
            .halfword 00,00, 0x8260, 0 ;A
            .halfword 01,00, 0x8261, 0 ;B
            .halfword 02,00, 0x8262, 0 ;C
            .halfword 03,00, 0x8263, 0 ;D
            .halfword 04,00, 0x8264, 0 ;E
            .halfword 05,00, 0x8265, 0 ;F
            .halfword 06,00, 0x8266, 0 ;G
            .halfword 07,00, 0x8267, 0 ;H
            .halfword 08,00, 0x8268, 0 ;I
            .halfword 09,00, 0x8269, 0 ;J
            .halfword 10,00, 0x826a, 0 ;K
            .halfword 11,00, 0x826b, 0 ;L
            
            .halfword 00,01, 0x826c, 0 ;M
            .halfword 01,01, 0x826d, 0 ;N
            .halfword 02,01, 0x826e, 0 ;O
            .halfword 03,01, 0x826f, 0 ;P
            .halfword 04,01, 0x8270, 0 ;Q
            .halfword 05,01, 0x8271, 0 ;R
            .halfword 06,01, 0x8272, 0 ;S
            .halfword 07,01, 0x8273, 0 ;T
            .halfword 08,01, 0x8274, 0 ;U
            .halfword 09,01, 0x8275, 0 ;V
            .halfword 10,01, 0x8276, 0 ;W
            .halfword 11,01, 0x8277, 0 ;X
            
            .halfword 00,02, 0x8278, 0 ;Y
            .halfword 01,02, 0x8279, 0 ;Z
            .halfword 02,02, 0x824f, 0 ;0
            .halfword 03,02, 0x8250, 0 ;1
            .halfword 04,02, 0x8251, 0 ;2
            .halfword 05,02, 0x8252, 0 ;3
            .halfword 06,02, 0x8253, 0 ;4
            .halfword 07,02, 0x8254, 0 ;5
            .halfword 08,02, 0x8255, 0 ;6
            .halfword 09,02, 0x8256, 0 ;7
            .halfword 10,02, 0x8257, 0 ;8
            .halfword 11,02, 0x8258, 0 ;9
            
            ;.....
            
            .halfword 00,04, 0x8281, 0 ;a
            .halfword 01,04, 0x8282, 0 ;b
            .halfword 02,04, 0x8283, 0 ;c
            .halfword 03,04, 0x8284, 0 ;d
            .halfword 04,04, 0x8285, 0 ;e
            .halfword 05,04, 0x8286, 0 ;f
            .halfword 06,04, 0x8287, 0 ;g
            .halfword 07,04, 0x8288, 0 ;h
            .halfword 08,04, 0x8289, 0 ;i
            .halfword 09,04, 0x828a, 0 ;j
            .halfword 10,04, 0x828b, 0 ;k
            .halfword 11,04, 0x828c, 0 ;l
            
            .halfword 00,05, 0x828d, 0 ;m
            .halfword 01,05, 0x828e, 0 ;n
            .halfword 02,05, 0x828f, 0 ;o
            .halfword 03,05, 0x8290, 0 ;p
            .halfword 04,05, 0x8291, 0 ;q
            .halfword 05,05, 0x8292, 0 ;r
            .halfword 06,05, 0x8293, 0 ;s
            .halfword 07,05, 0x8294, 0 ;t
            .halfword 08,05, 0x8295, 0 ;u
            .halfword 09,05, 0x8296, 0 ;v
            .halfword 10,05, 0x8297, 0 ;w
            .halfword 11,05, 0x8298, 0 ;x
            
            .halfword 00,06, 0x8299, 0 ;y
            .halfword 01,06, 0x829a, 0 ;z
            .halfword 02,06, 0x8140, 0 ;space
            .halfword 03,06, 0x8144, 0 ;.
            .halfword 04,06, 0x8143, 0 ;,
            .halfword 05,06, 0x8149, 0 ;!
            .halfword 06,06, 0x8148, 0 ;?
            .halfword 07,06, 0x8166, 0 ;'
            .halfword 08,06, 0x817c, 0 ;-
            .halfword 09,06, 0x817b, 0 ;+
            .halfword 10,06, 0x8181, 0 ;=
            .halfword 11,06, 0x815e, 0 ;/
            
            ;....
            
            .halfword 00,09, 0x8197, 0 ;@
            .halfword 01,09, 0x8194, 0 ;#
            .halfword 02,09, 0x8190, 0 ;$
            .halfword 03,09, 0x8193, 0 ;%
            .halfword 04,09, 0x8195, 0 ;&
            .halfword 05,09, 0x8196, 0 ;*
            .halfword 06,09, 0x8169, 0 ;(
            .halfword 07,09, 0x816a, 0 ;)
            .halfword 08,09, 0x816d, 0 ;[
            .halfword 09,09, 0x816e, 0 ;]
            .halfword 10,09, 0x8183, 0 ;<
            .halfword 11,09, 0x8184, 0 ;>
            
            .halfword 00,10, 0x81f4, 0 ;musical note
            .halfword 01,10, 0x8189, 0 ;male
            .halfword 02,10, 0x818a, 0 ;female
            .halfword 03,10, 0x819a, 0 ;star
            .halfword 04,10, 0x8160, 0 ;tilde
            .halfword 05,10, 0x8187, 0 ;infinity
            .halfword 06,10, 0x81a8, 0 ;right
            .halfword 07,10, 0x81a9, 0 ;left
            .halfword 08,10, 0x81aa, 0 ;up
            .halfword 09,10, 0x81ab, 0 ;down
            .halfword 10,10, 0x8180, 0 ;divide
            .halfword 11,10, 0x8151, 0 ;underscore
            
            .endarea
            .fill 0x80e6648-org(), 0xff ;END OF LIST
            
            
            ;
            ;
            ;
            ; word wrap trampoline
            ;
            ;
            ;
            .org 0x80afdde
            ldr r0, [ptr_ww]
            bx r0
            .align 4
ptr_ww:     .word wordwraphook | 1
            
            
            
            .close
            