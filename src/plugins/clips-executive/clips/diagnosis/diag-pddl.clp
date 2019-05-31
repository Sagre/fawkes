
;---------------------------------------------------------------------------
;  diag-pddl.clp - CLIPS executive - diagnosis creation for given problem
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
  (slot plan-id (type SYMBOL))
  (slot fault (type STRING))
  (slot mode (type SYMBOL) (allowed-values CREATED GENERATED DIAGNOSIS-CREATED ACTIVE-DIAGNOSIS FINAL FAILED))
)

(deftemplate diagnosis-gen
  (slot id (type SYMBOL))
  (slot gen-id (type SYMBOL))
  (slot plan-id (type SYMBOL))
)

(deftemplate diagnosis-hypothesis
  (slot id (type SYMBOL))
  (slot diag-id (type SYMBOL))
  (slot cost (type FLOAT))
  (slot status (type SYMBOL) (allowed-values POSSIBLE VALID REJECTED))
  (slot prob (type FLOAT))
)


(deffunction pddl-diagnosis-call (?diag-id ?fault)
   "Call the PDDL planner for the given diagnose-id with the goal given as string."
  (bind ?m
     (blackboard-create-msg "PddlPlannerInterface::pddl-planner" "PlanMessage")
  )
  (printout info "Calling PDDL planner")
  (bind ?planner-id (blackboard-send-msg ?m))
  (assert (pddl-plan
    (purpose-id ?diag-id) (goal ?fault) (status PENDING) (planner-id ?planner-id)
  ))
)

(deffunction pddl-diagnosis-gen-call (?diag-id ?plan-id ?fault)
  "Call the Pddl diagnosis generation plugin to create pddl domain and problem file"
  (bind ?m
      (blackboard-create-msg "PddlDiagInterface::diag-gen" "GenerateMessage")
  )
  (blackboard-set-msg-field ?m "goal" ?fault)
  (blackboard-set-msg-field ?m "plan" ?plan-id)
  (blackboard-set-msg-field ?m "collection" "pddl.worldmodel")
  (bind ?gen-id (blackboard-send-msg ?m))
  (assert (diagnosis-gen (id ?diag-id) (plan-id ?plan-id) (gen-id ?gen-id)))
)

(deffunction create-diagnosis (?diag-id ?plan-id ?fault)
  (assert (diagnosis (id ?diag-id) (plan-id ?plan-id)
                (fault ?fault))
  )
  (pddl-diagnosis-gen-call ?diag-id ?plan-id ?fault)
)

(defrule diagnosis-generation-finished
  ?d <- (diagnosis (id ?diag-id) (plan-id ?plan-id) (fault ?fault) (mode CREATED))
  ?dg <- (diagnosis-gen (id ?diag-id) (plan-id ?plan-id) (gen-id ?gen-id))
  (PddlDiagInterface (id "diag-gen") (msg_id ?gen_id) (final TRUE) (success TRUE))
  =>
  (retract ?dg)
  (printout error "Finished generation of pddl-diagnosis files for " ?plan-id crlf)
  (modify ?d (mode GENERATED))
  (pddl-diagnosis-call ?diag-id ?fault)
)

(defrule diagnosis-generation-failed
  ?d <- (diagnosis (id ?diag-id) (plan-id ?plan-id) (mode CREATED))
  ?df <- (diagnosis-gen (id ?diag-id) (plan-id ?plan-id) (gen-id ?gen-id))
  (PddlDiagInterface (id "diag-gen") (msg_id ?gen_id) (final TRUE) (success FALSE))
  =>
  (retract ?df)
  (modify ?d (mode FAILED))
  (printout error "Failed to generate pddl-diagnosis files for " ?plan-id crlf)
)

(defrule diagnosis-create-diag-from-robmem
  "Fetch the resulting plan from robot memory and expand the goal."
  ?g <- (diagnosis (id ?diag-id) (mode GENERATED))
  ?t <- (robmem-trigger (name "new-plan") (ptr ?obj))
  ?p <- (pddl-plan
          (status PLANNED)
          (purpose-id ?diag-id)
        )
  =>
  (printout t "Fetched a new diagnosis!" crlf)
  (bind ?plan-id (bson-get (bson-get ?obj "o") "plan"))
  (bind ?cost (bson-get (bson-get ?obj "o") "cost"))
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
        (diagnosis-hypothesis (id ?plan-id) (diag-id ?diag-id) (cost ?cost))
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
  ; Give domain rules time to add param-names to actions
  (declare (salience -1))
  ?d <- (diagnosis (id ?diag-id) (mode GENERATED))
  (diagnosis-hypothesis (diag-id ?diag-id))
  (not (robmem-trigger (name "new-plan")))
  ?p <- (pddl-plan (status PLANNED) (purpose-id ?diag-id))
  =>
  (modify ?d (mode DIAGNOSIS-CREATED))
  (retract ?p)
)


