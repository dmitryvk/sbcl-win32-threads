;;;; a bunch of handy macros for the PPC

;;;; This software is part of the SBCL system. See the README file for
;;;; more information.
;;;;
;;;; This software is derived from the CMU CL system, which was
;;;; written at Carnegie Mellon University and released into the
;;;; public domain. The software is in the public domain and is
;;;; provided with absolutely no warranty. See the COPYING and CREDITS
;;;; files for more information.

(in-package "SB!VM")

;;; Instruction-like macros.

(defmacro move (dst src)
  "Move SRC into DST unless they are location=."
  (once-only ((n-dst dst)
	      (n-src src))
    `(unless (location= ,n-dst ,n-src)
       (inst mr ,n-dst ,n-src))))

(macrolet
    ((frob (op inst shift)
       `(defmacro ,op (object base &optional (offset 0) (lowtag 0))
	  `(inst ,',inst ,object ,base (- (ash ,offset ,,shift) ,lowtag)))))
  (frob loadw lwz word-shift)
  (frob storew stw word-shift))

(defmacro load-symbol (reg symbol)
  `(inst addi ,reg null-tn (static-symbol-offset ,symbol)))

(macrolet
    ((frob (slot)
       (let ((loader (intern (concatenate 'simple-string
					  "LOAD-SYMBOL-"
					  (string slot))))
	     (storer (intern (concatenate 'simple-string
					  "STORE-SYMBOL-"
					  (string slot))))
	     (offset (intern (concatenate 'simple-string
					  "SYMBOL-"
					  (string slot)
					  "-SLOT")
			     (find-package "SB!VM"))))
	 `(progn
	    (defmacro ,loader (reg symbol)
	      `(inst lwz ,reg null-tn
		     (+ (static-symbol-offset ',symbol)
			(ash ,',offset word-shift)
			(- other-pointer-lowtag))))
	    (defmacro ,storer (reg symbol)
	      `(inst stw ,reg null-tn
		     (+ (static-symbol-offset ',symbol)
			(ash ,',offset word-shift)
			(- other-pointer-lowtag))))))))
  (frob value)
  (frob function))

(defmacro load-type (target source &optional (offset 0))
  "Loads the type bits of a pointer into target independent of
  byte-ordering issues."
  (once-only ((n-target target)
	      (n-source source)
	      (n-offset offset))
    (ecase *backend-byte-order*
      (:little-endian
       `(inst lbz ,n-target ,n-source ,n-offset))
      (:big-endian
       `(inst lbz ,n-target ,n-source (+ ,n-offset 3))))))

;;; Macros to handle the fact that we cannot use the machine native call and
;;; return instructions. 

(defmacro lisp-jump (function lip)
  "Jump to the lisp function FUNCTION.  LIP is an interior-reg temporary."
  `(progn
    ;; something is deeply bogus.  look at this
    ;; (loadw ,lip ,function sb!vm:function-code-offset sb!vm:function-pointer-type)
    (inst addi ,lip ,function (- (* n-word-bytes sb!vm:simple-fun-code-offset) sb!vm:fun-pointer-lowtag))
    (inst mtctr ,lip)
    (move code-tn ,function)
    (inst bctr)))

(defmacro lisp-return (return-pc lip &key (offset 0) (frob-code t))
  "Return to RETURN-PC."
  `(progn
     (inst addi ,lip ,return-pc (- (* (1+ ,offset) n-word-bytes) other-pointer-lowtag))
     (inst mtlr ,lip)
     ,@(if frob-code
         `((move code-tn ,return-pc)))
     (inst blr)))

(defmacro emit-return-pc (label)
  "Emit a return-pc header word.  LABEL is the label to use for this return-pc."
  `(progn
     (align n-lowtag-bits)
     (emit-label ,label)
     (inst lra-header-word)))



;;;; Stack TN's

;;; Move a stack TN to a register and vice-versa.
(defmacro load-stack-tn (reg stack)
  `(let ((reg ,reg)
	 (stack ,stack))
     (let ((offset (tn-offset stack)))
       (sc-case stack
	 ((control-stack)
	  (loadw reg cfp-tn offset))))))
(defmacro store-stack-tn (stack reg)
  `(let ((stack ,stack)
	 (reg ,reg))
     (let ((offset (tn-offset stack)))
       (sc-case stack
	 ((control-stack)
	  (storew reg cfp-tn offset))))))

(defmacro maybe-load-stack-tn (reg reg-or-stack)
  "Move the TN Reg-Or-Stack into Reg if it isn't already there."
  (once-only ((n-reg reg)
	      (n-stack reg-or-stack))
    `(sc-case ,n-reg
       ((any-reg descriptor-reg)
	(sc-case ,n-stack
	  ((any-reg descriptor-reg)
	   (move ,n-reg ,n-stack))
	  ((control-stack)
	   (loadw ,n-reg cfp-tn (tn-offset ,n-stack))))))))


;;;; Storage allocation:

;; Allocation macro
;;
;; This macro does the appropriate stuff to allocate space.
;;
;; The allocated space is stored in RESULT-TN with the lowtag LOWTAG
;; applied.  The amount of space to be allocated is SIZE bytes (which
;; must be a multiple of the lisp object size).
;;
;; If STACK-P is given, then allocation occurs on the control stack
;; (for dynamic-extent).  In this case, you MUST also specify NODE, so
;; that the appropriate compiler policy can be used, and TEMP-TN,
;; which is needed for work-space.  TEMP-TN MUST be a non-descriptor
;; reg.
;;
;; If generational GC is enabled, you MUST supply a value for TEMP-TN
;; because a temp register is needed to do inline allocation.
;; TEMP-TN, in this case, can be any register, since it holds a
;; double-word aligned address (essentially a fixnum).
(defmacro allocation (result-tn size lowtag &key stack-p node temp-tn)
  ;; We assume we're in a pseudo-atomic so the pseudo-atomic bit is
  ;; set.  If the lowtag also has a 1 bit in the same position, we're all
  ;; set.  Otherwise, we need to zap out the lowtag from alloc-tn, and
  ;; then or in the lowtag.
  ;; Normal allocation to the heap.
  `(let ((size ,size))
     (if (logbitp (1- n-lowtag-bits) ,lowtag)
	      (progn
		(inst ori ,result-tn alloc-tn ,lowtag)
		(if (numberp size)
		    (inst addi alloc-tn alloc-tn size)
		  (inst add alloc-tn alloc-tn size)))
	      (progn
		(inst clrrwi ,result-tn alloc-tn n-lowtag-bits)
		(inst ori ,result-tn ,result-tn ,lowtag)
		(if (numberp size)
		    (inst addi alloc-tn alloc-tn size)
		  (inst add alloc-tn alloc-tn size))))))

(defmacro with-fixed-allocation ((result-tn flag-tn temp-tn type-code size)
				 &body body)
  "Do stuff to allocate an other-pointer object of fixed Size with a single
  word header having the specified Type-Code.  The result is placed in
  Result-TN, and Temp-TN is a non-descriptor temp (which may be randomly used
  by the body.)  The body is placed inside the PSEUDO-ATOMIC, and presumably
  initializes the object."
  (once-only ((result-tn result-tn) (temp-tn temp-tn) (flag-tn flag-tn)
	      (type-code type-code) (size size))
    `(pseudo-atomic (,flag-tn)
       (allocation ,result-tn (pad-data-block ,size) other-pointer-lowtag)
       (when ,type-code
	 (inst lr ,temp-tn (logior (ash (1- ,size) n-widetag-bits) ,type-code))
	 (storew ,temp-tn ,result-tn 0 other-pointer-lowtag))
       ,@body)))


;;;; Error Code
(eval-when (:compile-toplevel :load-toplevel :execute)
  (defun emit-error-break (vop kind code values)
    (let ((vector (gensym)))
      `((let ((vop ,vop))
	  (when vop
	    (note-this-location vop :internal-error)))
	(inst unimp ,kind)
	(with-adjustable-vector (,vector)
	  (write-var-integer (error-number-or-lose ',code) ,vector)
	  ,@(mapcar #'(lambda (tn)
			`(let ((tn ,tn))
			   (write-var-integer (make-sc-offset (sc-number
							       (tn-sc tn))
							      (tn-offset tn))
					      ,vector)))
		    values)
	  (inst byte (length ,vector))
	  (dotimes (i (length ,vector))
	    (inst byte (aref ,vector i))))
	(align word-shift)))))

(defmacro error-call (vop error-code &rest values)
  "Cause an error.  ERROR-CODE is the error to cause."
  (cons 'progn
	(emit-error-break vop error-trap error-code values)))


(defmacro cerror-call (vop label error-code &rest values)
  "Cause a continuable error.  If the error is continued, execution resumes at
  LABEL."
  `(progn
     ,@(emit-error-break vop cerror-trap error-code values)
     (inst b ,label)))

(defmacro generate-error-code (vop error-code &rest values)
  "Generate-Error-Code Error-code Value*
  Emit code for an error with the specified Error-Code and context Values."
  `(assemble (*elsewhere*)
     (let ((start-lab (gen-label)))
       (emit-label start-lab)
       (error-call ,vop ,error-code ,@values)
       start-lab)))

(defmacro generate-cerror-code (vop error-code &rest values)
  "Generate-CError-Code Error-code Value*
  Emit code for a continuable error with the specified Error-Code and
  context Values.  If the error is continued, execution resumes after
  the GENERATE-CERROR-CODE form."
  (with-unique-names (continue error)
    `(let ((,continue (gen-label)))
       (emit-label ,continue)
       (assemble (*elsewhere*)
	 (let ((,error (gen-label)))
	   (emit-label ,error)
	   (cerror-call ,vop ,continue ,error-code ,@values)
	   ,error)))))

;;;; PSEUDO-ATOMIC

;;; handy macro for making sequences look atomic
;;;
;;; FLAG-TN must be wired to NL3. If a deferred interrupt happens
;;; while we have the low bits of ALLOC-TN set, we add a "large"
;;; constant to FLAG-TN. On exit, we add FLAG-TN to ALLOC-TN which (a)
;;; aligns ALLOC-TN again and (b) makes ALLOC-TN go negative. We then
;;; trap if ALLOC-TN's negative (handling the deferred interrupt) and
;;; using FLAG-TN - minus the large constant - to correct ALLOC-TN.
(defmacro pseudo-atomic ((flag-tn &key (extra 0)) &rest forms)
  (let ((n-extra (gensym)))
    `(let ((,n-extra ,extra))
       (without-scheduling ()
	;; Extra debugging stuff:
	#+debug
	(progn
	  (inst andi. ,flag-tn alloc-tn 7)
	  (inst twi :ne ,flag-tn 0))
	(inst lr ,flag-tn (- ,n-extra 4))
	(inst addi alloc-tn alloc-tn 4))
      ,@forms
      (without-scheduling ()
       (inst add alloc-tn alloc-tn ,flag-tn)
       (inst twi :lt alloc-tn 0))
      #+debug
      (progn
	(inst andi. ,flag-tn alloc-tn 7)
	(inst twi :ne ,flag-tn 0)))))




(defmacro sb!sys::with-pinned-objects ((&rest objects) &body body)
  "Arrange with the garbage collector that the pages occupied by
OBJECTS will not be moved in memory for the duration of BODY.
Useful for e.g. foreign calls where another thread may trigger
garbage collection.  This is currently implemented by disabling GC"
  (declare (ignore objects))		;should we eval these for side-effect?
  `(without-gcing
    ,@body))


