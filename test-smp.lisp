(defun compute-this (&optional (c #\.))
  (loop
    with z = 1
	do (loop
		repeat (expt 10 6)
		do (setf z (mod (+ z 1) 12351231)))
	(write-char c)
	(finish-output)
	))

(defun tf (c) (lambda () (compute-this c)))
	
	
(defun rl ()
  (loop
    for i from 0
	repeat 3
	for fn = (tf (code-char (+ (char-code #\A) i)))
    do (sb-thread:make-thread fn)))