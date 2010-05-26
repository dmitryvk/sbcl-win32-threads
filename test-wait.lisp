(use-package :sb-thread)

(defun test-it ()
  (let ((sem (make-semaphore)))
    (labels ((t1 () (wait-on-semaphore sem)))
	  (let ((th (make-thread #'t1)))
	    (format t "doing gc~%") (finish-output)
		(gc)
		(format t "done gc~%") (finish-output)
		(signal-semaphore sem)
		(join-thread th)))))
		
(test-it)
(quit)
