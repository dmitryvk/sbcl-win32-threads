(use-package :sb-alien)

(load-shared-object "some-lib.dll")

(define-alien-routine myfun int (c int))

(defun try-gc ()
  (let ((thread (sb-thread:make-thread (lambda () (myfun 123)))))
    (format t "doing gc~%")
	(finish-output)
	(gc :full t)
	(format t "gc done~%")
	(finish-output)
	(sb-thread:join-thread thread)))
	
(try-gc)

(quit)
  