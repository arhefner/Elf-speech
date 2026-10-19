; dict_scan.asm - look for a word in a block of the dictionary of tts
;
; The search through the entries of a block, the time critical part of
; dict_lookup, in assembly language, as ElfC code takes about a hundred
; times as long.
;
; The functions take their arguments from the expression stack of ElfC:
; R7 points one byte below the first argument, every argument is two
; bytes with the low byte at the lower address.  The result is returned
; in RA.  R7, R9 and RB are preserved, R8, RC, RD, RF and RE.0 are free.
; No subroutine of the ElfC runtime is called, so the library works in
; programs for Elf/OS and for ELF-DOS.
;
;   int  dict_scan(byte *block, char *word, byte **where)
;   void dict_hook(int (*poll)())
;
; A block is entries, each a word and its data, ended by FF:
;
;   shared      how many letters it shares with the word before
;   letters     the other letters, the last with bit 7 set
;   data        bytes, the last with bit 7 set
;
; The words are sorted, and the first one shares nothing.  See
; tools/mkdict.py, which makes the blocks, and has the same search in
; Python.

scall:      equ   r4                    ; standard call
sret:       equ   r5                    ; standard return

          ; how many entries are looked at between calls of the poll
          ; function

POLLWORDS:  equ   16


            proc  Cdict_scan

; int dict_scan(byte *block, char *word, byte **where)
;
; Look for a word, in upper case and ended by 0, in the entries of a
; block.  Sets *where to the data of the last entry that is not after the
; word, or to NULL if all are after it, and returns 1 if that entry is the
; word, or 0.
;
; M, the number of letters the word has in common with the entry before,
; is in R8.0, and RC points to letter M of the word.  An entry is before
; the word if it shares more letters than M with the entry before, and
; after it if it shares fewer.  Only an entry that shares M letters is
; compared with the word, from letter M.
;
;   RF    the next byte of the block
;   RC    letter M of the word
;   RD    the data of the last entry before the word
;   R8.0  M
;   R8.1  the entries to look at before the next call of the poll function
;   RE.0  the byte of the entry

            sex   r2

            inc   r7                    ; the block
            lda   r7
            plo   rf
            lda   r7
            phi   rf
            lda   r7                    ; the word
            plo   rc
            ldn   r7
            phi   rc
            dec   r7
            dec   r7
            dec   r7
            dec   r7

            ldi   0                     ; nothing in common yet, and no
            plo   r8                    ;  entry before the word
            plo   rd
            phi   rd
            ldi   POLLWORDS
            phi   r8

entry:      ghi   r8                    ; time to call the poll function?
            smi   1
            phi   r8
            lbnz  shared
            ldi   POLLWORDS
            phi   r8
            sep   scall
            dw    poll

shared:     lda   rf                    ; how many letters it shares
            plo   re
            xri   0ffh                  ; the end of the block?
            lbz   after
            glo   r8                    ; fewer than M, after the word
            str   r2
            glo   re
            sm
            lbnf  after
            lbnz  skipword              ; more than M, before the word

compare:    lda   rf                    ; compare the next letter
            plo   re
            ldn   rc
            lbz   after                 ; the word ends first, after it
            str   r2
            glo   re
            ani   7fh
            sm
            lbnf  before                ; a smaller letter, before it
            lbnz  after                 ; a greater one, after it

            inc   rc                    ; the same letter
            inc   r8
            glo   re
            ani   80h                   ; the last letter of the entry?
            lbz   compare
            ldn   rc                    ; yes, and the end of the word?
            lbnz  data                  ; no, so the entry is before it

            ghi   rf                    ; it is the word
            phi   rd
            glo   rf
            plo   rd
            ldi   1
            lbr   return

before:     glo   re                    ; skip the rest of the letters
            ani   80h
            lbnz  data
skipword:   lda   rf
            ani   80h
            lbz   skipword

data:       ghi   rf                    ; the entry is before the word,
            phi   rd                    ;  remember its data
            glo   rf
            plo   rd
skipdata:   lda   rf                    ; and skip it
            ani   80h
            lbz   skipdata
            lbr   entry

after:      ldi   0                     ; the word is not there

return:     plo   ra                    ; the result
            ldi   0
            phi   ra

            inc   r7                    ; set *where
            inc   r7
            inc   r7
            inc   r7
            inc   r7
            lda   r7
            plo   rf
            ldn   r7
            phi   rf
            dec   r7
            dec   r7
            dec   r7
            dec   r7
            dec   r7
            dec   r7
            glo   rd
            str   rf
            inc   rf
            ghi   rd
            str   rf

            sep   sret


; void dict_hook(int (*poll)())
;
; Set the function that dict_scan calls every few entries, or NULL for
; none.

            public Cdict_hook

Cdict_hook: sex   r2

            inc   r7                    ; get the function
            lda   r7
            plo   re
            ldn   r7
            dec   r7
            dec   r7
            phi   rc

            ldi   hook.1                ; and put it in the call to it,
            phi   rf                    ;  high byte first
            ldi   hook.0
            plo   rf
            ghi   rc
            str   rf
            inc   rf
            glo   re
            str   rf

            sep   sret


; Call the poll function, if there is one.  The registers of the search
; are saved, and R7, R9 and RB are those of ElfC.

poll:       glo   rf                    ; save the registers
            stxd
            ghi   rf
            stxd
            glo   rc
            stxd
            ghi   rc
            stxd
            glo   rd
            stxd
            ghi   rd
            stxd
            glo   r8
            stxd
            ghi   r8
            stxd

            ldi   hook.1                ; is there a function?
            phi   rf
            ldi   hook.0
            plo   rf
            lda   rf
            str   r2
            ldn   rf
            or
            lbz   nopoll

            sep   scall                 ; call it
hook:       dw    0
            sex   r2

nopoll:     irx                         ; restore the registers
            ldxa
            phi   r8
            ldxa
            plo   r8
            ldxa
            phi   rd
            ldxa
            plo   rd
            ldxa
            phi   rc
            ldxa
            plo   rc
            ldxa
            phi   rf
            ldx
            plo   rf

            sep   sret

            endp
