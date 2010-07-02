(defvar *t* (sb-thread:make-thread (lambda () (loop))))

(loop
 for i from 1
 do (sleep 0.33)
 do (sb-thread:interrupt-thread *t* (let ((j i)) (lambda () (format t "Interrupted ~A~%" j))))
 do (gc :full t)
 )
