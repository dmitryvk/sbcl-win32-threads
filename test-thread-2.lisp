(defun print-enter () (format t "Enter~%"))
(defun print-leave () (format t "Leave~%"))

(sb-thread:make-thread
  (lambda ()
    (format t "gcing~%")
    (sleep 1)
    (gc)))
