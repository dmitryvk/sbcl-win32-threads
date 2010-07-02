(use-package :sb-thread)

(let* ((parent *current-thread*)
         (semaphore (make-semaphore))
         (running t)
         (noise (make-thread (lambda ()
                               (loop with x = nil with y = nil while running
                                     do (setf x (make-array 1024))
                                     ;; Busy-wait a bit so we don't TOTALLY flood the
                                     ;; system with GCs: a GC occurring in the middle of
                                     ;; S-V-I-T causes it to start over -- we want that
                                     ;; to occur occasionally, but not _all_ the time.
                                        (loop repeat (random 128)
                                              do (setf y x)))))))
    (declare (ignorable parent))
    (write-string "; ")
    (dotimes (i 600000)
      (when (zerop (mod i 200))
        (write-char #\.)
        (force-output))
      (let* ((mom-mark (cons t t))
             (kid-mark (cons t t))
             (child (make-thread (lambda ()
                                   (wait-on-semaphore semaphore)
                                   (let ((old
                                         nil
                                         ;(symbol-value-in-thread 'this-is-new parent)
                                         ))
                                     (setf old ;(symbol-value-in-thread 'this-is-new parent)
                                           (make-array 24 #+nil :initial-element #+nil kid-mark))
                                     old)))))
        (progv '(this-is-new) (list (make-array 24 #+nil :initial-element #+nil mom-mark))
          (signal-semaphore semaphore)
          ;;(assert (eq mom-mark (aref
            (join-thread child)
          ;;0)))
          ;(assert (eq kid-mark (aref (symbol-value 'this-is-new) 0)))
          )))
    (setf running nil)
    (when noise (join-thread noise)))
    
(quit)
    