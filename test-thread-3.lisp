(defun cons-lot (stream char)
  (sleep 2)
  (loop
    (loop repeat 1
      for ar = (make-array (* 1024 1024 1) :initial-element 0)
      do (setf (aref ar 1000) 112))
    (format stream "~A" char)
    (finish-output stream)))

(defun threaded-cons-lot (n)
  (loop
    with stream = *standard-output*
    repeat n
    for i from 0
    for c = (code-char (+ i (char-code #\A)))
    do (sb-thread:make-thread (lambda () (cons-lot stream c)))))

(threaded-cons-lot 2)
;(quit)
