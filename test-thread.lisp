(defun print-enter () (format t "Enter~%"))
(defun print-leave () (format t "Leave~%"))

(sb-thread:make-thread
  (lambda ()
    (print-enter)
    (sleep 3)
    (print-leave)))

(sleep 1)
(gc)