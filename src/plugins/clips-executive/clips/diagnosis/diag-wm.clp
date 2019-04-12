
;---------------------------------------------------------------------------
;  diag-wm.clp - CLIPS executive - diagnosis creation for given problem
;
;  Created: Sun Mar 09 15:47:23 2019
;  Copyright  2019 Daniel Habering
;  Licensed under GPLv2+ license, cf. LICENSE file
;---------------------------------------------------------------------------

; When a plan gets dispatched that we may want to diagnose, the world model
; has to be stored in order to enable the reevaluation of the plan in case of a failure



(deffunction diagnosis-backup-wm (?plan-id ?dir)
  (bind ?dump-path (str-cat ?dir "/" ?plan-id))
  (bind ?succ (robmem-dump-collection ?*WM-ROBMEM-SYNC-COLLECTION* ?dump-path))
  (if (eq ?succ FALSE) then (printout error "Failed to backup world model")
                       else (printout t "Stored backup of " ?*WM-ROBMEM-SYNC-COLLECTION* " to " ?dump-path crlf))
  (printout t ?succ crlf)
)

(defrule diagnosis-trigger-backup-wm
  (declare (salience ?*SALIENCE-HIGH*))
  (goal (id ?goal-id) (mode EXPANDED) (committed-to ?plan-id))
  ?p <- (plan (id ?plan-id) (goal-id ?goal-id) (diag-wm-store TRUE))
  =>
  (modify ?p (diag-wm-store STORED))
  (printout t "Storing world model for plan " ?plan-id crlf)
  (diagnosis-backup-wm ?plan-id "@CONFDIR@/robot-memory")
)

(defrule diagnosis-update-history
  (declare (salience ?*SALIENCE-HIGH*))
  (wm-fact (key domain fact self args? r ?r))
  (plan (id ?plan-id) (goal-id ?goal-id) (diag-wm-store TRUE))
  (plan-action (id ?id) (action-name ?name)
        (plan-id ?plan-id)
        (goal-id ?goal-id)
        (state FINAL)
        (param-names $?p)
        (param-values $?pv))
  (not (wm-fact (key diagnosis plan-action ?name args?plan ?plan-id $?)))
  =>
  (bind $?args (create$))
  (loop-for-count (?i 1 (length ?p))
    (bind $?args (create$ ?args (nth$ ?i ?p) (nth$ ?i ?pv)))
  )
  (bind $?args (create$ plan ?plan-id id (sym-cat ?id) ?args))
  (assert (wm-fact (key diagnosis plan-action ?name args? ?args) ))
)

(defrule diagnosis-cleanup-history
  (declare (salience ?*SALIENCE-HIGH*))
  ?wm <- (wm-fact (key diagnosis plan-action ?name args? plan ?plan-id $?))
  (not (plan (id ?plan-id)))
  =>
  (retract ?wm)
  (printout error "Removed " ?r ?name ?plan-id crlf)
)

