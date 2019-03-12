
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
  (robmem-restore-collection ?*WM-ROBMEM-SYNC-COLLECTION* "@CONFDIR@/robot-memory/TEST-PLAN" "diagnosis.test")
)

(defrule diagnosis-wm-test
  (domain-facts-loaded)
  =>
  (assert (goal (id TEST-GOAL) (mode EXPANDED) (committed-to TEST-PLAN))
          (plan (id TEST-PLAN) (goal-id TEST-GOAL) (diag-wm-store TRUE))
  )
)
