
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
        (param-names $?pa-n)
        (param-values $?pa-v))
  (not (wm-fact (key diagnosis plan-action ?name args? plan ?plan-id $?)))
  (domain-predicate (name ?dn&:(eq ?dn (sym-cat last- ?name))) (param-names $?p))
  =>
  (bind $?args (create$))
  (loop-for-count (?i 1 (length ?pa-n))
    ; Only add parameters defined in last-  and next- predicates
    (if (neq FALSE (member$ (nth$ ?i ?pa-n) ?p)) then
     (bind $?args (create$ ?args (nth$ ?i ?pa-n) (nth$ ?i ?pa-v)))
    )
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
  (printout t "Removed " ?name ?plan-id " because plan " ?plan-id " does not exist anymore " crlf)
)

