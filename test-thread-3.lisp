(sb-alien:define-alien-routine ("odprint" odprint) sb-alien:void (msg sb-alien:c-string))

(defun cons-lot (stream char)
  (odprint (format nil "in cons-lot, char is ~A" char))
  (sleep 2)
  (loop
    (loop repeat 1
      for ar = (make-array (* 1024 1024 1) :initial-element 0)
      do (loop for i from 0 below (length ar) do (setf (aref ar i) 123)))
    (format stream "~A" char)
    (finish-output stream)
    (sleep 0.1)
    ))
    
(defun non-consing-loop (stream char)
  (loop
    (loop repeat (expt 10 8))
    (format stream "~A" char)
    (finish-output stream)
    (sleep 0.1)))

(defun threaded-cons-lot (n)
  (loop
    with stream = *standard-output*
    repeat n
    for i from 0
    for c = (code-char (+ i (char-code #\A)))
    do (odprint "creating thread")
    do (let ((c c)) (sb-thread:make-thread (lambda () (cons-lot stream c))))
    do (odprint "created thread")))

#+nil
(with-open-file (f "diasm.txt" :direction :output :if-exists :supersede)
  (let ((*standard-output* f))
    (disassemble 'cons-lot)))

(defconstant +n+ 3)
(sleep 1)
    
(let ((output *standard-output*))
  (sb-thread:make-thread (lambda () (non-consing-loop output #\+))))
(threaded-cons-lot (1- +n+))
(odprint "threads started, now cons myself")
(cons-lot *standard-output* (code-char (+ (1- +n+) (char-code #\A))))
;(quit)
