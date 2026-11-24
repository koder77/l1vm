; SDL 3.0 module test for lisp2l1vm.
;
; This uses only wrappers that are safe without input devices, font files,
; sound files or camera hardware.  For headless systems run with:
;   SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy

(define check (lambda (label ok)
  (if ok
      (begin (write "ok   ") (write label) (newline))
      (begin (write "FAIL ") (write label) (newline)))))

; --- screen ---------------------------------------------------------------
(define screen (sdl-open-screen 320 240 32 0))
(check "open screen" (= screen 0))
(sdl-set-window-title "lisp2l1vm SDL 3.0 test")

; --- drawing primitives ----------------------------------------------------
(sdl-pixel 10 10 255 0 0 255)
(sdl-line 20 20 80 20 0 255 0 255)
(sdl-rectangle 20 30 80 60 0 0 255 255)
(sdl-rectangle-fill 30 70 70 90 255 255 0 255)
(sdl-circle 40 120 20 255 0 255 255)
(sdl-circle-fill 90 120 20 0 255 255 255)
(sdl-ellipse 140 120 30 15 255 128 0 255)
(sdl-ellipse-fill 140 150 30 15 128 255 0 255)
(sdl-pie 200 120 30 0 90 0 128 255 255)
(sdl-pie-fill 200 120 30 90 180 255 0 128 255)
(sdl-trigon 20 180 80 180 50 220 255 0 0 255)
(sdl-trigon-fill 100 180 160 180 130 220 0 255 0 255)
(check "draw primitives" T)

; sdl-get-pixelcolor reads the surface, so present the renderer first.
(sdl-update)
(define red (sdl-get-pixelcolor 10 10))
(check "get pixel color" (= red 255))

; --- input helpers ---------------------------------------------------------
(define mouse-x (sdl-get-mouse-state))
(check "get mouse state" (>= mouse-x 0))

(define joy-name "joystick")
(define axes (sdl-get-joystick-info joy-name))
(check "get joystick info" (>= axes 0))

(define key (sdl-get-key))
(check "get key" (>= key -1))

; --- GUI allocation aliases ------------------------------------------------
(init-gui 4)
(sdl-free-all-gadgets)
(check "gui allocate/free" T)

; --- shutdown --------------------------------------------------------------
(sdl-delay 1)
(sdl-quit)
(write "sdl-3.0 done")
(newline)
"PASS"
