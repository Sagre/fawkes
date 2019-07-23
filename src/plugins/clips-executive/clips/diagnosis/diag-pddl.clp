
;---------------------------------------------------------------------------
;  diag-pddl.clp - CLIPS executive - diagnosis creation for given problem
;
;  Created: Sun Mar 09 15:47:23 2019
;  Copyright  2019 Daniel Habering
;  Licensed under GPLv2+ license, cf. LICENSE file
;---------------------------------------------------------------------------

; Creates diagnosis candidates for a given fault
; Makes use of the diag-planner plugin for generating multiple plans with kstar,
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

(deftemplate diag-plan
  ; The ID of the goal or diagnose.
  (slot purpose-id (type SYMBOL))
  ; The ID of PDDL generation message.
  (slot gen-id (type INTEGER))
  ; The ID of the PDDL Plan message.
  (slot planner-id (type INTEGER))
  ; The current status of this plan.
  (slot status (type SYMBOL)
    (allowed-values
      GEN-PENDING GEN-RUNNING GENERATED PENDING RUNNING PLANNED PLAN-FETCHED
    )
  )
  ; The PDDL formula to plan for.
  (slot goal (type STRING))
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
  (assert (diag-plan
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


(defrule diagnosis-check-if-planner-running
  "Check whether the planner started to plan."
  ?p <- (diag-plan (status PENDING) (planner-id ?planner-id))
  (PddlPlannerInterface (id "pddl-planner") (msg_id ?planner-id))
  =>
  (modify ?p (status RUNNING))
)

(defrule diagnosis-check-if-planner-finished
  "Check whether the planner finished planning."
  ?p <- (diag-plan (status RUNNING) (planner-id ?planner-id))
  (PddlPlannerInterface (id "pddl-planner") (msg_id ?planner-id) (final TRUE)
    (success TRUE))
  =>
  (modify ?p (status PLANNED))
)

(defrule diagnosis-check-if-planner-failed
  "Check whether the planner finished but has not found a plan."
  ?g <- (goal (id ?goal-id))
  ?p <- (diag-plan (status RUNNING) (purpose-id ?goal-id) (planner-id ?planner-id))
  (PddlPlannerInterface (id "pddl-planner") (msg_id ?planner-id) (final TRUE)
    (success FALSE))
  =>
  (printout error "Planning failed for goal " ?goal-id crlf)
  (modify ?g (mode FINISHED) (outcome FAILED) )
  (retract ?p)
)

(defrule diagnosis-generation-finished
  ?d <- (diagnosis (id ?diag-id) (plan-id ?plan-id) (fault ?fault) (mode CREATED))
  ?dg <- (diagnosis-gen (id ?diag-id) (plan-id ?plan-id) (gen-id ?gen-id))
  (PddlDiagInterface (id "diag-gen") (msg_id ?gen_id) (final TRUE) (success TRUE))
  =>
  (retract ?dg)
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
  ?p <- (diag-plan
       ;   (status PLANNED)
          (purpose-id ?diag-id)
        )
  =>
  (bind ?plan-id (bson-get (bson-get ?obj "fullDocument") "plan"))
  (bind ?cost (bson-get (bson-get ?obj "fullDocument") "cost"))
  (progn$ (?action (bson-get-array (bson-get ?obj "fullDocument") "actions"))
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
  ?p <- (diag-plan (status PLANNED) (purpose-id ?diag-id))
  =>
  (modify ?d (mode DIAGNOSIS-CREATED))
  (retract ?p)
)

(defrule diagnosis-filter-hypotheses-cost
  ; Remove all hypotheses with cost greater than minimum cost + 1
  (diagnosis (id ?diag-id) (mode DIAGNOSIS_CREATED))
  ?dh <- (diagnosis-hypothesis (id ?id) (cost ?cost) (diag-id ?diag-id))
  (diagnosis-hypothesis (id ?) (cost ?cost2&:(< (+ ?cost2 0) ?cost)) (diag-id ?diag-id))
  =>
  (printout t "Remove diag hypothesis " ?id " with cost " ?cost crlf)
  (do-for-all-facts ((?pa plan-action)) (eq ?pa:plan-id ?id)
    (retract ?pa)
  )
  (retract ?dh)
)

(defrule diagnosis-filter-hypotheses-duplicate
  ; Remove all hypotheses with cost greater than minimum cost + 1
  (diagnosis (id ?diag-id) (mode DIAGNOSIS-CREATED))
  ?dh <- (diagnosis-hypothesis (id ?id) (diag-id ?diag-id))
  (diagnosis-hypothesis (id ?id2&:(neq ?id ?id2)) (diag-id ?diag-id))
  (forall (plan-action (goal-id ?diag-id) (plan-id ?id) (id ?pa-id) (action-name ?an) (param-values $?pv))
          (plan-action (goal-id ?diag-id) (plan-id ?id2) (id ?pa-id) (action-name ?an) (param-values $?pv))
  )
  =>
  (printout t "Remove diag hypothesis " ?id " because its a duplcate of " ?id2 crlf)
  (do-for-all-facts ((?pa plan-action)) (eq ?pa:plan-id ?id)
    (retract ?pa)
  )
  (retract ?dh)
)

