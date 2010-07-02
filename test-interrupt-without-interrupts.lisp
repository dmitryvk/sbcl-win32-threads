(defvar *t*
  (sb-thread:make-thread
    (lambda ()
      (loop
        do (sb-sys:without-interrupts
             (format t "<")
             (loop repeat 5 do (sleep 0.1) do (format t ".") do (finish-output))
             (format t ">~%"))
        do (cons nil nil))
  )))

(loop
 for i from 1
 do (sleep 0.33)
 do (sb-thread:interrupt-thread *t* (let ((j i)) (lambda () (format t "Interrupted ~A~%" j))))
 ;do (gc :full t)
 )
