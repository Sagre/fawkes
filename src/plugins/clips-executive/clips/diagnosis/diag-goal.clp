(defrule diag-goal-start-active-diagnosis 
    ?d <- (diagnosis (id ?diag-id) (mode DIAGNOSIS-CREATED))
    (not (goal (class ACTIVE-DIAGNOSIS) (params ?diag-id)))
    =>
    (printout t "Started Diagnosis Main Goal" crlf)
    (bind ?setup-ret (active-diagnosis-set-up (str-cat ?diag-id)))
    (if ?setup-ret then
        (assert (goal (class ACTIVE-DIAGNOSIS) (id (sym-cat ACTIVE-DIAGNOSIS- (gensym*)))
                    (params ?diag-id)
                    (sub-type RUN-ALL-OF-SUBGOALS)))
        (modify ?d (mode ACTIVE-DIAGNOSIS))
    else
        (printout error "Failed to setup diagnosis environment" crlf)
        (modify ?d (mode FAILED))
    )
    
)

(defrule diag-goal-select-diagnosis-goal
    ?g <- (goal (class ACTIVE-DIAGNOSIS) (mode FORMULATED) (parent nil))
    =>
    (printout t "Selected Diagnosis Main Goal" crlf)
    (modify ?g (mode SELECTED))
)

(defrule diag-goal-expand-diagnosis-goal
    ?g <- (goal (class ACTIVE-DIAGNOSIS) (id ?id) (mode SELECTED) (parent nil))
    =>
    (bind ?action (active-diagnosis-get-sensing-action))
    (printout t "Create first sensing subgoal: " ?action crlf)
    ; TODO: Get action name and param
    (assert 
        (goal (id (sym-cat ACTIVE-DIAGNOSIS-SENSE- (gensym*)))
                (class ACTIVE-DIAGNOSIS-SENSE)
                (parent ?id)
                (type ACHIEVE)
                (mode FORMULATED)
                (params ?action))
    )
    (modify ?g (mode EXPANDED))
)

(defrule diag-goal-evaluate-subgoal
    (goal (id ?parent-id) (class ACTIVE-DIAGNOSIS) (params ?diag-id))
    ?sg <- (goal (id ?id) (class ACTIVE-DIAGNOSIS-SENSE) (parent ?parent-id&~nil)
                (mode FINISHED)
                (outcome ?outcome))
    (plan-action (goal-id ?id) (state FINAL))
    ?d <- (diagnosis (id ?gen-id))
    =>
    ; TODO Get measurement result
    (active-diagnosis-integrate-measurement "test-fact" "TRUE")
    (bind ?final (active-diagnosis-final))
    (if (and (not ?final) (eq ?outcome COMPLETED)) then
        (bind ?action (active-diagnosis-get-sensing-action))
        (printout t "Next sensing subgoal: " ?action crlf)
        ; TODO get action name and params
        (assert (goal (id (sym-cat ACTIVE-DIAGNOSIS-SENSE- (gensym*)))
                        (class ACTIVE-DIAGNOSIS-SENSE)
                        (parent ?parent-id)
                        (type ACHIEVE)
                        (mode FORMULATED)
                        (params ?action))  
        )
    )
    (modify ?sg (mode EVALUATED))
)

(defrule diag-goal-evaluate-diag-goal
    ?g <- (goal (id ?parent-id) (class ACTIVE-DIAGNOSIS) (mode FINISHED) (outcome ?outcome) (params ?diag-id))
    ?d <- (diagnosis (id ?diag-id) (mode ACTIVE-DIAGNOSIS))
    =>

    (if (eq ?outcome COMPLETED) then
        (printout t "Diagnosis of " ?diag-id " finished" crlf)
        (modify ?d (mode FINAL))
    else
        (printout t "Diagnosis of " ?diag-id " failed" crlf)
        (modify ?d (mode FAILED))
    )
    (modify ?g (mode EVALUATED))
)

(defrule diag-goal-retract-diag-goal
    ?g <- (goal (id ?parent-id) (class ACTIVE-DIAGNOSIS) (mode EVALUATED))
    =>
    (printout t "Retract diagnosis goal" crlf)
    (modify ?g (mode RETRACTED))

)

(defrule diag-goal-clean-up
    ?g <- (goal (id ?parent-id) (class ACTIVE-DIAGNOSIS) (mode RETRACTED))
    =>
    (printout t "Clean Up Diagnosis Goals" crlf)
    (do-for-all-facts ((?sg goal)) (eq ?sg:parent ?parent-id)
        (do-for-all-facts ((?p plan)) (eq ?p:goal-id ?sg:id)
            (retract ?p)
        )
        (do-for-all-facts ((?pa plan-action)) (eq ?pa:goal-id ?sg:id)
            (retract ?pa)
        )
        (retract ?sg)
    )
    (retract ?g)
    (printout t "Done" crlf)
)

(defrule diag-goal-expand-sense-goal
    ?g <- (goal (id ?id) (class ACTIVE-DIAGNOSIS-SENSE) (mode SELECTED) (params ?action))
    =>
    (assert 
        (plan (id SENSING-PLAN) (goal-id ?id))
        (plan-action (id 1) (plan-id SENSING-PLAN) (goal-id ?id) (action-name (sym-cat ?action)))
    )
    (modify ?g (mode DISPATCHED))
)

(defrule test-action
    ?pa <- (plan-action (action-name test-action) (state PENDING))
    =>
    (modify ?pa (state FINAL))
)