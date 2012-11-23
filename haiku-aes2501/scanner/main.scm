(require-extension bind coops cplusplus-object)
(bind* "#include <Application.h>\n#include <kernel/OS.h>\n"
 "#include \"app.h\"\n#include \"common/aes2501_app_common.h\"")

#|(define spawn-thread (foreign-lambda integer32 "spawn_thread"
 (function integer32 (c-pointer)) (const c-string) integer32 c-pointer))|#
(define resume-thread (foreign-lambda integer32 "resume_thread" integer32))
(bind-opaque-type b_message_ptr (c-pointer "BMessage"))
(define-external (SMessageReceived (scheme-object obj) (b_message_ptr mes))
 void (MessageReceived obj mes))


(define (make-kernel-thread obj)
 (SpawnThreadProxy "AEScan Runner" 50 obj)
)
(define (b-good? ch) (if (>= ch 0) #t #f))
(define (kernel-thread-start! th)
 (if (b-good? th)
  (resume-thread th)
 )
)
; (define instance (force init-val))
(define init-val (delay
 (let ((obj (new <AEScan> kAesSignature)))
  (kernel-thread-start! (make-kernel-thread obj))
  obj
 )
))

(define-method (MessageReceived (x <AEScan>) message)
 (let ((ltr (integer->char (message-what))))
  ; datums must be distinct :(
  (case ltr
  ((#\b) (display "yup\n"))
  (else (display "nope\n"))
 )
))

;;;

(define (may-continue?)
 (print "This program will enroll your right index finger, "
  "unconditionally overwriting any right-index print that was enrolled "
  "previously. If you want to continue, hit enter, otherwise press "
  "something else")
 (eq? (read-char) #\newline)
)

;;;
