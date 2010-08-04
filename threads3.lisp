(use-package :sb-thread)

(declaim (optimize (debug 3)) (notinline mm ml ct mct noisefn))

(defun mm ()
  (cons t t))

(defun ml (mom-mark)
  (list (make-array 24 :initial-element mom-mark)))
  
(defun ct (semaphore parent kid-mark)
  (wait-on-semaphore semaphore)
  (let ((old (symbol-value-in-thread 'this-is-new parent)))
    (setf (symbol-value-in-thread 'this-is-new parent)
          (make-array 24 :initial-element kid-mark))
    old))
    
(defun mct (semaphore parent kid-mark)
  (lambda () (ct semaphore parent kid-mark)))
  
(defvar *running* nil)  

(defun noisefn ()
(loop while t
      do (setf * (make-array 1024))
      ;; Busy-wait a bit so we don't TOTALLY flood the
      ;; system with GCs: a GC occurring in the middle of
      ;; S-V-I-T causes it to start over -- we want that
      ;; to occur occasionally, but not _all_ the time.
        (loop repeat (random 128)
              do (setf ** *))))

;(disassemble 'noisefn)
              
(defun run ()
(let* ((parent *current-thread*)
         (semaphore (make-semaphore))
         (*running* t)
         (noise nil))
    (setf noise (make-thread 'noisefn))
    (write-string "; ")
    (dotimes (i 600000)
      (when (zerop (mod i 20))
        (write-char #\.)
        (force-output))
      (let* ((mom-mark (mm))
             (kid-mark (mm))
             (child (make-thread (mct semaphore parent kid-mark))))
        (progv '(this-is-new) (ml mom-mark)
          (signal-semaphore semaphore)
          (assert (eq mom-mark (aref (join-thread child) 0)))
          (assert (eq kid-mark (aref (symbol-value 'this-is-new) 0))))))
    (setf *running* nil)
    (join-thread noise)))
    
(run)
    
(quit)
    
