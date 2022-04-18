            .arm.little
            .create OUTNAME,0x8000000
            .org 0x8000000
            .incbin ROMNAME,0,0x7f1e80
            .thumb
            .org 0x87f1e80
            .db "- Di Gi Charat -"
            .db "DigiCom  English"
            .db "                "
            .db "Built on:       "
            .db "Util: YEAR/MO/DA"
            .db "        HH:MM:SS"
            .db " PRG: ",BUILDDATE
            .db "        ",BUILDTIME
            .db "  TL: YEAR/MO/DA"
            .db "        HH:MM:SS"
            .db "                "
            .db "Hacking:        "
            .db "          karmic"
            .db "Translation:    "
            .db "       albatross"
            .db "Greetings to:   "
            .db "      125scratch"
            .db "   chaoskaiser72"
            .db "Izumi/ShamaliyyD"
            .db "          keffie"
            .db "    MarioNumbers"
            .db "  Negative Nancy"
            .db "            stel"
            .db "Special Thanks: "
            .db " Hazuki Fujiwara"
            .db "     Onpu Segawa"
            .db "    Momoko Asuka"
            .db " Sakura Kinomoto"
            .db " Tomoyo Daidouji"
            .db "Tsubomi Hanasaki"
            .db "    Konata Izumi"
            .db "  Kagami Hiiragi"
            .db " Tsukasa Hiiragi"
            .db "   Miyuki Takara"
            .db "       Suigintou"
            .db "     Rin Kokonoe"
            .db "     Iori Minase"
            .db " Yayoi Takatsuki"
            .db "    Haruka Amami"
            .db "                "
            .db "Don't proceed any further! If you want to hack this translation, better use the source tools at <https://github.com/karmic64/di-gi-charat-hacktools> instead of your hex editor."
            .align 0x10,' '
            
            
            
            
            
            ;
            ;
            ; ------ normal text word wrap
            ;
            ;
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
            beq @@newlinechar
            cmp r6, #0x20
            bne @@asnormal
            ;count the amount of chars until the next newline/space
            push {r4}
            mov r0, #6 ;width in width units (6 per tile)
            mov r1, #0 ;length in chars NOT including space
            ldr r2, [r5,#0x28] ;text pointer
            ldrh r3, [r5,#0x2e] ;max chars
@@countloop:
            cmp r1, r3
            beq @@clescape
            ldrh r4, [r2]
            add r2, #2
            add r1, #1
            cmp r4, #0x0a
            beq @@clescape
            cmp r4, #0x20
            blt @@countloop ;ignore control codes
            beq @@clescape
            add r0, #6
            lsr r4, #8 ;double-byte?
            bcc @@countloop
            add r0, #6
            b @@countloop
@@clescape: pop {r4}
            ;check if there's enough space on the line
            ldrh r1, [r5,#0x10] ;width
            cmp r0, r1 ;can't avoid ugliness if width > windowwidth...
            bgt @@asnormal
            ldrh r2, [r5,#0x18] ;x-position
            sub r1, r2
            beq @@nextchar ;do not print a newline at the end of a line
            cmp r0, r1
            bgt @@newlinechar ;if width > space left, print a new line instead of space
@@asnormal:
            ldr r0, =0x80afdf0 | 1
            bx r0
            
@@newlinechar:
            ;copied from 0x80afde2
@@newline:  ldrh r0, [r5,#0x16]
            ldrh r1, [r5,#0x20]
            mov r0, r1
            mov r2, r8
            strh r2, [r5,#0x18]
            strh r0, [r5,#0x1a]
@@nextchar:
            ldr r0, =0x80aff1a | 1
            bx r0
            
            .pool
            
            
            
            ;
            ;
            ;
            ;   --------- text entry menu hooks
            ;
            ;
            ;
textentrymenuinitcounthook:
            ;[r3,#6] byte count
            ldrh r0, [r3,#6]
            add r1, r0,#1
            
            ;get second char in r6
            mov r6,#0x97
            lsl r6,#3
            add r6,r7
            ldr r6,[r6]
            ldrb r6,[r6,#1]
            cmp r6,#0
            beq @@skip
            add r1,#1
@@skip:     strh r1, [r3,#6]
            
            ;return
            lsl r0,#0x10 - 1
            ldr r2,=0x8053a1c | 1
            bx r2
            
            .pool
            
            
            
textentrymenutilehook:
            ldr r2,=0x8055c20 | 1
            mov lr,r2
            
            mov r2,#1
            lsl r2,#15
            cmp r5,r2
            bhs @@skip
            
            ;sub r5,#0x20
            add r5,r5
            ldr r2,=ascjistbl - (0x20*2)
            ldrh r5,[r2,r5]
            
@@skip:     str r5,[sp,#4]
            ;end
            ldr r4,[r4,#0x24]
            mov r2,r6
            bx r4
            
            .pool
            
            
            
            
textentrymenuspritehook:
            mov r7,r10
            mov r6,r9
            mov r5,r8
            push {r5-r7}
            
            
            mov r4,#1
            lsl r4,#15
            cmp r1,r4
            bhs @@skip
            
            ;sub r1,#0x20
            add r1,r1
            ldr r4,=ascjistbl - (0x20*2)
            ldrh r1,[r4,r1]
            
@@skip:     ;return
            sub sp,#0x88
            ldr r4,=0x8053100 | 1
            bx r4
            
            .pool
            
            
            
textentrymenuheaderhook:
            ldr r0,=0x80b1aae | 1
            mov lr,r0
            ldr r0,=0x80b18dc | 1
            push {r0}
            
            
            mov r0,#1
            lsl r0,#15
            cmp r6,r0
            bhs @@skip
            
            ;sub r6,#0x20
            add r6,r6
            ldr r0,=ascjistbl - (0x20*2)
            ldrh r3,[r0,r6]
            
@@skip:     ;return
            mov r0,r9
            mov r1,r8
            mov r2,r7
            ;mov r3,r6
            pop {pc}
            
            .pool
            
            
            
textentrymenuwritehook:
            ;at this point:
            ;[r4,#0] - pointer to string buffer
            ;[r4,#4] - allocated space in double-bytes, NOT max length
            ;[r4,#6] - current string length
            ;[r4,#0x20] - actual max length in double-bytes
            ;[r6,#0] - next char to write
            ;r0 and r1 are definitely free for use
            ;r2 and r3 are probably safe too
            
            ldrh r0, [r6] ;get char
            ldr r1, [r4,#0] ;get pointer
            ldrh r2, [r4,#6] ;get current index
            push {r4,r5}
            ldrh r5, [r4,#0x20] ;get max length
            add r5,r5
            
            ;the original game writes the char in the wrong byte order with strh
            lsr r3, r0,#8
            beq @@single
            
            add r4, r2,#1
            cmp r4, r5
            bhs @@skip
            strb r3, [r1,r2]
            add r2, #1
            b @@singlenocheck
            
@@single:   cmp r2, r5
            bhs @@skip
@@singlenocheck:
            strb r0, [r1,r2]
            add r2, #1
            
@@skip:     pop {r4,r5}
            strh r2, [r4,#6] ;save new index
            
            ldr r0,=0x8055fde | 1
            bx r0
            .pool
            
            
            
            
textentrymenudeletehook:
            ;[r4,#0] - pointer to string buffer
            ;[r4,#6] - current string length
            ;r0-r3 are safe
            
            ldr r0,=0x8056b06 | 1
            mov lr,r0
            ldr r0,=0x8055cb0 | 1
            push {r0}
            
            
            ldrh r2, [r4,#6]
            cmp r2,#1 ;only one byte?
            bls @@skip
            
            ; find the last char
            ldr r1, [r4,#0]
            mov r3,#0
@@chkloop:  ldrb r0, [r1,r3]
            cmp r0,#0x80
            blo @@chks
            add r3,#1
@@chks:     add r3,#1
            cmp r3,r2
            blo @@chkloop
            
@@afterchk: cmp r0,#0x80
            blo @@skip
            sub r2,#1
@@skip:     sub r2,#1
            strh r2, [r4,#6]
            
            ;return
            mov r0,r5
            pop {pc}
            
            .pool
            
            
            
            ;
            ;
            ;
            ; --- worker info hooks
            ;
            ;
            ;
            
            /*
workerinfodistancehook:
            ldr r0,=0x8087f40 | 1
            mov lr,r0
            ldr r0,=0x8097618 | 1
            push {r0}
            
            mov r0,#0x0f
            strh r0,[r3,#8]
            
            ;return
            ldr r2,[r7]
            mov r0,r3
            pop {pc}
            .pool
            
            
            ;at this point:
            ;r7 contains the current string
            ;r9 contains the current string index
            ;r0,r1, and r3 are safe for use
workerinfocharstephook:
            ldr r0,=0x80977bc | 1
            push {r0}
            
            mov r3,r9
            mov r1,#6 ;chars left in this packet
@@loop:     ldrb r0, [r7,r3]
            cmp r0,#0
            beq @@skip
            add r3,#1
            cmp r0,#0x80
            bcc @@next
            add r3,#1
@@next:     sub r1,#1
            bne @@loop
            
@@skip:     mov r9,r3
            
            ;return
            mov r3,#6
            ldrsh r0, [r5,r3]
            pop {pc}
            .pool
            
            */
            
            
            
    ;todo: figure out how the game itself converts asc->jis
ascjistbl:
            ;          !      "      #      $      %      &      '
   .halfword 0x8140,0x8149,0x8168,0x8194,0x8190,0x8193,0x8195,0x8166
            ;   (      )      *      +      ,      -      .      /
   .halfword 0x8169,0x816a,0x8196,0x817b,0x8143,0x817d,0x8144,0x815e
            ;   0      1      2      3      4      5      6      7
   .halfword 0x824f,0x8250,0x8251,0x8252,0x8253,0x8254,0x8255,0x8256
            ;   8      9      :      ;      <      =      >      ?
   .halfword 0x8257,0x8258,0x8146,0x8147,0x8183,0x8181,0x8184,0x8148
            ;   @      A      B      C      D      E      F      G
   .halfword 0x8197,0x8260,0x8261,0x8262,0x8263,0x8264,0x8265,0x8266
            ;   H      I      J      K      L      M      N      O
   .halfword 0x8267,0x8268,0x8269,0x826a,0x826b,0x826c,0x826d,0x826e
            ;   P      Q      R      S      T      U      V      W
   .halfword 0x826f,0x8270,0x8271,0x8272,0x8273,0x8274,0x8275,0x8276
            ;   X      Y      Z      [      \      ]      ^      _
   .halfword 0x8277,0x8278,0x8279,0x816d,0x815f,0x816e,0x814f,0x8151
            ;   `      a      b      c      d      e      f      g
   .halfword 0x814d,0x8281,0x8282,0x8283,0x8284,0x8285,0x8286,0x8287
            ;   h      i      j      k      l      m      n      o
   .halfword 0x8288,0x8289,0x828a,0x828b,0x828c,0x828d,0x828e,0x828f
            ;   p      q      r      s      t      u      v      w
   .halfword 0x8290,0x8291,0x8292,0x8293,0x8294,0x8295,0x8296,0x8297
            ;   x      y      z      {      |      }      ~
   .halfword 0x8298,0x8299,0x829a,0x816f,0x8162,0x8170,0x8160
            
            
            
            
            .align 4
           
            ;disable non-ascii mode
            ;this may not work, and need to be done on a case-by-case basis
            .org 0x8096f80
            mov r2,#0
            nop
            
            ;
            ;
            ;
            ;   ---------- text entry menu
            ;
            ;
            ;
            
            
            ;copy original string more directly on entry
            .org 0x8053988
            lsl r1,#8
            orr r1,r0
            .org 0x8053a14 ;count by bytes
            ldr r0,=textentrymenuinitcounthook | 1
            bx r0
            .pool
            
            
            ;change font used by header to thin3px
            .org 0x8054bfa ;main font
            bl 0x8098e74
            .org 0x8054b6a ;outlines
            bl 0x8098e74
            
            ;change font used by entered text display
            .org 0x8053f1c
            bl 0x8098e5c
            
            ;allow more characters in entered text display
            ;conveniently this appears to also allocate more memory for the text itself. woooo
            .org 0x8053fdc
            mov r3,#0x10
            
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
            
            
            ;convert asc->jis before drawing tiles
            .org 0x8055c16
            ldr r2,=textentrymenutilehook | 1
            bx r2
            .pool
            ;convert asc->jis before showing highlighted sprite
            .org 0x80530f6
            ldr r4,=textentrymenuspritehook | 1
            bx r4
            .pool
            ;convert asc->jis before showing header
            .org 0x80b1aa2
            ldr r0,=textentrymenuheaderhook | 1
            bx r0
            .pool
            
            ;disable the old max-length check, we do a better one
            .org 0x8055f82
            nop
            ;write char 1/2-bytes instead of always 2
            .org 0x8055fcc
            ldr r0,=textentrymenuwritehook | 1
            bx r0
            .pool
            ;delete, taking into account 1-byte chars
            .org 0x8056afa
            ldr r0,=textentrymenudeletehook | 1
            bx r0
            .pool
            
            
            ;expand string to double-bytes before displaying
            ;this may be small enough to not require a hook
            ;VERY tightly packed, in fact this can't fit even one more byte
            .org 0x8055cb0
            push {r4-r7,lr} ;save more regs
            .org 0x8055cb4
            .area 0x8055d14-org()
textentrymenucorrecthook:
            ;r0 contains the struct base pointer
            ;[r0,#0x94] old string pointer
            ;[r0,#0x94+6] string length
            ;r0,#0x94+8 direct new string pointer, but there's not enough memory so we just take the end of wram
            add r0,#0x94
            ldr r1, [r0]
            ldrh r2,[r0, #6]
            
            mov r0,#3 ;r0 = $2ffffe0
            lsl r0,#24
            sub r0,#0x20
            
            ;copy string
            ldr r6,=ascjistbl - (0x20*2)
            mov r3,#0 ;old index
            mov r4,#0 ;new index
@@copyloop: cmp r3,r2
            bhs @@copyend
            ldrb r5, [r1,r3]
            add r3,#1
            cmp r5,#0x80
            blo @@single
            strb r5, [r0,r4]
            add r4,#1
            ldrb r5, [r1,r3]
            strb r5, [r0,r4]
            add r3,#1
            add r4,#1
            b @@copyloop
@@single:   add r5,r5
            ldrh r5,[r6,r5]
            lsr r7, r5,#8
            strb r7,[r0,r4]
            add r4,#1
            strb r5,[r0,r4]
            add r4,#1
            b @@copyloop
@@copyend:  
            ;pad end with spaces
            mov r2,#0x81
            mov r3,#0x40
@@spaceloop:
            cmp r4,#0x20
            bhs @@spaceend
            strb r2,[r0,r4]
            add r4,#1
            strb r3,[r0,r4]
            add r4,#1
            b @@spaceloop
@@spaceend:
            
            
            mov r1,r0
            ;normal exit
            mov r2,r12
            ldr r0,[r2,#0x6c]
            bl 0x80b0850
            pop {r4-r7,pc}
            .pool
            .endarea
            
            
            ;copy correctly on exit
            ;todo expand with leading/trailing spaces cleanup
            .org 0x805637e
            .area 0x8056388 - org()
            add r1,#0x94
            
            ldrh r2, [r1,#6] ;length
            ldr r1, [r1] ;text pointer
            
            .if org() != 0x8056388
                b 0x8056388
            .endif
            .endarea
            .org 0x805639a ;don't shift index of null terminator
            nop
            
            
            ;new character arrangement
            ;note, you MUST have characters on the bottom row
            ;or else the game will break moving up from bottom buttons
            .org 0x80e6348
            .area 0x80e6648-org()
                     ;col, row, char
            .halfword 00,00, 'A', 0 ;A
            .halfword 01,00, 'B', 0 ;B
            .halfword 02,00, 'C', 0 ;C
            .halfword 03,00, 'D', 0 ;D
            .halfword 04,00, 'E', 0 ;E
            .halfword 05,00, 'F', 0 ;F
            .halfword 06,00, 'G', 0 ;G
            .halfword 07,00, 'H', 0 ;H
            .halfword 08,00, 'I', 0 ;I
            .halfword 09,00, 'J', 0 ;J
            .halfword 10,00, 'K', 0 ;K
            .halfword 11,00, 'L', 0 ;L
            
            .halfword 00,01, 'M', 0 ;M
            .halfword 01,01, 'N', 0 ;N
            .halfword 02,01, 'O', 0 ;O
            .halfword 03,01, 'P', 0 ;P
            .halfword 04,01, 'Q', 0 ;Q
            .halfword 05,01, 'R', 0 ;R
            .halfword 06,01, 'S', 0 ;S
            .halfword 07,01, 'T', 0 ;T
            .halfword 08,01, 'U', 0 ;U
            .halfword 09,01, 'V', 0 ;V
            .halfword 10,01, 'W', 0 ;W
            .halfword 11,01, 'X', 0 ;X
            
            .halfword 00,02, 'Y', 0 ;Y
            .halfword 01,02, 'Z', 0 ;Z
            .halfword 02,02, '0', 0 ;0
            .halfword 03,02, '1', 0 ;1
            .halfword 04,02, '2', 0 ;2
            .halfword 05,02, '3', 0 ;3
            .halfword 06,02, '4', 0 ;4
            .halfword 07,02, '5', 0 ;5
            .halfword 08,02, '6', 0 ;6
            .halfword 09,02, '7', 0 ;7
            .halfword 10,02, '8', 0 ;8
            .halfword 11,02, '9', 0 ;9
            
            ;.....
            
            .halfword 00,04, 'a', 0 ;a
            .halfword 01,04, 'b', 0 ;b
            .halfword 02,04, 'c', 0 ;c
            .halfword 03,04, 'd', 0 ;d
            .halfword 04,04, 'e', 0 ;e
            .halfword 05,04, 'f', 0 ;f
            .halfword 06,04, 'g', 0 ;g
            .halfword 07,04, 'h', 0 ;h
            .halfword 08,04, 'i', 0 ;i
            .halfword 09,04, 'j', 0 ;j
            .halfword 10,04, 'k', 0 ;k
            .halfword 11,04, 'l', 0 ;l
            
            .halfword 00,05, 'm', 0 ;m
            .halfword 01,05, 'n', 0 ;n
            .halfword 02,05, 'o', 0 ;o
            .halfword 03,05, 'p', 0 ;p
            .halfword 04,05, 'q', 0 ;q
            .halfword 05,05, 'r', 0 ;r
            .halfword 06,05, 's', 0 ;s
            .halfword 07,05, 't', 0 ;t
            .halfword 08,05, 'u', 0 ;u
            .halfword 09,05, 'v', 0 ;v
            .halfword 10,05, 'w', 0 ;w
            .halfword 11,05, 'x', 0 ;x
            
            .halfword 00,06, 'y', 0 ;y
            .halfword 01,06, 'z', 0 ;z
            .halfword 02,06, ' ', 0 ;space
            .halfword 03,06, '.', 0 ;.
            .halfword 04,06, ',', 0 ;,
            .halfword 05,06, '!', 0 ;!
            .halfword 06,06, '?', 0 ;?
            .halfword 07,06, ''', 0 ;'
            .halfword 08,06, '-', 0 ;-
            .halfword 09,06, '+', 0 ;+
            .halfword 10,06, '=', 0 ;=
            .halfword 11,06, '/', 0 ;/
            
            ;....
            
            .halfword 00,09, '@', 0 ;@
            .halfword 01,09, '#', 0 ;#
            ;todo choose something else (game uses $ for substitution)
            ;.halfword 02,09, '$', 0 ;$
            .halfword 03,09, '%', 0 ;%
            .halfword 04,09, '&', 0 ;&
            .halfword 05,09, '*', 0 ;*
            .halfword 06,09, '(', 0 ;(
            .halfword 07,09, ')', 0 ;)
            .halfword 08,09, '[', 0 ;[
            .halfword 09,09, ']', 0 ;]
            .halfword 10,09, '<', 0 ;<
            .halfword 11,09, '>', 0 ;>
            
            .halfword 00,10, 0x81f4, 0 ;musical note
            .halfword 01,10, 0x8189, 0 ;male
            .halfword 02,10, 0x818a, 0 ;female
            .halfword 03,10, 0x819a, 0 ;star
            .halfword 04,10, '~', 0 ;tilde
            .halfword 05,10, 0x8187, 0 ;infinity
            .halfword 06,10, 0x81a8, 0 ;right
            .halfword 07,10, 0x81a9, 0 ;left
            .halfword 08,10, 0x81aa, 0 ;up
            .halfword 09,10, 0x81ab, 0 ;down
            .halfword 10,10, 0x8180, 0 ;divide
            .halfword 11,10, '_', 0 ;underbar summer
            
            .endarea
            .fill 0x80e6648-org(), 0xff ;END OF LIST
            
            
            
            ;
            ;
            ;
            ; worker info menu
            ;
            ;
            ;
            .org 0x8087782 ;disable thick font
            nop
            nop
            mov r1,#0
            .org 0x80878ba
            nop
            nop
            mov r1,#0
            
            ;skip to the next text packet, taking into account ASCII
            .org 0x80977b4
            ;ldr r0,=workerinfocharstephook | 1
            ;bx r0
            .pool
            
            ;fix sprite widths to allow more chars (18 to be precise)
            .org 0x80e7454 + (4*5*1)
            ;.word 0x30
            .org 0x80e7454 + (4*5*3)
            ;.word 0x30
            .org 0x80e7454 + (4*5*5)
            ;.word 0x30
            .org 0x80e7454 + (4*5*7)
            ;.word 0x30
            .org 0x80e7454 + (4*5*8)
            ;.word 0x30
            
            ;activate inbuilt ascii mode
            .org 0x80e7464 + (4*5*1)
            .word 0
            .org 0x80e7464 + (4*5*3)
            .word 0
            .org 0x80e7464 + (4*5*5)
            .word 0
            .org 0x80e7464 + (4*5*7)
            .word 0
            .org 0x80e7464 + (4*5*8)
            .word 0
            
            ;fix sprite distances
            .org 0x8087f38
            ;ldr r2,=workerinfodistancehook | 1
            ;bx r2
            .pool
            
            
            
            
            
            ;
            ;
            ;
            ; word wrap trampoline
            ;
            ;
            ;
            .org 0x80afdde
            ldr r0, =wordwraphook | 1
            bx r0
            .pool
            
            
            
            
            ;
            ;
            ;
            ; ending credits
            ;
            ;
            ;
            
            ; make text allocated tiles wider
            .org 0x808b26e
            mov r3,#0x22
            
            ; disable loading of MFM $11abe8 and $11ac18, since it screws up the spacing
            .org 0x8098e8c
            mov r0,#0
            bx lr
            
            ; change font (fixes chars being always printed far apart)
            .org 0x808ae14
            bl 0x8098e5c
            
            ; clear string with ascii spaces instead of shift-jis spaces
            .org 0x80e76b8
            .fill 0x28,' '


            
            
            
            .close
            