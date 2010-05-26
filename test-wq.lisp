(use-package :sb-thread)

     (defvar *buffer-queue* (make-waitqueue))
     (defvar *buffer-lock* (make-mutex :name "buffer lock"))
     
     (defvar *buffer* (list nil))
     
     (defun reader ()
       (with-mutex (*buffer-lock*)
         (loop
          (condition-wait *buffer-queue* *buffer-lock*)
          (loop
           (unless *buffer* (return))
           (let ((head (car *buffer*)))
             (setf *buffer* (cdr *buffer*))
             (format t "reader ~A woke, read ~A~%"
                     *current-thread* head))))))
     
     (defun writer ()
       (loop
        (sleep (random 1.0))
        (with-mutex (*buffer-lock*)
          (let ((el (intern
                     (string (code-char
                              (+ (char-code #\A) (random 26)))))))
            (setf *buffer* (cons el *buffer*)))
          (condition-notify *buffer-queue*))))
     
     (make-thread #'writer)
     (make-thread #'reader)
     (make-thread #'reader)
