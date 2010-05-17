(defun cons-lot (stream char)
  (sleep 2)
  (loop
    (loop repeat 1
      for ar = (make-array (* 1024 1024 1) :initial-element 0)
      do (loop for i from 0 below (length ar) do (setf (aref ar i) 123)))
    (format stream "~A" char)
    (finish-output stream)
    (sleep 0.1)
    ))

(defun threaded-cons-lot (n)
  (loop
    with stream = *standard-output*
    repeat n
    for i from 0
    for c = (code-char (+ i (char-code #\A)))
    do (sb-thread:make-thread (lambda () (cons-lot stream c)))))

#+nil
(with-open-file (f "diasm.txt" :direction :output :if-exists :supersede)
  (let ((*standard-output* f))
    (disassemble 'cons-lot)))

(defconstant +n+ 3)
(sleep 1)
    
(threaded-cons-lot (1- +n+))
(cons-lot *standard-output* (code-char (+ (1- +n+) (char-code #\A))))
;(quit)
