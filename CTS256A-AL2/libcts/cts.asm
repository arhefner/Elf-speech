; cts.asm - the text to allophone conversion of the CTS256A-AL2, for ElfC
;
; The CTS256A-AL2 is a TMS7000 microcontroller with a program in ROM that
; turns English text into the allophones of the SP0256-AL2 with the letter
; to sound rules of the Naval Research Laboratory.  This is that program,
; written for the 1802 from the disassembly of the ROM.  It uses the rules
; of the ROM, in rules.inc, and the same steps as the ROM, so it gives the
; same allophones as the chip, also where the ROM has a bug.  It is in
; assembly language because the ElfC code for it is about a hundred times
; slower.
;
; The functions take their arguments from the expression stack of ElfC:
; R7 points one byte below the first argument, every argument is two
; bytes with the low byte at the lower address.  The result is returned
; in RA.  R7, R9 and RB are preserved, R8, RC, RD, RF and RE.0 are free.
; No subroutine of the ElfC runtime is called, so the library works in
; programs for Elf/OS and for ELF-DOS.
;
;   void cts_init(byte **exceptions)
;   void cts_feed(int c)
;   void cts_finish(void)
;   int  cts_step(byte *allophones)
;   void cts_poll(int (*poll)())
;
; See cts.h for what they do.
;
; THE INPUT
;
; The text is kept as the chip keeps it: the characters in upper case,
; with bit 7 set on the characters that end a word, which are all but the
; letters, the digits, ' and [\]^_`{.  It is a ring of 256 bytes on a page
; of its own, so that an index into it is the low byte of a pointer, which
; wraps around by itself.  The ring keeps the text before the word being
; converted, which a left context can look at, and the word after it, which
; a right context can look at, as in <[THE]<# for "the apple".  So when a
; word ends, cts_feed lets the words before it be converted, but not that
; word, until the next one ends too.
;
; THE RULES
;
; Each character has a list of rules, the letters A to Z, the digits, and
; the rest.  A rule is a left context, the characters in brackets, a right
; context and the allophones:
;
;   #:[E]D< = (nothing)       the E of "dragged"
;
; The first byte in brackets has bit 6 set and the last one bit 7, and so
; have the first and the last allophone.  A byte with both bits is the only
; one.  In brackets, a byte is the code of a character, which is its ASCII
; value less 20h.  The first letter of the brackets of a letter is left out,
; so a letter on its own is FF.  The allophones FF are no allophones at all.
; A context is codes of characters (07, 15 and above) and patterns:
;
;   09 #  one or more vowels             0F +  E I Y
;   0A .  B D G J L M N R V W Z          10 :  zero or more consonants
;   0B %  E ER ERS ES ED ELY ING MENT OR 11 *  one or more consonants
;         at the end of a word           12 >  O U
;   0C &  C G J S X Z, or CH or SH       13 <  anything but a letter
;   0D @  D J L N R S T Z, or TH CH SH   14 ?  two or more vowels
;   0E ^  one consonant
;
; The rules of a character are tried in order, and the first one that
; matches gives the allophones.  The characters in brackets are matched
; first, then the right context from left to right, then the left context
; from right to left.  The patterns look at the class of the letters, the
; classes table.
;
; A list of exception words of an exception EPROM is the same, except that
; the brackets of a letter on its own, FF, end the list.  At the start of a
; word, the word is looked for in the exceptions of its first character
; before the rules.  If an exception matches up to the end of the word,
; the next word starts after it, otherwise the rules carry on with the
; rest of the word.
;
; REGISTERS
;
; While a rule is matched, the registers hold what the registers of the
; chip that are in brackets hold:
;
;   RF    the byte of the rule (R21)
;   RD    the next character to the right in the ring (R3)
;   RC    the next character to the left in the ring (R17)
;   R8    the variables, R8.1 is their page
;   R9    where the next allophone goes
;   RA.0  the code of the last character of a context (R22)
;   RA.1  its class
;   RB.0  the flags, F_LEFT, F_LETTER and F_EXC
;   RB.1  the class a pattern skips, and the number of allophones
;
; The other state is in the variables.

scall:      equ   r4                    ; standard call
sret:       equ   r5                    ; standard return

          ; the longest word, which is cut there, as the chip does when its
          ; input buffer is full

LONGEST:    equ   200

          ; the most allophones of a rule that are given back

MAXALLO:    equ   128

          ; how many rules are tried between calls of the poll function

POLLRULES:  equ   8

          ; the flags in RB.0

F_LEFT:     equ   1                     ; matching the left context
F_LETTER:   equ   2                     ; the character is a letter
F_EXC:      equ   4                     ; the rule is an exception word

          ; the patterns of the contexts

P_VOWELS:   equ   09h                   ; #
P_VOICED:   equ   0ah                   ; .
P_SUFFIX:   equ   0bh                   ; %
P_SIBIL:    equ   0ch                   ; &
P_LONGU:    equ   0dh                   ; @
P_CONS:     equ   0eh                   ; ^
P_FRONT:    equ   0fh                   ; +
P_CONS0:    equ   10h                   ; :
P_CONS1:    equ   11h                   ; *
P_BACK:     equ   12h                   ; >
P_NONLET:   equ   13h                   ; <
P_VOWELS2:  equ   14h                   ; ?
P_LITERAL:  equ   15h                   ; this and above are characters

          ; the classes of the letters

C_VOWEL:    equ   80h                   ; A E I O U Y
C_VOICED:   equ   40h                   ; B D G J L M N R V W Z
C_SIBIL:    equ   20h                   ; C G J S X Z
C_LONGU:    equ   10h                   ; D J L N R S T Z
C_CONS:     equ   08h                   ; all letters but the vowels
C_FRONT:    equ   04h                   ; E I Y
C_BACK:     equ   02h                   ; O U
C_E:        equ   01h                   ; E, the start of a suffix

.link .align page

            proc  Ccts

          ; The ring of the input, which is on a page of its own.  The
          ; procedure is aligned to a page for it.  The code is after it,
          ; then the variables, the rules and the classes of the letters.

ring:       ds    256



; void cts_init(byte **exceptions)
;
; Start the conversion, with the lists of exception words, or with none
; if exceptions is NULL.

            public Ccts_init

Ccts_init:  sex   r2

            ldi   vars.1                ; point to the variables
            phi   r8

            inc   r7                    ; store the lists of exceptions
            lda   r7
            plo   re
            ldn   r7
            dec   r7
            dec   r7
            str   r2
            ldi   v_exc.0
            plo   r8
            ldx
            str   r8
            inc   r8
            glo   re
            str   r8

            ldi   ring.1                ; clear the ring
            phi   rd
            ldi   0
            plo   rd
clear:      ldi   0
            str   rd
            inc   rd
            glo   rd
            lbnz  clear

            ldi   ring.1                ; the chip starts with a space
            phi   rd
            ldi   ' '
            str   rd

            ldi   v_wr.0                ; the text starts after it
            plo   r8
            ldi   1
            str   r8                    ; v_wr
            inc   r8
            str   r8                    ; v_rd
            inc   r8
            str   r8                    ; v_prev
            inc   r8
            str   r8                    ; v_stop
            inc   r8
            str   r8                    ; v_word

            sep   sret


; void cts_feed(int c)
;
; Add a character to the text.  When it ends a word, the words before
; that word can be converted.

            public Ccts_feed

Ccts_feed:  sex   r2

            inc   r7                    ; get the character
            ldn   r7
            dec   r7
            plo   rf

            sep   scall                 ; and add it
            dw    feed

            sep   sret


; void cts_finish(void)
;
; End the text with a CR, as the chip gets at the end, and let all of it
; be converted.

            public Ccts_finish

Ccts_finish: sex  r2

            ldi   0dh                   ; add the CR
            plo   rf
            sep   scall
            dw    feed

            ldi   vars.1                ; and convert up to the end
            phi   r8
            ldi   v_wr.0
            plo   r8
            ldn   r8
            plo   re
            ldi   v_stop.0
            plo   r8
            glo   re
            str   r8
            ldi   v_prev.0
            plo   r8
            glo   re
            str   r8

            sep   sret


; Add the character in RF.0 to the ring, as the chip does when it gets
; it, in upper case, and with bit 7 set if it ends a word.

feed:       glo   rf                    ; make a-z upper case
            smi   'a'
            lbnf  upper
            smi   26
            lbdf  upper
            glo   rf
            smi   20h
            plo   rf

upper:      glo   rf                    ; ' does not end a word
            xri   27h
            lbz   stored
            glo   rf                    ; nor do 0-9
            smi   '0'
            lbnf  ends
            smi   10
            lbnf  stored
            glo   rf                    ; nor A-Z [\]^_` {
            smi   'A'
            lbnf  ends
            smi   '{'-'A'+1
            lbnf  stored

ends:       glo   rf                    ; but all other characters do
            ori   80h
            plo   rf

stored:     ldi   vars.1                ; put it in the ring
            phi   r8
            ldi   ring.1
            phi   rd
            ldi   v_wr.0
            plo   r8
            ldn   r8
            plo   rd
            glo   rf
            str   rd
            inc   rd                    ; RD.1 is not used after this
            glo   rd
            str   r8                    ; v_wr

            glo   rf                    ; the end of a word?
            ani   80h
            lbnz  wordend

            ldi   v_rd.0                ; no, but is the word too long?
            plo   r8
            ldn   r8
            str   r2
            glo   rd
            sm
            smi   LONGEST
            lbnf  sentinel

            ldi   ring.1                ; then end it with a CR, as the chip
            phi   rd                    ;  does when its buffer is full,
            ldi   8dh                   ;  and convert all of it
            str   rd
            inc   rd
            ldi   v_wr.0
            plo   r8
            glo   rd
            str   r8
            ldi   v_stop.0
            plo   r8
            glo   rd
            str   r8
            lbr   newprev

wordend:    ldi   v_prev.0              ; convert the words before this one
            plo   r8
            ldn   r8
            plo   re
            ldi   v_stop.0
            plo   r8
            glo   re
            str   r8

newprev:    ldi   v_prev.0              ; this word is now the last one
            plo   r8
            glo   rd
            str   r8

sentinel:   ldi   ring.1                ; a right context that looks past
            phi   rd                    ;  the end of the text finds zeros
            ldi   0
            str   rd
            glo   rd
            adi   1
            plo   rd
            ldi   0
            str   rd

            sep   sret


; int cts_step(byte *allophones)
;
; Convert the next character or characters of the text with a rule,
; and put the allophones of the rule in the array.  Returns how many
; there are, which can be 0, or -1 if there is nothing to convert.

            public Ccts_step

Ccts_step:  sex   r2

            ldi   vars.1                ; point to the variables
            phi   r8

            ldi   v_wr.0                ; is there anything to convert, is
            plo   r8                    ;  wr - rd greater than wr - stop?
            ldn   r8
            plo   re
            ldi   v_stop.0
            plo   r8
            ldn   r8
            str   r2
            glo   re
            sm
            plo   rf                    ; wr - stop
            ldi   v_rd.0
            plo   r8
            ldn   r8
            str   r2
            glo   re
            sm
            str   r2                    ; wr - rd
            glo   rf
            sm
            lbnf  convert

            ldi   0ffh                  ; no, return -1
            phi   ra
            plo   ra
            sep   sret

convert:    glo   r9                    ; save the registers of ElfC,
            stxd                        ;  on the stack and for the poll
            ghi   r9                    ;  function
            stxd
            glo   rb
            stxd
            ghi   rb
            stxd
            ldi   v_r9.0
            plo   r8
            ghi   r9
            str   r8
            inc   r8
            glo   r9
            str   r8
            inc   r8
            ghi   rb
            str   r8
            inc   r8
            glo   rb
            str   r8

            inc   r7                    ; where the allophones go
            lda   r7
            plo   r9
            ldn   r7
            phi   r9
            dec   r7
            dec   r7

            ldi   ring.1                ; RD and RC point into the ring
            phi   rd
            phi   rc
            ldi   v_rd.0
            plo   r8
            ldn   r8
            plo   rd

            ldi   0                     ; no allophones yet
            plo   rb
            phi   rb

            ldi   v_word.0              ; at the start of a word, look
            plo   r8                    ;  for it in the exceptions
            ldn   r8
            lbz   byrules
            ldi   v_exc.0
            plo   r8
            lda   r8
            phi   rf
            ldn   r8
            plo   rf
            lbnz  except
            ghi   rf
            lbz   byrules

except:     ldi   v_start.0             ; remember where the word starts
            plo   r8
            glo   rd
            str   r8

            sep   scall                 ; read its first character
            dw    first

            smi   26                    ; the digits share the list
            lbnf  excidx                ;  of the other characters
            ldi   0
excidx:     adi   26                    ; the address of the list, low
            shl                         ;  byte first
            str   r2
            glo   rf
            add
            plo   rf
            ghi   rf
            adci  0
            phi   rf
            lda   rf
            plo   re
            ldn   rf
            phi   rf
            glo   re
            plo   rf

            glo   rb                    ; match it
            ori   F_EXC
            plo   rb
            sep   scall
            dw    apply
            lbdf  done

            ldi   vars.1                ; not an exception, so convert it
            phi   r8                    ;  with the rules
            ldi   v_start.0
            plo   r8
            ldn   r8
            plo   rd

byrules:    glo   rb                    ; not an exception
            ani   0ffh-F_EXC
            plo   rb

            sep   scall                 ; read the character
            dw    first

            shl                         ; the address of its list
            adi   lists.0
            plo   r8
            lda   r8
            phi   rf
            ldn   r8
            plo   rf

            sep   scall                 ; and convert it
            dw    apply

done:       ldi   vars.1                ; save where the next one is
            phi   r8
            ldi   v_rd.0
            plo   r8
            glo   rd
            str   r8

            ldi   0                     ; return the number of allophones
            phi   ra
            ghi   rb
            plo   ra

            irx                         ; restore the registers of ElfC
            ldxa
            phi   rb
            ldxa
            plo   rb
            ldxa
            phi   r9
            ldx
            plo   r9

            sep   sret


; void cts_poll(int (*poll)())
;
; Set a function that cts_step calls every few rules while it converts,
; so that a program can give the SP0256-AL2 the next allophone as soon as
; it can take it, as the chip does with an interrupt.  NULL for none.

            public Ccts_poll

Ccts_poll:  sex   r2

            inc   r7                    ; get the function
            lda   r7
            plo   re
            ldn   r7
            dec   r7
            dec   r7
            phi   rf
            glo   re
            plo   rf

            ldi   hook.1                ; put it in the call to it
            phi   rd
            ldi   hook.0
            plo   rd
            ghi   rf
            str   rd
            inc   rd
            glo   rf
            str   rd

            ldi   vars.1                ; and remember if there is one
            phi   r8
            ldi   v_hook.0
            plo   r8
            glo   rf
            str   r2
            ghi   rf
            or
            str   r8

            sep   sret


; Call the poll function, if there is one.  The registers of the
; conversion are saved, and R9 and RB are those of ElfC while it runs.

poll:       ldi   v_hook.0
            plo   r8
            ldn   r8
            lbz   nopoll

            glo   rf                    ; save the registers
            stxd
            ghi   rf
            stxd
            glo   rd
            stxd
            ghi   rd
            stxd
            glo   rc
            stxd
            ghi   rc
            stxd
            glo   ra
            stxd
            ghi   ra
            stxd
            glo   r9
            stxd
            ghi   r9
            stxd
            glo   rb
            stxd
            ghi   rb
            stxd

            ldi   v_r9.0                ; R9 and RB of ElfC
            plo   r8
            lda   r8
            phi   r9
            lda   r8
            plo   r9
            lda   r8
            phi   rb
            ldn   r8
            plo   rb

            sep   scall                 ; call the function
hook:       dw    0

            irx                         ; restore the registers
            ldxa
            phi   rb
            ldxa
            plo   rb
            ldxa
            phi   r9
            ldxa
            plo   r9
            ldxa
            phi   ra
            ldxa
            plo   ra
            ldxa
            phi   rc
            ldxa
            plo   rc
            ldxa
            phi   rd
            ldxa
            plo   rd
            ldxa
            phi   rf
            ldx
            plo   rf

            ldi   vars.1                ; R8 points to the variables again
            phi   r8

nopoll:     sep   sret


; Read the character at RD to convert, with RC on the one before it.
; Sets v_word if it ends a word, and F_LETTER if it is a letter.
; Returns the number of its list in D: 0 to 25 for A to Z, 26 for the
; digits and 27 for the others.

first:      glo   rd                    ; the left context starts before it
            smi   1
            plo   rc

            lda   rd                    ; get the character
            plo   re
            ldi   ring.1
            phi   rd

            ldi   v_word.0              ; does it end a word?
            plo   r8
            glo   re
            ani   80h
            str   r8

            glo   rb                    ; a letter?
            ani   0ffh-F_LETTER
            plo   rb
            glo   re
            ani   7fh
            plo   re
            ani   0dfh
            smi   'A'
            lbnf  notlet
            smi   26
            lbdf  notlet
            adi   26                    ; yes, the list is A to Z
            plo   re
            glo   rb
            ori   F_LETTER
            plo   rb
            glo   re
            sep   sret

notlet:     glo   re                    ; a digit?
            smi   '0'
            lbnf  other
            smi   10
            lbdf  other
            ldi   26
            sep   sret

other:      ldi   27                    ; something else
            sep   sret


; Apply the rules of the list at RF to the character at RD - 1, and put
; the allophones of the first one that matches at R9.  Returns DF set, or
; DF clear if it is a list of exceptions and none of them matched.

apply:      ldi   1                     ; find the first rule
            sep   scall
            dw    find

try:        ldi   v_poll.0              ; time to call the poll function?
            plo   r8
            ldn   r8
            smi   1
            str   r8
            lbnz  trybr
            ldi   POLLRULES
            str   r8
            sep   scall
            dw    poll

trybr:      sep   scall                 ; do the brackets match?
            dw    brackets
            lbdf  inbrackets

next2:      inc   rf                    ; no, the brackets of the next rule
            ldi   2
            sep   scall
            dw    find
            lbr   try

inbrackets: glo   rb                    ; the end of a list of exceptions?
            ani   F_EXC
            lbz   rightctx
            ldn   rf
            xri   0ffh
            lbnz  rightctx
            adi   0                     ; yes, return DF clear
            sep   sret

rightctx:   ldi   v_ls.0                ; remember where the left context is
            plo   r8
            glo   rc
            str   r8

            glo   rb                    ; match the right context
            ani   0ffh-F_LEFT
            plo   rb
            sep   scall
            dw    context
            lbdf  back

            sep   scall                 ; no match, try the next rule
            dw    restore
            lbr   next2

back:       ldn   rf                    ; back to the start of the brackets
            ani   40h
            lbnz  leftctx
            dec   rf
            lbr   back

leftctx:    glo   rb                    ; match the left context
            ori   F_LEFT
            plo   rb
            sep   scall
            dw    context
            lbdf  matched

            sep   scall                 ; no match, try the rule after
            dw    restore               ;  the brackets and allophones
            ldi   3
            sep   scall
            dw    find
            lbr   try

matched:    ldi   2                     ; the allophones of the rule
            sep   scall
            dw    find
            ldi   0                     ; RB.1 counts them
            phi   rb

allo:       lda   rf                    ; copy them
            plo   re
            xri   0ffh
            lbz   applied
            ghi   rb
            smi   MAXALLO
            lbdf  full
            glo   re
            ani   3fh
            str   r9
            inc   r9
            ghi   rb
            adi   1
            phi   rb
full:       glo   re
            ani   80h
            lbz   allo

applied:    ldi   vars.1                ; carry on after the brackets
            phi   r8
            ldi   v_re.0
            plo   r8
            ldn   r8
            plo   rd
            smi   0                     ; return DF set
            sep   sret


; Put RD and RC back as they were before a rule that did not match.

restore:    ldi   vars.1
            phi   r8
            ldi   v_rs.0
            plo   r8
            ldn   r8
            plo   rd
            ldi   v_ls.0
            plo   r8
            ldn   r8
            plo   rc
            sep   sret


; Move RF to the Dth byte from it that has bit 6 set, the start of the
; brackets or the allophones of a rule.

find:       plo   re
findnext:   ldn   rf
            ani   40h
            lbz   findskip
            glo   re
            smi   1
            plo   re
            lbz   found
findskip:   inc   rf
            lbr   findnext
found:      sep   sret


; Match the characters in brackets of the rule at RF with the text at
; RD.  The first character, at RD - 1, is not in the brackets of a
; letter.  Returns DF set if they match, with RD after them and RF on the
; last byte in brackets, otherwise DF clear and RD as it was.

brackets:   ldi   v_rs.0                ; remember where the text was
            plo   r8
            glo   rd
            str   r8

            ldn   rf                    ; a letter on its own?
            xri   0ffh
            lbz   bmatch

            glo   rb                    ; the brackets of the other
            ani   F_LETTER              ;  characters have the first one
            lbnz  bnext
            dec   rd
            ldi   ring.1
            phi   rd

bnext:      lda   rd                    ; the code of the next character
            plo   re
            ldi   ring.1
            phi   rd
            glo   re
            sep   scall
            dw    tocode
            str   r2

            ldn   rf                    ; is it the one in brackets?
            ani   3fh
            xor
            lbnz  bfail

            ldn   rf                    ; yes, the last one?
            ani   80h
            lbnz  bmatch
            inc   rf
            lbr   bnext

bmatch:     ldi   v_re.0                ; they match
            plo   r8
            glo   rd
            str   r8
            smi   0
            sep   sret

bfail:      ldi   v_rs.0                ; they do not
            plo   r8
            ldn   r8
            plo   rd
            adi   0
            sep   sret


; The code of the character in D, which is its ASCII value in upper case
; less 20h.

tocode:     ani   7fh
            smi   61h
            lbdf  tclower
            adi   41h
            sep   sret
tclower:    adi   21h
            sep   sret


; Read the next character of the context, to the right at RD or to the
; left at RC.  Returns its class in D and RA.1, and its code in RA.0.

nextch:     glo   rb
            ani   F_LEFT
            lbz   nright
            ldn   rc
            dec   rc
            lbr   ncode
nright:     lda   rd
ncode:      plo   re
            ldi   ring.1
            phi   rd
            phi   rc
            glo   re
            ani   7fh
            smi   61h
            lbdf  nlower
            adi   41h
            lbr   nclass
nlower:     adi   21h
nclass:     plo   ra

            smi   21h                   ; the class of a letter
            lbnf  nonlet
            smi   26
            lbdf  nonlet
            adi   (classes+26).0
            plo   r8
            ldn   r8
            phi   ra
            sep   sret

nonlet:     ldi   0                     ; and of anything else
            phi   ra
            sep   sret


; Put back the last character of the context.

unread:     glo   rb
            ani   F_LEFT
            lbz   unright
            inc   rc
            lbr   unring
unright:    dec   rd
unring:     ldi   ring.1
            phi   rd
            phi   rc
            sep   sret


; Read characters of the context while they are of the class in RB.1,
; then put back the one that is not.

skip:       sep   scall
            dw    nextch
            str   r2
            ghi   rb
            and
            lbnz  skip
            sep   scall
            dw    unread
            sep   sret


; Read the next character to the right, without bit 7, into D and RA.1.

raw:        lda   rd
            ani   7fh
            phi   ra
            ldi   ring.1
            phi   rd
            ghi   ra
            sep   sret


; Match a context of the rule at RF, the right context if F_LEFT is
; clear, after the brackets, or the left context, from right to left
; before the brackets.  Returns DF set if it matches, with RF on its last
; byte, otherwise DF clear.

context:    glo   rb
            ani   F_LEFT
            lbz   cright

            dec   rf                    ; the next byte to the left, the
            ldn   rf                    ;  end of the allophones of the
            ani   80h                   ;  rule before ends it
            lbz   cchar
            inc   rf
            smi   0
            sep   sret

cright:     inc   rf                    ; the next byte to the right, the
            ldn   rf                    ;  start of the allophones ends it
            ani   40h
            lbz   cchar
            dec   rf
            smi   0
            sep   sret

cfail:      adi   0                     ; no match
            sep   sret

cchar:      sep   scall                 ; read a character of the text
            dw    nextch

            ldn   rf                    ; a character to match?
            xri   07h
            lbz   cliteral
            ldn   rf
            smi   P_LITERAL
            lbnf  cpattern

cliteral:   glo   ra                    ; then it is that character
            str   r2
            ldn   rf
            xor
            lbz   context
            lbr   cfail

cpattern:   ldn   rf                    ; otherwise a pattern
            xri   P_NONLET
            lbz   pnonlet
            ldn   rf
            xri   P_CONS0
            lbz   pcons0
            ldn   rf
            xri   P_VOWELS
            lbz   pvowels
            ldn   rf
            xri   P_CONS1
            lbz   pcons1
            ldn   rf
            xri   P_CONS
            lbz   pcons
            ldn   rf
            xri   P_FRONT
            lbz   pfront
            ldn   rf
            xri   P_VOICED
            lbz   pvoiced
            ldn   rf
            xri   P_SUFFIX
            lbz   psuffix
            ldn   rf
            xri   P_BACK
            lbz   pback
            ldn   rf
            xri   P_VOWELS2
            lbz   pvowels2
            ldn   rf
            xri   P_SIBIL
            lbz   psibil
            ldn   rf
            xri   P_LONGU
            lbz   plongu
            lbr   cfail                 ; the chip goes astray on others

pnonlet:    ghi   ra                    ; < anything but a letter
            lbnz  cfail
            lbr   context

pcons0:     ghi   ra                    ; : zero or more consonants
            ani   C_CONS
            lbnz  pskipcons
            sep   scall
            dw    unread
            lbr   context

pcons1:     ghi   ra                    ; * one or more consonants
            ani   C_CONS
            lbz   cfail
pskipcons:  ldi   C_CONS
pskip:      phi   rb
            sep   scall
            dw    skip
            lbr   context

pvowels2:   ghi   ra                    ; ? two or more vowels
            ani   C_VOWEL
            lbz   cfail
            sep   scall
            dw    nextch
pvowels:    ghi   ra                    ; # one or more vowels
            ani   C_VOWEL
            lbz   cfail
            ldi   C_VOWEL
            lbr   pskip

pcons:      ldi   C_CONS                ; ^ one consonant
            lbr   pclass
pfront:     ldi   C_FRONT               ; + E I Y
            lbr   pclass
pvoiced:    ldi   C_VOICED              ; . B D G J L M N R V W Z
            lbr   pclass
pback:      ldi   C_BACK                ; > O U
pclass:     str   r2
            ghi   ra
            and
            lbz   cfail
            lbr   context

psibil:     ghi   ra                    ; & C G J S X Z, or CH SH
            ani   C_SIBIL
            lbnz  context
            ldi   23h                   ; (C again)
            lbr   digraph

plongu:     ghi   ra                    ; @ D J L N R S T Z, or TH CH SH
            ani   C_LONGU
            lbnz  context
            ldi   34h                   ; T

          ; The CH and SH of &, and the TH, CH and SH of @: the next
          ; character has to be C, S or the one in D, and then the chip
          ; looks for the H two characters to the right of RC.  In a left
          ; context, that is the first character of the context, the H.
          ; In a right context, RC is still before the brackets, so the
          ; chip looks in the wrong place, and so does this.

digraph:    plo   re
            ldi   v_other.0
            plo   r8
            glo   re
            str   r8
            sep   scall
            dw    nextch
            glo   ra
            xri   23h                   ; C
            lbz   dgh
            glo   ra
            xri   33h                   ; S
            lbz   dgh
            ldi   v_other.0
            plo   r8
            ldn   r8
            str   r2
            glo   ra
            xor
            lbnz  cfail

dgh:        glo   rc                    ; then H at RC + 2, and the text
            adi   2                     ;  carries on after it
            plo   rd
            sep   scall
            dw    raw
            xri   'H'
            lbnz  cfail
            lbr   context

          ; % one of the suffixes E ER ERS ES ED ELY ING MENT OR, which are
          ; not followed by a letter.  The chip reads them to the right of
          ; RD, also in a left context.

psuffix:    ghi   ra                    ; E?
            ani   C_E
            lbz   sufnote
            sep   scall
            dw    raw
            xri   'R'
            lbnz  sufnotr
            sep   scall                 ; ER, ERS
            dw    raw
            xri   'S'
            lbz   sufend
            lbr   sufback
sufnotr:    ghi   ra                    ; ELY
            xri   'L'
            lbnz  sufnotl
            sep   scall
            dw    raw
            xri   'Y'
            lbnz  cfail
            lbr   sufend
sufnotl:    ghi   ra                    ; ES, ED
            xri   'S'
            lbz   sufend
            ghi   ra
            xri   'D'
            lbz   sufend
sufback:    dec   rd                    ; E
            ldi   ring.1
            phi   rd
            lbr   sufend

sufnote:    glo   ra                    ; ING
            xri   'I'-20h
            lbnz  sufnoti
            sep   scall
            dw    raw
            xri   'N'
            lbnz  cfail
            lbr   sufg
sufnoti:    glo   ra                    ; MENT
            xri   'M'-20h
            lbnz  sufnotm
            sep   scall
            dw    raw
            xri   'E'
            lbnz  cfail
            sep   scall
            dw    raw
            xri   'N'
            lbnz  cfail
            ldi   'T'
            lbr   suflast
sufnotm:    glo   ra                    ; OR
            xri   'O'-20h
            lbnz  cfail
            ldi   'R'
            lbr   suflast
sufg:       ldi   'G'
suflast:    plo   re
            ldi   v_other.0
            plo   r8
            glo   re
            str   r8
            sep   scall
            dw    raw
            str   r2
            ldi   v_other.0
            plo   r8
            ldn   r8
            xor
            lbnz  cfail

sufend:     sep   scall                 ; the next character must not be
            dw    raw                   ;  a letter
            dec   rd
            ldi   ring.1
            phi   rd
            ghi   ra
            ani   0dfh
            smi   'A'
            lbnf  context
            smi   26
            lbdf  context
            lbr   cfail

          ; The variables, on a page after the code.  The classes and the
          ; lists are on it too, so that they can be indexed with the low
          ; byte of R8.

            ds    (256-($ & 255)) & 255

vars:
v_wr:       db    1                     ; where the next character goes
v_rd:       db    1                     ; the next character to convert
v_prev:     db    1                     ; after the word before the last one
v_stop:     db    1                     ; convert up to here
v_word:     db    1                     ; at the start of a word
v_start:    db    0                     ; the start of the word
v_rs:       db    0                     ; rd at the start of the brackets (R19)
v_re:       db    0                     ; rd after the brackets (R15)
v_ls:       db    0                     ; lc before the contexts (R29)
v_other:    db    0                     ; the other letter of a digraph
v_exc:      dw    0                     ; the lists of exceptions, or 0
v_r9:       dw    0                     ; R9 and RB of ElfC, for the poll
v_rb:       dw    0                     ;  function
v_poll:     db    POLLRULES             ; rules to try before the next call
v_hook:     db    0                     ; there is a poll function

#include    rules.inc

            endp
