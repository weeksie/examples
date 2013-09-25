;; this is just a snippet. It's not a mode or anything fancy.

(defun kill-current-line ()
  "An abbreviated method for killing a whole line plus the newline terminal"
  (kill-region (line-beginning-position) (+ (line-end-position) 1)))

(defun copy-current-line ()
  "Similar to the above but copy the text rather than cut it"
  (copy-region-as-kill (line-beginning-position) (+ (line-end-position) 1)))

(defun kill-yank (n)
  (kill-current-line) ;grab current line of text
  (forward-line n)
  (beginning-of-line)
  (yank)
  (forward-line -1)   ;move back to the beginning of the yanked text
  (beginning-of-line))

(defun kill-yank-region (start end n)
  (let ((lines (count-lines start end)))
    (goto-char start)
    (beginning-of-line)
    (kill-line lines))
  (progn
    (forward-line n)
    (yank)
    (if (> 0 n)
	(exchange-point-and-mark))
    (setq deactivate-mark nil)))

(defun copy-yank (n)
  (copy-current-line)
  (forward-line n)
  (beginning-of-line)
  (yank)
  (forward-line -1)
  (beginning-of-line))

(defun kill-yank-up ()
  (interactive)
  (if mark-active
      (kill-yank-region (region-beginning) (region-end) -1)
    (kill-yank -1)))

(defun kill-yank-down ()
  (interactive)
  (if mark-active
      (kill-yank-region (region-beginning) (region-end) 1)
    (kill-yank 1)))


(defun copy-yank-up ()
  (interactive)
  (copy-yank 0))

(defun copy-yank-down ()
  (interactive)
  (copy-yank 1))
