===File /com/genera/tags/rel-8-1/sys.sct/lmtape/tapex.lisp.~2616~===
;-*- Lowercase: Yes; Mode:LISP; Package:TAPE; Base:8 -*-
;;;>
;;;> *****************************************************************************************
;;;> ** (c) Copyright 1991-1982 Symbolics, Inc.  All rights reserved.
;;;> ** Portions of font library Copyright (c) 1984 Bitstream, Inc.  All Rights Reserved.
;;;>
;;;>    The software, data, and information contained herein are proprietary 
;;;> to, and comprise valuable trade secrets of, Symbolics, Inc., which intends 
;;;> to keep such software, data, and information confidential and to preserve 
;;;> them as trade secrets.  They are given in confidence by Symbolics pursuant 
;;;> to a written license agreement, and may be used, copied, transmitted, and 
;;;> stored only in accordance with the terms of such license.
;;;> 
;;;> Symbolics, Symbolics 3600, Symbolics 3670 (R), Symbolics 3675 (R), Symbolics 3630,
;;;> Symbolics 3640, Symbolics 3645 (R), Symbolics 3650 (R), Symbolics 3653, Symbolics
;;;> 3620 (R), Symbolics 3610 (R), Symbolics Common Lisp (R), Symbolics-Lisp (R),
;;;> Zetalisp (R), Genera (R), Wheels (R), Dynamic Windows (R), Showcase, SmartStore (R),
;;;> Semanticue (R), Frame-Up (R), Firewall (R), MACSYMA (R), COMMON LISP MACSYMA (R),
;;;> CL-MACSYMA (R), LISP MACHINE MACSYMA (R), MACSYMA Newsletter (R), PC-MACSYMA, Document
;;;> Examiner (R), Delivery Document Examiner, S-DYNAMICS (R), S-GEOMETRY (R), S-PAINT (R),
;;;> S-RECORD, S-RENDER (R), Displacement Animation, FrameThrower, PaintAmation, "Your Next
;;;> Step in Computing" (R), Ivory, MacIvory, MacIvory model 1, MacIvory model 2, MacIvory
;;;> model 3, XL400, XL1200, Symbolics UX400S, Symbolics UX1200S, Symbolics C, Symbolics
;;;> Pascal (R), Symbolics Prolog, Symbolics Fortran (R), CLOE (R), CLOE Application Generator,
;;;> CLOE Developer, CLOE Runtime, Common Lisp Developer, Symbolics Concordia, Joshua, and
;;;> Statice (R) are trademarks of Symbolics, Inc.
;;;> 
;;;> RESTRICTED RIGHTS LEGEND
;;;>    Use, duplication, and disclosure by the Government are subject to restrictions 
;;;> as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data and Computer 
;;;> Software Clause at DFAR 52.227-7013.
;;;> 
;;;>      Symbolics, Inc.
;;;>      8 New England Executive Park, East
;;;>      Burlington, Massachusetts  01803
;;;>      United States of America
;;;>      617-221-1000
;;;> *****************************************************************************************
;;;>

;;; Tapex tapes are written at 1600 bpi.  Every record is 4096 bytes long.
;;; There are "logical files" separated by file marks.  A double file
;;; mark ends a tape.  Each logical file is a header record followed by
;;; as many records as it takes to express the bytes of the file.  The
;;; header record contains the file's "pathname", followed by a 200 octal
;;; character.  The rest of the header record is ignored.  The data of 
;;; each file starts at the beginning of the first record after the header
;;; record and continues until a 200 octal character, after which the
;;; remainder of the record containing it, and all records until the 
;;; next file mark are ignored.  

;;; The "pathname" is some description of the file.  There is no standard
;;; on how this is expressed.  The tapex program on the Lisp Machine
;;; queries for each file.  The file data is in (non-Lisp Machine)
;;; ASCII, which is to say tabs are octal 11, lines end in CR/LF (15/12)
;;; sequences.  There is no definition about how Lisp Machine extended
;;; characters are represented.  It cannot work for other than ASCII files.

(defconst *tapex-reclen* 4096.)
(defconst *tapex-chars* '(#O11 #O12 #O14 #O15 #O200 #O0))
(defvar *tapex-defaults* nil)

(defun tapex ()
  (selectq
    (fquery '(:choices (((:read "Read in a tapex tape") #/R)
			((:write "Write files to tape") #/W)
			((:list  "List the files on a tapex tape") #/L)))
	    "Read, Write, or List? ")
    (:read  (tapex-reader))
    (:list  (tapex-lister))
    (:write (tapex-writer))))


(defun tapex-reader ()
  (or *tapex-defaults* (setq *tapex-defaults* (fs:user-homedir)))
  (with-open-stream (stream (tape:make-stream ':record-length *tapex-reclen*))
    (do () (())
      (let ((path (make-array '100. ':type 'art-string ':leader-length 1)))
	(store-array-leader 0 path 0)
	
	(let ((c (send stream ':tyi)))
	  (if (null c) (return nil))
	  (array-push-extend path c))
	(loop for c = (send stream ':tyi)
	      until (= c 200)
	      do (array-push-extend path c))
	(send stream ':clear-input)
	(let ((pathname (prompt-and-read
			  `(:pathname-or-nil :default ,*tapex-defaults*)
			  "File ~A. New pathname? (” to SKIP this file)~%(default  ~A): "
			  path *tapex-defaults*)))
	  (when pathname
	    (with-open-file (outstr (setq *tapex-defaults* pathname) ':direction ':output)
	      (format t "~&Creating ~A." (send outstr ':truename))
	      (do () (())
		(multiple-value-bind (buf curx remaining)
		    (send stream ':get-input-buffer)
		  (if (null buf) (return nil))
		  (let ((interestx
			  ;; string-search-set would be fine, but our arg is not a string
			  (loop repeat remaining for i  upfrom curx
				for ch = (aref buf i)
				when (dolist (c *TAPEX-CHARS*) (if (= c ch) (return t)))
				return i)))
		    (cond (interestx		;found one
			   (unless (= curx interestx)
			     (send outstr ':string-out buf curx interestx))
			   (send stream ':advance-input-buffer interestx))
			  (t
			   (unless (zerop remaining)
			     (send outstr ':string-out buf curx (+ curx remaining)))
			   (send stream ':advance-input-buffer)))
		    (let ((c (send stream ':tyi)))
		      (if (or (null c) (= c 200) (= c 0))
			  (return))
		      (selectq c
			(#O15             ())	;throw away cr
			(#O12             (send outstr ':tyo #\CR))
			(#O14		  (send outstr ':tyo #\FORM))
			(#O11		  (send outstr ':tyo #\TAB))
			(t		  (send outstr ':tyo c)))))))))
	      (send stream ':skip-file))))))

(defconst *INVERSE-TAPEX-CHARS* '(#\TAB #\CR #\FORM))

;;; Remember that final eof is written by close op, not us.
(defun tapex-writer ()
  (or *tapex-defaults* (setq *tapex-defaults* (fs:user-homedir)))
  (with-open-stream (stream (tape:make-stream
			      ':direction ':output ':record-length *tapex-reclen*
			      ':pad-char #O200 ':minimum-record-length ':full))
    (do () (())
      (let ((pathname
	      (prompt-and-read `(:pathname-or-nil :default ,*tapex-defaults*)
			       "~&Filename? (” to end, default ~A) " *tapex-defaults*)))
	(unless pathname (return nil))
	(with-open-file (instr (setq *tapex-defaults* pathname))
	  (format t "~&Dumping ~A." (send instr ':truename))
	  (send stream ':string-out (send (send instr ':truename)
					  ':string-for-host))
	  (send stream ':tyo #O200)
	  (send stream ':force-output)		;end the record
	  (do () (())
	    (multiple-value-bind (buf curx remaining)
		(send instr ':get-input-buffer)
	      (if (null buf) (return nil))
	      (let ((interestx
		      (string-search-set *INVERSE-TAPEX-CHARS*
					 buf curx (+ curx remaining))))
		(cond ((null interestx)
		       (if (not (zerop remaining))
			   (send stream ':string-out buf curx (+ curx remaining)))
		       (send instr ':advance-input-buffer))
		      ;; found one
		      (t
		       (if (not (= curx interestx))
			   (send stream ':string-out buf curx interestx))
		       (send instr ':advance-input-buffer interestx)))
		
		(let ((c (send instr ':tyi)))
		  (if (null c) (return))
		  (selectq c
		    (#\CR             (send stream ':tyo #O15)
				      (send stream ':tyo #O12))
		    (#\TAB	      (send stream ':tyo #O11))
		    (#\FORM           (send stream ':tyo #O14))
		    (t		      (send stream ':tyo c)))))))
	  (send stream ':tyo #O200)
	  (send stream ':eof))))))


(defun tapex-lister ()
  (with-open-stream (stream (tape:make-stream ':record-length *tapex-reclen*))
    (do ((path (make-array '100. ':type 'art-string ':leader-list '(0)))) (nil)
      (store-array-leader 0 path 0)		;have to do each time
      (let ((c (funcall stream ':tyi)))
	(if (null c) (return nil))
	(array-push-extend path c))
      (loop for c = (send stream ':tyi)
	    until (= c 200)
	    do (array-push-extend path c))
      (funcall stream ':clear-input)
      (format t "~&~A" path)
      (funcall stream ':skip-file))))
