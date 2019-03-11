
;---------------------------------------------------------------------------
;  diagnosis.clp - CLIPS executive - diagnosis creation for given problem
;
;  Created: Sun Mar 09 15:47:23 2019
;  Copyright  2019 Daniel Habering
;  Licensed under GPLv2+ license, cf. LICENSE file
;---------------------------------------------------------------------------

; Creates diagnosis candidates for a given fault
; Makes use of the pddl-planner plugin for generating multiple plans with kstar,
; which are then asserted to the fact base to be later evaluated by active diagnosis goals
;
; Enhancements: - Replace plan-actions with some diagnosis specific action template

(deftemplate diagnosis
  (slot id (type SYMBOL))
  (slot fault (type STRING))
  (slot mode (type SYMBOL) (allowed-values CREATED DIAGNOSIS-CREATED FINAL))
)

(deftemplate diagnosis-hypothesis
  (slot id (type SYMBOL))
  (slot diag-id (type SYMBOL))
  (slot status (type SYMBOL) (allowed-values POSSIBLE VALID REJECTED))
  (slot prob (type FLOAT))
)

(deffunction create-diagnosis (?diag-id ?fault)
  (assert (diagnosis (id ?diag-id)
                (fault ?fault))
  )
  (pddl-diagnosis-call ?diag-id ?fault)
)

(defrule diagnosis-test
  (domain-facts-loaded)
  (not (diagnosis))
  =>
  (create-diagnosis TEST-DIAG "(and (next FINISH) (not (wp-at WP M2 INPUT)))")
)

(defrule diagnosis-create-diag-from-robmem
  "Fetch the resulting plan from robot memory and expand the goal."
  ?g <- (diagnosis (id ?diag-id) (mode CREATED))
  ?t <- (robmem-trigger (name "new-plan") (ptr ?obj))
  ?p <- (pddl-plan
          (status PLANNED)
          (purpose-id ?diag-id)
        )
  =>
  (printout t "Fetched a new diagnosis!" crlf)
  (bind ?plan-id (bson-get (bson-get ?obj "o") "plan"))
  (progn$ (?action (bson-get-array (bson-get ?obj "o") "actions"))
    (bind ?action-name (sym-cat (bson-get ?action "name")))
    ; FF sometimes returns the pseudo-action REACH-GOAL. Filter it out.
    (if (neq ?action-name REACH-GOAL) then
      (bind ?param-values (bson-get-array ?action "args"))
      ; Convert all parameters to upper-case symbols
      (progn$ (?param ?param-values)
        (bind ?param-values
              (replace$
                ?param-values
                ?param-index ?param-index
                (sym-cat (upcase ?param))
              )
        )
      )
      (assert
        (diagnosis-hypothesis (id ?plan-id) (diag-id ?diag-id))
        (plan-action
          (id ?action-index)
          (goal-id ?diag-id)
          (plan-id ?plan-id)
          (action-name ?action-name)
          (param-values ?param-values)
        )
      )
    )
  )
)

(defrule diagnosis-finish-hypothesis-generation
  ?d <- (diagnosis (id ?diag-id) (mode CREATED))
  (diagnosis-hypothesis (diag-id ?diag-id))
  (not (robmem-trigger (name "new-plan")))
  ?p <- (pddl-plan (status PLANNED) (purpose-id ?diag-id))
  =>
  (modify ?d (mode DIAGNOSIS-CREATED))
  (retract ?p)
)


