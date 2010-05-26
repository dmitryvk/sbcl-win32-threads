(defvar *t*)

(setf *t*
  (sb-thread:make-thread
    (lambda ()
      (describe sb-thread:*current-thread*))))
    
(sb-thread:join-thread *t*)

(quit)
