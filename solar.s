#
# Constants
#
      ; portB setup
      .set PORTB,0x05
      .set DDIRB,0x04
      .set MOTDATA,0
      .set MOTLATCH,4
      .set M1ENABLE,3
      .set BOARDLED, 5
      
      ; portD setup
      .set PORTD,0x0B
      .set DDIRD,0x0A
      .set MOTCLOCK,4
      .set M2ENABLE,3
      .set M4ENABLE,6
      .set M3ENABLE,5
      
      .set  PINB,0x03            
      .set  WESTSWITCH,2 
      .set  EASTSWITCH,1
#
# External Symbols
#
	.extern delay

#
# Global Data 
#

      .data
      .comm west,1
      .global west
      .comm east,1
      .global east
      .comm temp,1
      .global temp
      .comm westDigit,1
      .global westDigit

#
# Program Code
#

      .text
      .global operatePanel, initializePanel, readWest, readEast, reset, done
      
#
# INITIALIZE EVERY THING
#

initializePanel:
      out   0x04, r20         ; initialize board LED
      call  motorInitiate     ; initialize motor
      call  initAD            ; initialize analog to digitial 
      call  reset             ; move the panel to the reset position
      call  LEDBlink          ; blink the light 3 times to signify initialization
      ldi   r26,0
      ret
      
#
# MAKE THINGS GO
#


operatePanel:

      sbis  PINB, WESTSWITCH	 ;skip if bit set (skip i+99999999999999999f switch not pressed)
      call  westPressed
      lds   r18, west            ; west light sensor data into r18
      lds   r19, east            ; east light sensor data into r19
      lds   r20, west
      lds   r21, east
      ldi   r23, 25              ; "buffer" value into r23

      rjmp motorgo
      
 motorgo: 
      ldi   r23, 15
      sub   r21, r20
      sub   r20, r19
      cp    r18, r19              ; if west is less than east
      brsh  Skip2
      cp    r21, r23
      brlo  done
      call motorWest
      Skip2:
    

#
# moves it east 
#
      cp   r19, r18	         ; east is less than west (due to over correction)
      brsh Skip3
      cp  r20, r23
      brlo done
      call motorEast
      Skip3:       
      
done:
      call   motorStop           ; stops all motor movement for end of this loop
      ret			 ; goes back to .ino for loop

			
	  	

##############################################################
### MOTOR CODE
###

motorInitiate:
	;Set ports B and D to be output
	ldi  r20, 0xff
	out  DDIRB, r20
	out  DDIRD, r20
	cbi  DDIRB, WESTSWITCH	;make pin 2 an input pin
	sbi  PORTB, WESTSWITCH	;turn on pull up resistor for pin 2
	call delay1		;give pull up time to settle
	cbi  DDIRB, EASTSWITCH	;make pin 3 an input pin
	sbi  PORTB, EASTSWITCH	;turn on pull up resistor for pin 3
	call delay1		;give pull up time to settle
	ret

westPressed:
      ldi r26, 1
      sts westDigit, r26
      ret

reset:
  	  call  motorEast	; moves the motor east
  resetLoop:
	  sbis	PINB,EASTSWITCH ;skip if bit set (skip if switch not pressed)
	  rjmp  EastPressed
	  rjmp	resetLoop
  EastPressed:
	  call motorWest	; moves the motor west
	  call motorStop	; stops the motor 
     	  ret		        ; goes back to ino for loop


motorWest:                      ; method for moving the motors west
	ldi   r24, 0b11011000
	call  sendMotorByte
	ret

motorEast:                      ; method for moving the motors east
	ldi  r24, 0b00100111
	call sendMotorByte
	ret

motorStop:                      ; method for stopping the motors
	ldi  r24,0x00
	call sendMotorByte
	ret

#
# Delays for 1, ledblink, and 255 milliseconds
#

delay1:
      ldi   r22, 0x01
      ldi   r23, 0x00
      ldi   r24, 0
      ldi   r25, 0
      call  delay
      ret

delay2:                     ; led blink delay
      ldi   r22, 0xa0
      ldi   r23, 0x00
      ldi   r24, 0
      ldi   r25, 0
      call  delay
      ret

delayLong:     
      ldi   r22, 0xe8
      ldi   r23, 0x13
      ldi   r24, 0
      ldi   r25, 0
      call  delay     
      ret

#  1 bit transmission
sendOneBit:
      sbi PORTB,MOTDATA     ; set clock high
      call delay1
      sbi PORTD,MOTCLOCK    ; set data bit high
      call delay1           ; wait for it
      cbi PORTD,MOTCLOCK    ; Set clock low
      call delay1      
      ret

# 0 bit transmission
sendZeroBit:
      cbi PORTB,MOTDATA     ; set clock high
      call delay1
      sbi PORTD,MOTCLOCK    ;set data bit low
      call delay1
      cbi PORTD, MOTCLOCK    
      call delay1
      ret

#
# latch now should be enabled (one) in order to release 
# the control pattern to the motor driver chips 
#
latchData:
      sbi   PORTB,MOTLATCH
      call  delay1
      ; make sure PWM outputs are on
      sbi   PORTB, M1ENABLE
      sbi   PORTD, M2ENABLE
      sbi   PORTD, M3ENABLE
      sbi   PORTD, M4ENABLE
      ret

# latch should be zero in order to send the control 
# pattern to shift register    
latchReset: 
      cbi   PORTB,MOTLATCH
      call  delay1
      ret

sendMotorByte:
      push  r15
      push  r16
      mov   r15, r24
      call  latchReset
      ldi   r16, 8
smbloop:
      lsl   r15
      brcs  smbone
      call  sendZeroBit   
      rjmp  smbdone
smbone:
      call  sendOneBit
smbdone:
      dec   r16
      brne  smbloop
      call  latchData
      call  latchReset
      pop   r16
      pop   r15
      ret
###
##############################################################

##############################################################
### LIGHT CODE

initAD:

    ldi  r20, 0x3f     ; put the number for bits on and off for DIDR0
    sts  0x007e, r20   ; load into DIDR0 the bits on and off thing
    ldi  r20, 0x87     ; number for bits on and off for ADCSRA
    sts  0x007a, r20   ; put into ADSCRA the bits on and off to initialize conversion
    ret
    
readEast:
    ldi  r20, 0x60     ; loads 0x60 into r20. This is the address for analog 0
    add  r20, r24      ; adds the parameter value to 0x60 to get the desired analog port

    sts  0x007c, r20   ; put that into ADSCRA to set things
    ldi  r20, 0xc7
    sts  0x007a, r20   ; starts conversion
EastLoop:
    lds   r22, 0x007a  ;loads ADCSRA into r22
    sbrc  r22, 6       ;compares the 6th bit to see if it is clear. Skips the next line if it is cleared
    rjmp  EastLoop     ;loops if the bit is not cleared.  
    lds   r24, 0x0079  ; put result high byte into r24
    clr   r25          ; clears the low byte
    ret

readWest:
    ldi  r20, 0x60     ; loads 0x60 into r20. This is the address for analog 0
    add  r20, r24      ; adds the parameter value to 0x60 to get the desired analog port

    sts  0x007c, r20   ; put that into ADSCRA to set things
    ldi  r20, 0xc7
    sts  0x007a, r20   ; starts conversion
WestLoop:
    lds   r22, 0x007a  ;loads ADCSRA into r22
    sbrc  r22, 6       ;compares the 6th bit to see if it is clear. Skips the next line if it is cleared
    rjmp  WestLoop     ;loops if the bit is not cleared.  
    lds   r24, 0x0079  ; put result high byte into r24
    clr   r25          ; clears the low byte
    ret

    
#
# Blinks the onboard led 3 times
#
LEDBlink:    
    ldi r18, 3
  ledloop:
    sbi  PORTB, BOARDLED
    push r18
    call delay2 
    cbi  PORTB, BOARDLED
    call delay2
    pop r18
    dec r18
    brne ledloop
    ret
    
    
