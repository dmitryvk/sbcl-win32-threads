(defvar *t* (sb-thread:make-thread (lambda () (loop do (sleep 1)))))

(loop
 do (sleep 0.5)
 do (sb-thread:interrupt-thread *t* (lambda ()))
 do (gc :full t))
